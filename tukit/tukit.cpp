/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#include "tukit.hpp"
#include "Configuration.hpp"
#include "SnapshotManager.hpp"
#include "Transaction.hpp"
#include "Reboot.hpp"
#include "Log.hpp"
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <iostream>

using namespace std;
using TransactionalUpdate::config;

bool cancel;

void TUKit::displayHelp() {
    cout << "Syntax: tukit [option...] command\n";
    cout << "\n";
    cout << "Transaction and snapshot management utility\n";
    cout << "\n";
    cout << "Transaction Commands:\n";
    cout << "execute <command>\n";
    cout << "\tOpens a new snapshot and executes the given command; on success the snapshot\n";
    cout << "\twill be set as the new default snapshot, any non-zero return value will\n";
    cout << "\tdelete the snapshot again.\n";
    cout << "\tIf no command is given an interactive shell will be opened.\n";
    cout << "open\n";
    cout << "\tCreates a new transaction and prints its unique ID\n";
    cout << "call <ID> <command>\n";
    cout << "\tExecutes the given command from within the transaction's chroot environment,\n";
    cout << "\tresuming the transaction with the given ID; returns the exit status of the\n";
    cout << "\tgiven command, but will not delete the snapshot in case of errors\n";
    cout << "callext <ID> <command>\n";
    cout << "\tExecutes the given command. The command is not executed in a chroot\n";
    cout << "\tenvironment, but instead runs in the current system, replacing '{}' with the\n";
    cout << "\tmount directory of the given snapshot; returns the exit status of the given\n";
    cout << "\tcommand, but will not delete the snapshot in case of errors\n";
    cout << "close <ID>\n";
    cout << "\tCloses the given transaction and sets the snapshot as the new default snapshot\n";
    cout << "abort <ID>\n";
    cout << "\tDeletes the given snapshot again\n";
    cout << "rollback <ID>\n";
    cout << "\tRoll back to given snapshot\n";
    cout << "\n";
    cout << "Transaction Options:\n";
    cout << "--continue[=<ID>], -c[<ID>]  Use latest or given snapshot as base\n";
    cout << "--description=<description>  Use custom snapshot description for \"open\"\n";
    cout << "--keep, -k                   Keep snapshot even if there is an error\n";
    cout << "--discard, -d                Discard snapshot if no files were changed in root\n";
    cout << "\n";
    cout << "Snapshot Commands:\n";
    cout << "snapshots\n";
    cout << "\tPrints a list of all available transactions\n";
    cout << "\n";
    cout << "Snapshot Options:\n"; //TODO: Migrate to options of command
    cout << "--fields=<default,active,number,date,description>, -f<...>\n";
    cout << "                             List of fields to print\n";
    cout << "\n";
    cout << "Reboot Commands:\n";
    cout << "reboot [auto|rebootmgr|notify|systemd|kured|kexec|none]\n";
    cout << "\tReboot the system using the given method; Default: auto\n";
    cout << "\n";
    cout << "Generic Options:\n";
    cout << "--help, -h                   Display this help and exit\n";
    cout << "--log=<console,syslog>[,<...>,...], -l<...>\n";
    cout << "                             Restrict output channels to the given ones\n";
    cout << "--manager=<auto,snapper,podman>, -m<...>\n";
    cout << "                             Use specific snapshot manager\n";
    cout << "--quiet, -q                  Decrease verbosity\n";
    cout << "--verbose, -v                Increase verbosity\n";
    cout << "--version, -V                Display version and exit\n" << endl;
}

int TUKit::parseOptions(int argc, char *argv[]) {
    static const char optstring[] = "+c::dkf:hl:m:qvV";
    static const struct option longopts[] = {
        { "continue", optional_argument, nullptr, 'c' },
        { "description", required_argument, nullptr, 0 },
        { "keep", no_argument, nullptr, 'k' },
        { "discard", no_argument, nullptr, 'd' },
        { "fields", required_argument, nullptr, 'f' },
        { "help", no_argument, nullptr, 'h' },
        { "log", required_argument, nullptr, 'l' },
        { "manager", required_argument, nullptr, 'm' },
        { "quiet", no_argument, nullptr, 'q' },
        { "verbose", no_argument, nullptr, 'v' },
        { "version", no_argument, nullptr, 'V' },
        { 0, 0, 0, 0 }
    };

    int c;
    int lopt_idx;

    while ((c = getopt_long(argc, argv, optstring, longopts, &lopt_idx)) != -1) {
        switch (c) {
        case 0:
            description = optarg;
            break;
        case 'c':
            if (optarg)
                baseSnapshot = optarg;
            else
                baseSnapshot = "default";
            break;
        case 'k':
            keepSnapshot = true;
            break;
        case 'd':
            discardSnapshot = true;
            break;
        case 'f':
            fields = optarg;
            break;
        case 'h':
            displayHelp();
            return 0;
        case 'l':
            tulog.setLogOutput(optarg);
            break;
        case 'm':
            manager = optarg;
            break;
        case 'q':
            tulog.level = TULogLevel::Error;
            break;
        case 'v':
            tulog.level = TULogLevel::Debug;
            break;
        case 'V':
            cout << VERSION << endl;
            return 0;
        case '?':
            displayHelp();
            return -1;
        }
    }

    return optind;
}

