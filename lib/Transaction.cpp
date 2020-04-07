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
#include "Util.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unistd.h>
using namespace std;

Transaction::Transaction() {
    cout << "Konstruktor Transaktion" << endl;
}

Transaction::~Transaction() {
    cout << "Destruktor Transaktion" << endl;
    dirsToMount.clear();
    filesystem::remove_all(filesystem::path{bindDir});
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

void Transaction::open() {
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

void Transaction::execute(string command) {
    chroot(bindDir.c_str());
    Util::exec(command);
    chroot("/");
}

void Transaction::close() {
    snapshot->close();
    snapshot.reset();
}
