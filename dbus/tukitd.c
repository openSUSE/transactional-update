#include "Bindings/libtukit.h"
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>
#include <unistd.h>
#include <wordexp.h>

#define _cleanup_(f) __attribute__((cleanup(f)))

enum transactionstates { queued, running, finished };

typedef struct t_entry {
    char* id;
    struct t_entry *next;
    enum transactionstates state;
} TransactionEntry;

struct call_args {
    char *transaction;
    char *command;
    int chrooted;
    enum transactionstates *state;
};
struct execute_args {
    struct tukit_tx *transaction;
    char *command;
    enum transactionstates *state;
    char *rebootmethod;
};

// Even though userdata / activeTransaction is shared between several threads, due to
// systemd's serial event loop processing it's always guaranteed that no parallel
// access will be happen: lockSnapshot will be called in the event functions before
// starting the new thread, and unlockSnapshot is triggered by the signal when a
// thread has finished.
// Any method which does write the variable must do so from the main event loop.
int lockSnapshot(void* userdata, const char* transaction, sd_bus_error *ret_error) {
    fprintf(stdout, "Locking further invocations for snapshot %s...\n", transaction);
    TransactionEntry* activeTransaction = userdata;
    TransactionEntry* newTransaction;
    while (activeTransaction->id != NULL) {
        if (strcmp(activeTransaction->id, transaction) == 0) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "The transaction is currently in use by another thread.");
            return -EBUSY;
        }
        activeTransaction = activeTransaction->next;
    }
    if ((newTransaction = malloc(sizeof(TransactionEntry))) == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Error while allocating space for transaction.");
        return -ENOMEM;
    }
    newTransaction->id = NULL;
    activeTransaction->id = strdup(transaction);
    if (activeTransaction->id == NULL) {
        free(newTransaction);
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Error during strdup.");
        return -ENOMEM;
    }
    activeTransaction->state = queued;
    activeTransaction->next = newTransaction;
    return 0;
}

void unlockSnapshot(void* userdata, const char* transaction) {
    TransactionEntry* activeTransaction = userdata;
    TransactionEntry* prevNext = NULL;

    while (activeTransaction->id != NULL) {
        // The entry point can't be changed, so just set the data of the to be deleted
        // transaction to the next entry's data (also for later matches to have just one code
        // path).
        if (strcmp(activeTransaction->id, transaction) == 0) {
            fprintf(stdout, "Unlocking snapshot %s...\n", transaction);
            free(activeTransaction->id);
            activeTransaction->id = activeTransaction->next->id;
            prevNext = activeTransaction->next;
            if (activeTransaction->id != NULL) {
                activeTransaction->next = activeTransaction->next->next;
            }
            free(prevNext);
            return;
        }
        activeTransaction = activeTransaction->next;
    }
}

sd_bus* get_bus() {
    sd_bus *bus = NULL;
    if (sd_bus_default_system(&bus) < 0) {
        // When opening a new bus connection fails there aren't a lot of options to present this
        // error to the user here. Maybe add some synchronization with the main thread and exit
        // on error in the future?
        fprintf(stderr, "Failed to connect to system bus.");
    }
    return bus;
}

int emit_internal_signal(sd_bus *bus, const char *transaction, const char* signame, const char *types, ...) {
    va_list ap;
    _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
    int ret;

    if ((ret = sd_bus_message_new_signal(bus, &m, "/org/opensuse/tukit/internal", "org.opensuse.tukit.internal", signame)) != 0) {
        fprintf(stderr, "Error during sd_bus_message_new_signal for %s (Transaction %s): %s\n", signame, transaction, strerror(ret));
    }
    va_start(ap, types);
    if (ret >= 0 && (ret = sd_bus_message_appendv(m, "sis", ap)) < 0) {
        fprintf(stderr, "Error during sd_bus_message_appendv for %s (Transaction %s): %s\n", signame, transaction, strerror(ret));
    }
    va_end(ap);
    if (ret >= 0 && (ret = sd_bus_send_to(bus, m, "org.opensuse.tukit", NULL)) < 0) {
        fprintf(stderr, "Error during sd_bus_send_to for %s (Transaction %s): %s\n", signame, transaction, strerror(ret));
    }

    if (ret < 0)
        return ret;
    else
        return 0;
}