int TUKit::processCommand(char *argv[]) {
    if (argv[0] == nullptr) {
        throw invalid_argument{"Missing command. See --help for usage information."};
    }
    string arg = argv[0];
    if (arg == "execute") {
        TransactionalUpdate::Transaction transaction{manager};
        if (keepSnapshot) {
            transaction.setKeepIfError(true);
        }
        if (discardSnapshot) {
            transaction.setDiscardIfUnchanged(true);
        }
        transaction.init(baseSnapshot);
        int status = transaction.execute(&argv[1]); // All remaining arguments
        if (status == 0) {
            transaction.finalize();
        } else {
            throw runtime_error{"Application returned with exit status " + to_string(status)};
        }
        return 0;
    }
    else if (arg == "open") {
        TransactionalUpdate::Transaction transaction{manager};
        if (keepSnapshot) {
            transaction.setKeepIfError(true);
        }
        if (discardSnapshot) {
            transaction.setDiscardIfUnchanged(true);
        }
        transaction.init(baseSnapshot, description);
        cout << "ID: " << transaction.getSnapshot() << endl;
        transaction.keep();
        return 0;
    }
    else if (arg == "call") {
        if (argv[1] == nullptr) {
            displayHelp();
            throw invalid_argument{"Missing argument for 'call'"};
        }
        TransactionalUpdate::Transaction transaction{manager};
        if (keepSnapshot) {
            transaction.setKeepIfError(true);
        }
        transaction.resume(argv[1]);
        int status = transaction.execute(&argv[2]); // All remaining arguments
        transaction.keep();
        return status;
    }
    else if (arg == "callext") {
        if (argv[1] == nullptr) {
            displayHelp();
            throw invalid_argument{"Missing argument for 'callext'"};
        }
        TransactionalUpdate::Transaction transaction{manager};
        if (keepSnapshot) {
            transaction.setKeepIfError(true);
        }
        transaction.resume(argv[1]);
        int status = transaction.callExt(&argv[2]); // All remaining arguments
        transaction.keep();
        return status;
    }
    else if (arg == "close") {
        if (argv[1] == nullptr) {
            displayHelp();
            throw invalid_argument{"Missing argument for 'close'"};
        }
        TransactionalUpdate::Transaction transaction{manager};
        if (keepSnapshot) {
            transaction.setKeepIfError(true);
        }
        transaction.resume(argv[1]);
        transaction.finalize();
        return 0;
    }
    else if (arg == "abort") {
        if (argv[1] == nullptr) {
            displayHelp();
            throw invalid_argument{"Missing argument for 'abort'"};
        }
        TransactionalUpdate::Transaction transaction{manager};
        if (keepSnapshot) {
            transaction.setKeepIfError(true);
        }
        transaction.resume(argv[1]);
        return 0;
    }
    else if (arg == "rollback") {
        if (argv[1] == nullptr) {
            displayHelp();
            throw invalid_argument{"Missing argument for 'rollback'"};
        }
        unique_ptr<TransactionalUpdate::SnapshotManager> snapshotMgr = TransactionalUpdate::SnapshotFactory::get(manager);
        snapshotMgr->rollbackTo(argv[1]);
        return 0;
    }
    else if (arg == "snapshots") {
        unique_ptr<TransactionalUpdate::SnapshotManager> snapshotMgr = TransactionalUpdate::SnapshotFactory::get(manager);
        if (fields.empty()) {
            fields = "number";
        }
        for(auto value: snapshotMgr->getList(fields)) {
            stringstream fieldsStream(fields);
            for (string field; getline(fieldsStream, field, ','); ) {
                cout << value[field] << "\t";
            }
            cout << endl;
        }
        return 0;
    }
    else if (arg == "reboot") {
        string method;
        if (argv[1]) {
            method = argv[1];
        } else {
            method = "auto";
        }
        TransactionalUpdate::Reboot rebootmgr{method};
        rebootmgr.reboot();

        return 0;
    }
    else {
        displayHelp();
        throw invalid_argument{"Unknown command or option '" + arg + "'."};
    }
}

class Lock {
public:
    Lock(){
        lockfile = open(config.get("LOCKFILE").c_str(), O_CREAT|O_WRONLY|O_CLOEXEC, 0600);
        if (lockfile < 0) {
            throw runtime_error{"Could not create lock file '" + config.get("LOCKFILE") + "': " + strerror(errno)};
        }
        int status = lockf(lockfile, F_TLOCK, (off_t)10000);
        if (status) {
            throw runtime_error{"Another instance of tukit is already running: " + string(strerror(errno))};
            remove(config.get("LOCKFILE").c_str());
        }
    }
    ~Lock() {
        close(lockfile);
        remove(config.get("LOCKFILE").c_str());
    }
private:
    int lockfile;
};

void interrupt(int signal) {
    //Nothing to do here - the child has been signalled already as it's part of the same
    // progress group. Maybe it may be worth killing the process when receiving multiple
    // interrupts?
    tulog.debug("tukit: Received signal ", signal);
}

TUKit::TUKit(int argc, char *argv[]) {
    signal(SIGINT, interrupt);
    signal(SIGHUP, interrupt);
    signal(SIGQUIT, interrupt);
    signal(SIGTERM, interrupt);

    tulog.level = TULogLevel::Info;

    int ret = parseOptions(argc, argv);
    if (ret <= 0) {
        throw ret;
    }

    Lock lock;
    tulog.info("tukit ", VERSION, " started");

    string optionsline = "Options: ";
    for(int i = 1; i < argc; ++i)
        optionsline.append(argv[i]).append(" ");
    tulog.info(optionsline);

    ret = processCommand(&argv[ret]);
    if (ret != 0) {
        throw ret;
    }

    tulog.info("Transaction completed.");
}
