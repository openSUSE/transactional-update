/*
  transactional-update - apply updates to the system in an atomic way

  Copyright (c) 2016 - 2020 SUSE LLC

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include <memory>
#include <stdexcept>
#include <typeinfo>

void Transkit::getHelp() {
    cout << "Syntax: transkit [option...] command" << endl;
    cout << "" << endl;
    cout << "Manage transactions ..." << endl;
    cout << "" << endl;
    cout << "Commands:" << endl;
    cout << "execute <command>" << endl;
    cout << "\tOpens a new snapshot and executes the command; will not delete the" << endl;
    cout << "\tsnapshot if the operation was not successful" << endl;
    cout << "\tIf no command is given an interactive shell will be opened." << endl;
    cout << "Options:" << endl;
    cout << "--continue [<number>], -c  Use latest or given snapshot as base" << endl;
    cout << "--help, -h                 Display this help and exit" << endl;
    cout << "--version                  Display version and exit" << endl << endl;
}

int Transkit::parseOptions(int argc, const char *argv[]) {
    int waitForSnapNum = false;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "execute") {
            Transaction transaction{};
            transaction.init(baseSnapshot);
            int status = transaction.execute(&argv[i + 1]); // All remaining arguments
            if (status == 0) {
                transaction.finalize();
            } else {
                throw runtime_error{"Application returned with exit status " + to_string(status)};
            }
            return 0;
        }
        else if (arg == "--continue" || arg == "-c") {
            waitForSnapNum = true;
            baseSnapshot = "default";
        }
        else if (arg == "--help" || arg == "-h" ) {
            getHelp();
            return 0;
        }
        else {
            if (waitForSnapNum) {
                baseSnapshot = arg;
                waitForSnapNum = false;
            } else {
                getHelp();
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

    parseOptions(argc, argv);
}