int send_error_signal(sd_bus *bus, const char *transaction, const char *message, int error) {
    if (bus == NULL) {
        bus = get_bus();
    }
    int ret = emit_internal_signal(bus, transaction, "Error", "sis", transaction, error, message);
    if (ret < 0) {
        // Something is seriously broken when even an error message can't be sent any more...
        fprintf(stderr, "Cannot reach D-Bus any more: %s\n", strerror(ret));
    }
    sd_bus_flush_close_unref(bus);
    return ret;
}

static void *execute_func(void *args) {
    int ret = 0;
    int exec_ret = 0;
    wordexp_t p;

    struct execute_args* ea = (struct execute_args*)args;
    struct tukit_tx* tx = ea->transaction;
    char command[strlen(ea->command) + 1];
    strcpy(command, ea->command);
    char rebootmethod[strlen(ea->rebootmethod) + 1];
    strcpy(rebootmethod, ea->rebootmethod);


    enum transactionstates *state = ea->state;
    *state = running;

    // D-Bus doesn't support connection sharing between several threads, so a new dbus connection
    // has to be established. The bus will only be initialized directly before it is used to
    // avoid timeouts.
    sd_bus *bus = NULL;

    const char* transaction = tukit_tx_get_snapshot(tx);
    if (tx == NULL) {
        send_error_signal(bus, transaction, tukit_get_errmsg(), -1);
        goto finish_execute;
    }

    fprintf(stdout, "Executing command `%s` in snapshot %s...\n", command, transaction);

    ret = wordexp(command, &p, 0);
    if (ret != 0) {
        if (ret == WRDE_NOSPACE) {
            wordfree(&p);
        }
        send_error_signal(bus, transaction, "Command could not be processed.", ret);
        goto finish_execute;
    }

    const char* output;
    exec_ret = tukit_tx_execute(tx, p.we_wordv, &output);

    wordfree(&p);

    // Discard snapshot on error
    if (exec_ret != 0) {
        send_error_signal(bus, transaction, output, exec_ret);
        goto finish_execute;
    }

    if ((ret = tukit_tx_finalize(tx)) != 0) {
        send_error_signal(bus, transaction, tukit_get_errmsg(), -1);
        goto finish_execute;
    }

    bus = get_bus();

    ret = emit_internal_signal(bus, transaction, "CommandExecuted", "sis", transaction, exec_ret, output);
    if (ret < 0) {
        send_error_signal(bus, transaction, "Cannot send signal 'CommandExecuted'.", ret);
    }

    free((void*)output);

    if (strcmp(rebootmethod, "none") != 0) {
        if (tukit_reboot(rebootmethod) != 0){
            bus = get_bus();
            send_error_signal(bus, transaction, tukit_get_errmsg(), -1);
        }
    }

finish_execute:
    sd_bus_flush_close_unref(bus);
    tukit_free_tx(tx);
    free((void*)transaction);

    return (void*)(intptr_t) ret;
}

