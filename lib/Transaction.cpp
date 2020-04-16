/*
  A Transaction is unique instance, shared between all classes derived from
  the "TransactionalCommand" base class; that way it is made sure that all
  commands operate on the same snapshot. In case the destructor should be
  called before the transaction instance is closed, an eventual snapshot will
  be deleted again.

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

#include "Transaction.h"
#include "Log.h"
#include "Util.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
using namespace std;

Transaction::Transaction() {
    tulog.debug("Constructor Transaction");
}

Transaction::~Transaction() {
    tulog.debug("Destructor Transaction");
    dirsToMount.clear();
    try {
        filesystem::remove_all(filesystem::path{bindDir});
    }  catch (exception e) {
        tulog.error("ERROR: ", e.what());
    }
    if (isInitialized())
        snapshot->abort();
}

bool Transaction::isInitialized() {
    return snapshot ? true : false;
}

string Transaction::getChrootDir()
{
    return bindDir;
}

void Transaction::init() {
    snapshot = SnapshotFactory::create();

    dirsToMount.push_back(make_unique<BindMount>("/dev"));
    dirsToMount.push_back(make_unique<BindMount>("/var/log"));

    Mount mntVar{"/var"};
    if (mntVar.isMounted()) {
        dirsToMount.push_back(make_unique<BindMount>("/var/cache"));
        dirsToMount.push_back(make_unique<BindMount>("/var/lib/alternatives"));
    }
    Mount mntEtc{"/etc"};
    if (mntEtc.isMounted() && mntEtc.getFS() == "overlay") {
        // Lots of things TODO
    }

    unique_ptr<Mount> mntProc{new Mount{"/proc"}};
    mntProc->setType("proc");
    mntProc->setSource("none");
    dirsToMount.push_back(std::move(mntProc));

    unique_ptr<Mount> mntSys{new Mount{"/sys"}};
    mntSys->setType("sysfs");
    mntSys->setSource("sys");
    dirsToMount.push_back(std::move(mntSys));

    dirsToMount.push_back(make_unique<BindMount>("/.snapshots"));

    for (auto it = dirsToMount.begin(); it != dirsToMount.end(); ++it) {
        it->get()->mount(snapshot->getRoot());
    }

    // When all mounts are set up, then bind mount everything into a temporary
    // directory - GRUB needs to have an actual mount point for the root
    // partition
    char bindTemplate[] = "/tmp/transactional-update-XXXXXX";
    bindDir = mkdtemp(bindTemplate);
    unique_ptr<BindMount> mntBind{new BindMount{bindDir, MS_REC}};
    mntBind->setSource(snapshot->getRoot());
    mntBind->mount();
    dirsToMount.push_back(std::move(mntBind));
}

int Transaction::execute(const char* argv[]) {
    std::string opts = "Executing `";
    int i = 0;
    while (argv[i]) {
        if (i > 0)
            opts.append(" ");
        opts.append(argv[i]);
        i++;
    }
    opts.append("`:");
    tulog.info(opts);

    int status = 1;
    pid_t pid = fork();
    if (pid < 0) {
        throw runtime_error{"fork() failed: " + string(strerror(errno))};
    } else if (pid == 0) {
        if (chroot(bindDir.c_str()) < 0) {
            throw runtime_error{"Chrooting to " + bindDir + " failed: " + string(strerror(errno))};
        }
        cout << "◸" << flush;
        if (execvp(argv[0], (char* const*)argv) < 0) {
            throw runtime_error{"Calling " + string(argv[0]) + " failed: " + string(strerror(errno))};
        }
    } else {
        int ret;
        ret = waitpid(pid, &status, 0);
        cout << "◿" << endl;
        if (ret < 0) {
            throw runtime_error{"waitpid() failed: " + string(strerror(errno))};
        } else {
            tulog.info("Application returned with exit status ", WEXITSTATUS(status), ".");
        }
    }
    return WEXITSTATUS(status);
}

void Transaction::finalize() {
    snapshot->close();
    snapshot.reset();
}
