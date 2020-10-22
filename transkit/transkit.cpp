/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#include "transkit.h"
#include "../lib/Configuration.h"
#include "../lib/Transaction.h"
#include "../lib/Log.h"
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <typeinfo>

using namespace std;
using TransactionalUpdate::config;

void Transkit::displayHelp() {
    cout << "Syntax: transkit [option...] command\n";
    cout << "\n";
    cout << "Manage transactions ...\n";
    cout << "\n";
    cout << "Commands:\n";
    cout << "execute <command>\n";
    cout << "\tOpens a new snapshot and executes the given command; on success the snapshot\n";
    cout << "\twill be set as the new default snapshot, any non-zero return value will\n";
    cout << "\tdelete the snapshot again.\n";
    cout << "\tIf no command is given an interactive shell will be opened.\n";
    cout << "open\n";
    cout << "\tCreates a new transaction and prints its unique ID\n";
    cout << "call <ID> <command>\n";
    cout << "\tExecutes the given command, resuming the transaction with the given ID; returns\n";
    cout << "\tthe exit status of the given command, but will not delete the snapshot in case\n";
    cout << "\tof errors\n";
    cout << "close <ID>\n";
    cout << "\tCloses the given transaction and sets the snapshot as the new default snapshot\n";
    cout << "abort <ID>\n";
    cout << "\tDeletes the given snapshot again\n";
    cout << "Options:\n";
    cout << "--continue [<number>], -c  Use latest or given snapshot as base\n";
    cout << "--help, -h                 Display this help and exit\n";
    cout << "--version                  Display version and exit\n" << endl;
}

int Transkit::parseOptions(int argc, const char *argv[]) {
    int waitForSnapNum = false;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "execute") {
            TransactionalUpdate::Transaction transaction{};
            transaction.init(baseSnapshot);
            int status = transaction.execute(&argv[i + 1]); // All remaining arguments
            if (status == 0) {
                transaction.finalize();
            } else {
                throw runtime_error{"Application returned with exit status " + to_string(status)};
            }
            return 0;
        }
        else if (arg == "open") {
            TransactionalUpdate::Transaction transaction{};
            transaction.init(baseSnapshot);
            cout << "UUID: " << transaction.getSnapshot() << endl;
            transaction.keep();
            return 0;
        }
        else if (arg == "call") {
            if (argc < i + 1) {
                getHelp();
                throw invalid_argument{"Missing argument for 'call'"};
            }
            TransactionalUpdate::Transaction transaction{};
            transaction.resume(argv[i + 1]);
            int status = transaction.execute(&argv[i + 2]); // All remaining arguments
            transaction.keep();
            return status;
        }
        else if (arg == "close") {
            TransactionalUpdate::Transaction transaction{};
            transaction.resume(argv[i + 1]);
            transaction.finalize();
            return 0;
        }
        else if (arg == "abort") {
            TransactionalUpdate::Transaction transaction{};
            transaction.resume(argv[i + 1]);
            return 0;
        }
        else if (arg == "--continue" || arg == "-c") {
            waitForSnapNum = true;
            baseSnapshot = "default";
        }
        else if (arg == "--help" || arg == "-h" ) {
            displayHelp();
            return 0;
        }
        else {
            if (waitForSnapNum) {
                baseSnapshot = arg;
                waitForSnapNum = false;
            } else {
                displayHelp();
                throw invalid_argument{"Unknown command or option '" + arg + "'."};
            }
        }
    }
    cout << "Programmende" << endl;
    return 0;
}

class Lock {
public:
    Lock(){
        lockfile = open(config.get("LOCKFILE").c_str(), O_CREAT|O_WRONLY);
        if (lockfile < 0) {
            throw runtime_error{"Could not create lock file '" + config.get("LOCKFILE") + "': " + strerror(errno)};
        }
        int status = lockf(lockfile, F_TLOCK, (off_t)10000);
        if (status) {
            throw runtime_error{"Another instance of transkit is already running: " + string(strerror(errno))};
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

Transkit::Transkit(int argc, const char *argv[]) {
    Lock lock;
    cout << "transkit @VERSION@ started" << endl;
    tulog.level = TULogLevel::DEBUG;

    cout << "Options: ";
    for(int i = 1; i < argc; ++i)
        cout << argv[i] << " ";
    cout << endl;

    int ret = parseOptions(argc, argv);
    if (ret != 0) {
        throw ret;
    }
}