static int execute_common(sd_bus_message *m, void *userdata, sd_bus_error *ret_error, int reboot) {
    char *base;
    const char *snapid = NULL;
    char *rebootmethod = "none";
    pthread_t execute_thread;
    struct execute_args exec_args;
    TransactionEntry* activeTransaction = userdata;
    int ret = 0;

    if (reboot) {
        ret = sd_bus_message_read(m, "sss", &base, &exec_args.command, &rebootmethod);
    } else {
        ret = sd_bus_message_read(m, "ss", &base, &exec_args.command);
    }
    if (ret < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read execution parameters.");
        return -1;
    }
    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        return -1;
    }
    if ((ret = tukit_tx_init(tx, base)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        goto finish_execute;
    }
    if ((snapid = tukit_tx_get_snapshot(tx)) == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        ret = -1;
        goto finish_execute;
    }

    if ((ret = lockSnapshot(userdata, snapid, ret_error)) != 0) {
        goto finish_execute;
    }

    if ((ret = sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit.Transaction", "TransactionOpened", "s", snapid)) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Sending signal 'TransactionOpened' failed.");
        goto finish_execute;
    }

    fprintf(stdout, "Snapshot %s created.\n", snapid);

    while (activeTransaction->next->id != NULL) {
        activeTransaction = activeTransaction->next;
    }
    exec_args.transaction = tx;
    exec_args.state = &activeTransaction->state;
    exec_args.rebootmethod = rebootmethod;

    if ((ret = pthread_create(&execute_thread, NULL, execute_func, &exec_args)) != 0) {
        goto finish_execute;
    }

    while (activeTransaction->state != running) {
        usleep(500);
    }

    pthread_detach(execute_thread);

    ret = sd_bus_reply_method_return(m, "s", snapid);
    if (snapid != NULL) {
        free((void*)snapid);
    }
    return ret;

finish_execute:
    tukit_free_tx(tx);
    if (snapid != NULL) {
        free((void*)snapid);
    }

    return ret;
}

static int transaction_execute(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_common(m, userdata, ret_error, 0);
}

static int transaction_execute_reboot(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return execute_common(m, userdata, ret_error, 1);
}

static int transaction_open(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    char *base;
    const char *snapid;
    int ret = 0;

    if (sd_bus_message_read(m, "s", &base) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read base snapshot identifier.");
        return -1;
    }
    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        return -1;
    }
    if ((ret = tukit_tx_init(tx, base)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
    }
    if (!ret) {
        snapid = tukit_tx_get_snapshot(tx);
        if (snapid == NULL) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
            ret = -1;
        }
    }
    if (!ret && (ret = tukit_tx_keep(tx)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
    }

    tukit_free_tx(tx);
    if (ret) {
        return ret;
    }

    if (sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit", "org.opensuse.tukit.Transaction", "TransactionOpened", "s", snapid) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Sending signal 'TransactionOpened' failed.");
        return -1;
    }

    fprintf(stdout, "Snapshot %s created.\n", snapid);

    ret = sd_bus_reply_method_return(m, "s", snapid);
    free((void*)snapid);
    return ret;
}

static void *call_func(void *args) {
    int ret = 0;
    int exec_ret = 0;
    wordexp_t p;

    struct call_args* ea = (struct call_args*)args;
    char transaction[strlen(ea->transaction) + 1];
    strcpy(transaction, ea->transaction);
    char command[strlen(ea->command) + 1];
    strcpy(command, ea->command);
    int chrooted = ea->chrooted;

    enum transactionstates *state = ea->state;
    *state = running;

    fprintf(stdout, "Executing command `%s` in snapshot %s...\n", command, transaction);

    // D-Bus doesn't support connection sharing between several threads, so a new dbus connection
    // has to be established. The bus will only be initialized directly before it is used to
    // avoid timeouts.
    sd_bus *bus = NULL;

    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        send_error_signal(bus, transaction, tukit_get_errmsg(), -1);
        goto finish_execute;
    }
    ret = tukit_tx_resume(tx, transaction);
    if (ret != 0) {
        send_error_signal(bus, transaction, tukit_get_errmsg(), ret);
        goto finish_execute;
    }

    ret = wordexp(command, &p, 0);
    if (ret != 0) {
        if (ret == WRDE_NOSPACE) {
            wordfree(&p);
        }
        send_error_signal(bus, transaction, "Command could not be processed.", ret);
        goto finish_execute;
    }

    const char* output;
    if (chrooted) {
        exec_ret = tukit_tx_execute(tx, p.we_wordv, &output);
    } else {
        exec_ret = tukit_tx_call_ext(tx, p.we_wordv, &output);
    }

    wordfree(&p);

    ret = tukit_tx_keep(tx);
    if (ret != 0) {
        free((void*)output);
        send_error_signal(bus, transaction, tukit_get_errmsg(), -1);
        goto finish_execute;
    }

    bus = get_bus();
    ret = emit_internal_signal(bus, transaction, "CommandExecuted", "sis", transaction, exec_ret, output);
    if (ret < 0) {
        send_error_signal(bus, transaction, "Cannot send signal 'CommandExecuted'.", ret);
    }

    free((void*)output);

