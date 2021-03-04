#include "Bindings/libtukit.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <wordexp.h>

int exec(const char *cmd, char **output) {
    int rc;

    printf ("Executing `%s`.\n", cmd);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "popen() failed!\n");
        return -1;
    }

    // Does the caller want to see the output?
    if (output != NULL) {
        int buffincr = 1;
        char buffer[BUFSIZ];
        while (!feof(pipe)) {
            if (fgets(buffer, BUFSIZ, pipe) != NULL) {
                char* newmem = realloc(*output, buffincr * BUFSIZ);
                if (newmem == NULL) {
                    fprintf(stderr, "realloc() failed!\n");
                    return -1;
                }
                *output = newmem;
                *output = strcat(*output, buffer);
                buffincr++;
            }
        }
    }

    rc = pclose(pipe);
    return rc;
}

static int method_open(sd_bus_message *m, [[maybe_unused]] void *userdata, sd_bus_error *ret_error) {
    char *base;
    const char *snapid;
    int rc = 0;

    if (sd_bus_message_read(m, "s", &base) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Could not read base snapshot identifier.");
        return -1;
    }
    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
        return -1;
    }
    if ((rc = tukit_tx_init(tx, base)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
    }
    if (!rc) {
        snapid = tukit_tx_get_snapshot(tx);
        if (snapid == NULL) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
            rc = -1;
        }
    }
    if (!rc && (rc = tukit_tx_keep(tx)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
    }

    tukit_free_tx(tx);
    if (rc) {
        return rc;
    }

    if (sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "TransactionOpened", "s", snapid) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Sending signal 'TransactionOpened' failed.");
        return -1;
    }
    return sd_bus_reply_method_return(m, "");
}

static int method_call(sd_bus_message *m, [[maybe_unused]] void *userdata, sd_bus_error *ret_error) {
    char *transaction;
    char *command;
    int rc = 0;

    if (sd_bus_message_read(m, "ss", &transaction, &command) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Could not read D-Bus parameters.");
        return -1;
    }
    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
        return -1;
    }
    if ((rc = tukit_tx_resume(tx, transaction)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
    }
    wordexp_t p;
    if (!rc && (rc = wordexp(command, &p, 0)) != 0) {
        if (rc == WRDE_NOSPACE) {
            wordfree(&p);
        }
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Command could not be processed.");
    }
    if (!rc) {
        if ((rc = tukit_tx_execute(tx, p.we_wordv)) != 0) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
        }
        wordfree(&p);
    }
    if (!rc && (rc = tukit_tx_keep(tx)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
    }

    tukit_free_tx(tx);
    if (rc) {
        return rc;
    }

    if (sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandExecuted", "s", transaction, "i", 0) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Sending signal 'CommandExecuted' failed.");
        return -1;
    }
    return sd_bus_reply_method_return(m, ""); //Muss nach vorne, um nicht zu blockieren
}

static const sd_bus_vtable tukit_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("open", SD_BUS_ARGS("s", base), SD_BUS_NO_RESULT, method_open, 0),
    SD_BUS_METHOD_WITH_ARGS("call", SD_BUS_ARGS("s", transaction, "s", command), SD_BUS_NO_RESULT, method_call, 0),
    SD_BUS_SIGNAL_WITH_ARGS("TransactionOpened", SD_BUS_ARGS("s", snapshot), 0),
    SD_BUS_SIGNAL_WITH_ARGS("CommandExecuted", SD_BUS_ARGS("s", snapshot, "i", returncode), 0),
    SD_BUS_VTABLE_END
};

int main() {
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    sd_event *event = NULL;
    int ret;

    ret = sd_bus_open_system(&bus);
    if (ret < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-ret));
        goto finish;
    }

    ret = sd_bus_add_object_vtable(bus,
                                   &slot, // Muss vermutlich non-floating sein
                                   "/org/opensuse/tukit",
                                   "org.opensuse.tukit",
                                   tukit_vtable,
                                   NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", strerror(-ret));
        goto finish;
    }

    /* Take a well-known service name so that clients can find us */
    ret = sd_bus_request_name(bus, "org.opensuse.tukit", 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-ret));
        goto finish;
    }

    ret = sd_event_default(&event);
    if (ret < 0) {
        fprintf(stderr, "Failed to create default event loop: %s\n", strerror(-ret));
        goto finish;
    }
    sigset_t ss;
    if (sigemptyset(&ss) < 0 || sigaddset(&ss, SIGTERM) < 0 || sigaddset(&ss, SIGINT) < 0) {
        fprintf(stderr, "Failed to set the signal set: %s\n", strerror(-ret));
        goto finish;
    }
    /* Block SIGTERM first, so that the event loop can handle it */
    if (sigprocmask(SIG_BLOCK, &ss, NULL) < 0) {
        fprintf(stderr, "Failed to block the signals: %s\n", strerror(-ret));
        goto finish;
    }
    /* Let's make use of the default handler and "floating" reference features of sd_event_add_signal() */
    ret = sd_event_add_signal(event, NULL, SIGTERM, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not add signal handler for SIGTERM to event loop: %s\n", strerror(-ret));
        goto finish;
    }
    ret = sd_event_add_signal(event, NULL, SIGINT, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not add signal handler for SIGINT to event loop: %s\n", strerror(-ret));
        goto finish;
    }
    ret = sd_bus_attach_event(bus, event, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not add sd-bus handling to event bus: %s\n", strerror(-ret));
        goto finish;
    }

    ret = sd_event_loop(event);
    if (ret < 0) {
        fprintf(stderr, "Error while running event loop: %s\n", strerror(-ret));
        goto finish;
    }

finish:
    sd_event_unref(event);
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);

    return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
