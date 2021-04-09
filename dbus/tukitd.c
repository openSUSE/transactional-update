#include "Bindings/libtukit.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <unistd.h>
#include <wordexp.h>

static volatile int execution_finished = 0;
#define WORDEXP_ERROR 1000

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

struct execution_args {
    struct tukit_tx* tx;
    char *command;
};

static void* execution_func(void *args) {
    int ret;
    wordexp_t p;

    ret = wordexp(((struct execution_args*)args)->command, &p, 0);
    if (ret != 0) {
        if (ret == WRDE_NOSPACE) {
            wordfree(&p);
        }
        ret = WORDEXP_ERROR;
    } else {
        ret = tukit_tx_execute(((struct execution_args*)args)->tx, p.we_wordv);
        wordfree(&p);
    }

    execution_finished = 1;

    return (void*)(intptr_t) ret;
}

static int method_call(sd_bus_message *m, [[maybe_unused]] void *userdata, sd_bus_error *ret_error) {
    int ret;
    char *transaction;
    int pipefd[2];
    int stdout_orig = 0, stderr_orig = 0;
    struct execution_args exec_args;
    char *output = NULL;

    if (sd_bus_message_read(m, "ss", &transaction, &exec_args.command) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Could not read D-Bus parameters.");
        return -1;
    }
    exec_args.tx = tukit_new_tx();
    if (exec_args.tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
        return -1;
    }
    if (tukit_tx_resume(exec_args.tx, transaction) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", tukit_get_errmsg());
        goto finish_call;
    }

    // Redirect output to be able to return it to the caller
    stdout_orig = dup(1);
    if (stdout_orig < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Couldn't store old stdout file descriptor.");
        goto finish_call;
    }
    stderr_orig = dup(2);
    if (stderr_orig < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Couldn't store old stderr file descriptor.");
        goto finish_call;
    }
    if (pipe(pipefd) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Error opening pipe for command output.");
        goto finish_call;
    }
    if (dup2(pipefd[1], 1) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Couldn't set pipe for stdout.");
        goto finish_call;
    }
    if (dup2(pipefd[1], 2) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.error", "Couldn't set pipe for stderr.");
        goto finish_call;
    }

    // Async from here
    pthread_t execute_thread;
    void *res;
    ret = pthread_create(&execute_thread, NULL, execution_func, &exec_args);
    if (ret != 0) {
        sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, ret, "Failed creating a new thread to run command.");
        goto finish_call;
    }

    char buffer[2048];
    int allocated = 2049;
    int len;
    int total = 0;

    output = malloc(allocated);
    if (output == NULL) {
        sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, -1, "Failed allocating memory for output buffer.");
        goto finish_call;
    }
    len = fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    if (len < 0) {
        sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, len, "Failed setting non-blocking pipe.");
        goto finish_call;
    }
    while (execution_finished < 2) {
        if (execution_finished == 1)
            execution_finished = 2;
        while((len = read(pipefd[0], buffer, 2048)) > 0) {
            total = total + len;
            if (total >= allocated) {
                output = realloc(output, (allocated * 2) - 1);
                if (output == NULL) {
                    ret = -1;
                }
                allocated = (allocated * 2) - 1;
            }
            strncpy(&output[total - len], buffer, len);
        }
        if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, len, "Error reading pipe.");
            goto finish_call;
        }
        usleep(500000);
    }
    output[total] = '\0';
    // Don't abort while reading the pipe already, it may block if full otherwise
    if (ret != 0) {
        sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, -1, "Failed re-allocating memory for output buffer.");
        goto finish_call;
    }

    pthread_join(execute_thread, &res);
    ret = (intptr_t) res;
    if (ret == WORDEXP_ERROR) {
        sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, -1, "Error parsing command.");
        goto finish_call;
    }

    if (tukit_tx_keep(exec_args.tx) != 0) {
        sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandError", "sis", transaction, -1, tukit_get_errmsg());
        goto finish_call;
    }

    sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit", "CommandExecuted", "sis", transaction, ret, output);

finish_call:
    if (output != NULL)
        free(output);

    pthread_join(execute_thread, NULL);

    if (stdout_orig != 0) {
        dup2(stdout_orig, 1);
        close(stdout_orig);
    }
    if (stderr_orig != 0) {
        dup2(stderr_orig, 2);
        close(stderr_orig);
    }

    tukit_free_tx(exec_args.tx);
    if (ret) {
        //return ret;
    }
    execution_finished = 0;
    return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable tukit_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("open", SD_BUS_ARGS("s", base), SD_BUS_NO_RESULT, method_open, 0),
    SD_BUS_METHOD_WITH_ARGS("call", SD_BUS_ARGS("s", transaction, "s", command), SD_BUS_NO_RESULT, method_call, 0),
    SD_BUS_SIGNAL_WITH_ARGS("TransactionOpened", SD_BUS_ARGS("s", snapshot), 0),
    SD_BUS_SIGNAL_WITH_ARGS("CommandExecuted", SD_BUS_ARGS("s", snapshot, "i", returncode, "s", output), 0),
    SD_BUS_SIGNAL_WITH_ARGS("CommandError", SD_BUS_ARGS("s", snapshot, "i", returncode, "s", message), 0),
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