finish_execute:
    sd_bus_flush_close_unref(bus);
    tukit_free_tx(tx);

    return (void*)(intptr_t) ret;
}

static int call_common(sd_bus_message *m, void *userdata,
           sd_bus_error *ret_error, const int chrooted) {
    int ret;
    pthread_t execute_thread;
    struct call_args exec_args;
    TransactionEntry* activeTransaction = userdata;

    if (sd_bus_message_read(m, "ss", &exec_args.transaction, &exec_args.command) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read D-Bus parameters.");
        return -1;
    }

    ret = lockSnapshot(userdata, exec_args.transaction, ret_error);
    if (ret != 0) {
        return ret;
    }

    while (activeTransaction->next->id != NULL) {
        activeTransaction = activeTransaction->next;
    }
    exec_args.chrooted = chrooted;
    exec_args.state = &activeTransaction->state;

    if ((ret = pthread_create(&execute_thread, NULL, call_func, &exec_args)) != 0) {
        return ret;
    }

    while (activeTransaction->state != running) {
        usleep(500);
    }

    pthread_detach(execute_thread);

    return ret;
}

static int transaction_call(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int ret = call_common(m, userdata, ret_error, 1);
    if (ret)
        return ret;
    return sd_bus_reply_method_return(m, "");
}

static int transaction_callext(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int ret = call_common(m, userdata, ret_error, 0);
    if (ret)
        return ret;
    return sd_bus_reply_method_return(m, "");
}

static int signalCallback(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *member;
    char *transaction;
    int exec_ret = 0;
    const char* output;

    if ((member = sd_bus_message_get_member(m)) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read member name.");
        return -1;
    }
    if (strcmp(member, "CommandExecuted") == 0) {
        if (sd_bus_message_read(m, "sis", &transaction, &exec_ret, &output) < 0) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read CommandExecuted signal data.");
            return -1;
        }

        int ret = sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit/Transaction", "org.opensuse.tukit.Transaction", "CommandExecuted", "sis", transaction, exec_ret, output);
        if (ret < 0) {
            send_error_signal(sd_bus_message_get_bus(m), transaction, "Cannot send signal 'CommandExecuted'.", ret);
        }
        unlockSnapshot(userdata, transaction);
    } else if (strcmp(member, "Error") == 0) {
        if (sd_bus_message_read(m, "sis", &transaction, &exec_ret, &output) < 0) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read CommandExecuted signal data.");
            return -1;
        }

        int ret = sd_bus_emit_signal(sd_bus_message_get_bus(m), "/org/opensuse/tukit/Transaction", "org.opensuse.tukit.Transaction", "Error", "sis", transaction, exec_ret, output);
        if (ret < 0) {
            send_error_signal(sd_bus_message_get_bus(m), transaction, "Cannot send signal 'Error'.", ret);
        }
        unlockSnapshot(userdata, transaction);
    }
    return 0;
}

