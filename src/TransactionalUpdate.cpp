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

#include "TransactionalUpdate.h"
#include "Configuration.h"
#include "Util.h"
#include "Commands/Bootloader.h"
#include "Commands/Cleanup.h"
#include "Commands/DistUpgrade.h"
#include "Commands/GrubConfiguration.h"
#include "Commands/Update.h"
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

Configuration config;

void TransactionalUpdate::getHelp() {
    cout << "Syntax: transactional-update [option...] [general-command...] [package-command]" << endl;
    cout << "        transactional-update [option...] standalone-command" << endl;
    cout << "" << endl;
    cout << "Applies package updates to a new snapshot without touching the running" << endl;
    cout << "system." << endl;
    cout << "" << endl;
    cout << "General Commands:" << endl;
    cout << "cleanup                    Mark unused snapshots for snapper removal" << endl;
    cout << "grub.cfg                   Regenerate grub.cfg" << endl;
    cout << "bootloader                 Reinstall the bootloader" << endl;
    cout << "initrd                     Regenerate initrd" << endl;
    cout << "kdump                      Regenerate kdump initrd" << endl;
    cout << "shell                      Open rw shell in new snapshot before exiting" << endl;
    cout << "reboot                     Reboot after update" << endl;
    cout << "" << endl;
    cout << "Package Commands:" << endl;
    cout << "Defaults: (i) interactive command; (n) non-interactive command" << endl;
    cout << "dup                        Call 'zypper dup' (n)" << endl;
    cout << "up                         Call 'zypper up' (n)" << endl;
    cout << "patch                      Call 'zypper patch' (n)" << endl;
    cout << "migration                  Updates systems registered via SCC / SMT (i)" << endl;
    cout << "pkg install ...            Install individual packages (i)" << endl;
    cout << "pkg remove ...             Remove individual packages (i)" << endl;
    cout << "pkg update ...             Updates individual packages (i)" << endl;
    cout << "" << endl;
    cout << "Standalone Commands:" << endl;
    cout << "rollback [<number>]        Set the current or given snapshot as default snapshot" << endl;
    cout << "rollback last              Set the last working snapshot as default snapshot" << endl;
    cout << "" << endl;
    cout << "Options:" << endl;
    cout << "--interactive, -i          Use interactive mode for package command" << endl;
    cout << "--non-interactive, -n      Use non-interactive mode for package command" << endl;
    cout << "--continue [<number>], -c  Use latest or given snapshot as base" << endl;
    cout << "--no-selfupdate            Skip checking for newer version" << endl;
    cout << "--quiet                    Don't print warnings and infos to stdout" << endl;
    cout << "--help, -h                 Display this help and exit" << endl;
    cout << "--version                  Display version and exit" << endl << endl;
}

/* Does two things: Sets the command list and the global options */
int TransactionalUpdate::parseOptions(int argc, const char *argv[]) {
    deque<unique_ptr<Command>> commandList;
    shared_ptr<Transaction> transaction = make_shared<Transaction>();
    cout << "Use count: " << transaction.use_count() << endl;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "cleanup") {
            commandList.push_back(make_unique<Cleanup>());
        }
        else if (arg == "dup") {
            commandList.push_back(make_unique<DistUpgrade>(transaction));
            cout << "Use count: " << transaction.use_count() << endl;
        }
        else if (arg == "up" || arg == "patch") {
            commandList.push_back(make_unique<Update>(transaction));
            cout << "Use count: " << transaction.use_count() << endl;
        }
        else if (arg == "bootloader") {
            commandList.push_back(make_unique<Bootloader>(transaction));
            commandList.push_back(make_unique<GrubConfiguration>(transaction));
            cout << "Use count: " << transaction.use_count() << endl;
        }
        else if (arg == "grub.cfg") {
            commandList.push_back(make_unique<GrubConfiguration>(transaction));
            cout << "Use count: " << transaction.use_count() << endl;
        }
        else if (arg == "migration"
            || arg == "shell"
            || arg == "initrd"
            || arg == "kdump"
            || arg == "reboot"
            || arg == "salt"
            || arg == "--interactive" || arg == "-i"
            || arg == "--non-interactive" || arg == "-n"
            || arg == "--no-selfupdate"
            || arg == "--quiet"
            || arg == "--version") {
            Util::stub(arg);
        }
        else if (arg == "--continue" || arg == "-c") {
            Util::stub(arg);
            // Check if followed by a (snapshot) number
            if (i + 1 < argc) {
                string num = argv[i + 1];
                if (all_of(num.begin(), num.end(), ::isdigit)) {
                    unsigned int snapshot = stoi(num); // snapper numbers are uint, kernel is would be u64
                    i++;
                }
            }
        }
        else if (arg == "rollback"
              || arg == "ptf" || arg == "pkg" || arg == "package"
              || arg == "register") {
            Util::stub(arg);
            break;
        }
        else if (arg == "--help" || arg == "-h" ) {
            getHelp();
            return 0;
        }
        else {
            getHelp();
            throw invalid_argument{"Unknown command or option '" + arg + "'."};
        }
    }
    // If no commands were given, assume "up"
    if (commandList.empty()) {
        commandList.push_back(make_unique<DistUpgrade>(transaction));
    }
    while (commandList.size() >= 1) {
        commandList.front()->execute();
        commandList.pop_front();
    }
    transaction->close();
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
            throw runtime_error{"Another instance of transactional-update is already running: " + string(strerror(errno))};
            remove(config.get("LOCKFILE").c_str());
        }
    }
    ~Lock(){
        close(lockfile);
        remove(config.get("LOCKFILE").c_str());
    }
private:
    int lockfile;
};

TransactionalUpdate::TransactionalUpdate(int argc, const char *argv[]) {
    Lock lock;
    cout << "transactional-update @VERSION@ started" << endl;
    cout << "Options:";
    for (int i=1; i<argc; i++)
        cout << " " << argv[i];
    cout << endl;

    parseOptions(argc, argv);
}

TransactionalUpdate::~TransactionalUpdate() {
    // TODO Auto-generated destructor stub
}

