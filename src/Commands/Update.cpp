/*
  Update the system to the newest patchlevel

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

#include "Update.h"
#include "PackageManager.h"
#include <algorithm>

Update::Update(shared_ptr<Transaction> transaction) : TransactionalCommand(transaction) {
}

Update::~Update() {
}

void Update::execute() {
    cout << "Performing distribution upgrade" << endl;
    unique_ptr<PackageManager> pkgmgr = PackageManagerFactory::create();
    pkgmgr->doUpdate(chrootDir);
}