static int transaction_close(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    char *transaction;
    int ret = 0;

    if (sd_bus_message_read(m, "s", &transaction) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read D-Bus parameters.");
        return -1;
    }
    ret = lockSnapshot(userdata, transaction, ret_error);
    if (ret != 0) {
        return ret;
    }
    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        goto finish_close;
    }
    if ((ret = tukit_tx_resume(tx, transaction)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        goto finish_close;
    }
    if ((ret = tukit_tx_finalize(tx)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        goto finish_close;
    }

    fprintf(stdout, "Snapshot %s closed.\n", transaction);
    sd_bus_reply_method_return(m, "i", ret);

finish_close:
    tukit_free_tx(tx);
    unlockSnapshot(userdata, transaction);

    return ret;
}

static int transaction_abort(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    char *transaction;
    int ret = 0;

    if (sd_bus_message_read(m, "s", &transaction) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read D-Bus parameters.");
        return -1;
    }
    ret = lockSnapshot(userdata, transaction, ret_error);
    if (ret != 0) {
        return ret;
    }
    struct tukit_tx* tx = tukit_new_tx();
    if (tx == NULL) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
	goto finish_abort;
    }
    if ((ret = tukit_tx_resume(tx, transaction)) != 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", tukit_get_errmsg());
        goto finish_abort;
    }
    fprintf(stdout, "Snapshot %s aborted.\n", transaction);
    sd_bus_reply_method_return(m, "i", ret);

finish_abort:
    tukit_free_tx(tx);
    unlockSnapshot(userdata, transaction);

    return ret;
}

static int snapshot_list(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    char *columns;
    size_t list_len = 0;
    int columnnum = 1;

    if (sd_bus_message_read(m, "s", &columns) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Could not read D-Bus parameters.");
        return -1;
    }

    for (int i=0; columns[i]; i++)
        columnnum += (columns[i] == ',');

    struct tukit_sm_list* list = tukit_sm_get_list(&list_len, columns);

    sd_bus_message *message = NULL;
    if (sd_bus_message_new_method_return(m, &message) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Creating new return method failed.");
        return -1;
    }
    if (sd_bus_message_open_container(message, SD_BUS_TYPE_ARRAY, "as") < 0 ) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Creating container (array of snapshots) failed.");
        return -1;
    }
    for (int i=0; i < list_len; i++) {
        if (sd_bus_message_open_container(message, SD_BUS_TYPE_ARRAY, "s") < 0 ) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Creating container (array of snapshot data) failed.");
            return -1;
        }
        for (int j=0; j < columnnum; j++) {
            if (sd_bus_message_append(message, "s", tukit_sm_get_list_value(list, i, j)) < 0) {
                sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Couldn't append to message (container).");
                return -1;
            }
        }
        if (sd_bus_message_close_container(message) < 0) {
            sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Closing container (array of snapshot data) failed.");
            return -1;
        }
    }
    if (sd_bus_message_close_container(message) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Closing container (array of snapshot data) failed.");
        return -1;
    }
    if (sd_bus_send(sd_bus_message_get_bus(message), message, NULL) < 0) {
        sd_bus_error_set_const(ret_error, "org.opensuse.tukit.Error", "Sending message failed.");
        return -1;
    }
    sd_bus_message_unref(message);
    tukit_free_sm_list(list);
    return 0;
}

int event_handler(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
    TransactionEntry* activeTransaction = userdata;
    if (activeTransaction->id != NULL) {
        fprintf(stdout, "Waiting for remaining transactions to finish...\n");
        sleep(1);
        kill(si->ssi_pid, si->ssi_signo);
        // TODO: New requests should probably be rejected from here, but unlocking is an event itself...
    } else {
        fprintf(stdout, "Terminating.\n");
        int ret;
        if ((ret = sd_event_exit(sd_event_source_get_event(s), 0)) < 0) {
            fprintf(stderr, "Cannot exit the main loop! %s\n", strerror(-ret));
            exit(1);
        }
    }
    return 0;
}

