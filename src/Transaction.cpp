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

#include "Transaction.h"
#include "Mount.h"
#include <iostream>
#include <vector>
using namespace std;

Transaction::Transaction() {
    cout << "Konstruktor Transaktion" << endl;
}

Transaction::~Transaction() {
    cout << "Destruktor Transaktion" << endl;
    if (isInitialized())
        snapshot->abort();
}

bool Transaction::isInitialized() {
    return snapshot ? true : false;
}

string Transaction::getChrootDir()
{
    return snapshot->getRoot();
}

void Transaction::open() {
    snapshot = SnapshotFactory::create();

    vector<unique_ptr<Mount>> dirsToMount;

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

    // Create bind mounts, required by GRUB
    //dirsToMount.push_back(make_unique<BindMount>(snapshot->getRoot(), MS_REC));
    dirsToMount.push_back(make_unique<BindMount>("/.snapshots", MS_RDONLY));

    for (auto it = dirsToMount.begin(); it != dirsToMount.end(); ++it) {
        cout << "Mounting " << it->get()->getTarget() << endl;
        it->get()->mount(snapshot->getRoot());
    }
}

void Transaction::close() {
    snapshot->close();
    snapshot.reset();
}