static const sd_bus_vtable tukit_transaction_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("Execute", SD_BUS_ARGS("s", base, "s", command), SD_BUS_RESULT("s", snapshot), transaction_execute, 0),
    SD_BUS_METHOD_WITH_ARGS("ExecuteAndReboot", SD_BUS_ARGS("s", base, "s", command, "s", rebootmethod), SD_BUS_RESULT("s", snapshot), transaction_execute_reboot, 0),
    SD_BUS_METHOD_WITH_ARGS("Open", SD_BUS_ARGS("s", base), SD_BUS_RESULT("s", snapshot), transaction_open, 0),
    SD_BUS_METHOD_WITH_ARGS("Call", SD_BUS_ARGS("s", transaction, "s", command), SD_BUS_NO_RESULT, transaction_call, 0),
    SD_BUS_METHOD_WITH_ARGS("CallExt", SD_BUS_ARGS("s", transaction, "s", command), SD_BUS_NO_RESULT, transaction_callext, 0),
    SD_BUS_METHOD_WITH_ARGS("Close", SD_BUS_ARGS("s", transaction), SD_BUS_RESULT("i", ret), transaction_close, 0),
    SD_BUS_METHOD_WITH_ARGS("Abort", SD_BUS_ARGS("s", transaction), SD_BUS_RESULT("i", ret), transaction_abort, 0),
    SD_BUS_SIGNAL_WITH_ARGS("TransactionOpened", SD_BUS_ARGS("s", snapshot), 0),
    SD_BUS_SIGNAL_WITH_ARGS("CommandExecuted", SD_BUS_ARGS("s", snapshot, "i", returncode, "s", output), 0),
    SD_BUS_SIGNAL_WITH_ARGS("Error", SD_BUS_ARGS("s", snapshot, "i", returncode, "s", output), 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable tukit_snapshot_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("List", SD_BUS_ARGS("s", columns), SD_BUS_RESULT("aas", list), snapshot_list, 0),
    SD_BUS_VTABLE_END
};

int main() {
    fprintf(stdout, "Started tukitd %s\n", VERSION);

    sd_bus_slot *slot_tx = NULL;
    sd_bus_slot *slot_snap = NULL;
    sd_bus *bus = NULL;
    sd_event *event = NULL;
    int ret = 1;

    TransactionEntry* activeTransactions = (TransactionEntry*) malloc(sizeof(TransactionEntry));
    if (activeTransactions == NULL) {
        fprintf(stderr, "malloc failed for TransactionEntry.\n");
        goto finish;
    }
    activeTransactions->id = NULL;
    activeTransactions->state = queued;

    ret = sd_bus_open_system(&bus);
    if (ret < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-ret));
        goto finish;
    }

    ret = sd_bus_add_object_vtable(bus,
                                   &slot_tx,
                                   "/org/opensuse/tukit/Transaction",
                                   "org.opensuse.tukit.Transaction",
                                   tukit_transaction_vtable,
                                   activeTransactions);
    if (ret < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", strerror(-ret));
        goto finish;
    }

    ret = sd_bus_add_object_vtable(bus,
                                   &slot_snap,
                                   "/org/opensuse/tukit/Snapshot",
                                   "org.opensuse.tukit.Snapshot",
                                   tukit_snapshot_vtable,
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

    ret = sd_bus_match_signal(bus,
                NULL,
                NULL,
                "/org/opensuse/tukit/internal",
                NULL,
                NULL,
                signalCallback,
                activeTransactions);
    if (ret < 0) {
        fprintf(stderr, "Failed to register internal signal listener: %s\n", strerror(-ret));
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
    ret = sd_event_add_signal(event, NULL, SIGTERM, event_handler, activeTransactions);
    if (ret < 0) {
        fprintf(stderr, "Could not add signal handler for SIGTERM to event loop: %s\n", strerror(-ret));
        goto finish;
    }
    ret = sd_event_add_signal(event, NULL, SIGINT, event_handler, activeTransactions);
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
    while (activeTransactions && activeTransactions->id != NULL) {
        TransactionEntry* nextTransaction = activeTransactions->next;
        free(activeTransactions->id);
        free(activeTransactions);
        activeTransactions = nextTransaction;
    }
    free(activeTransactions);
    sd_event_unref(event);
    sd_bus_slot_unref(slot_tx);
    sd_bus_slot_unref(slot_snap);
    sd_bus_unref(bus);

    return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
