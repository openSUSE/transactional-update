/*
  Base class for commands (see "Commands" directory) which require a writeable
  root file system

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

#include "TransactionalCommand.h"

TransactionalCommand::TransactionalCommand(shared_ptr<Transaction> transaction) {
    cout << "Konstruktor TransactionalCommand" << endl;
    this->transaction = transaction;
    if (transaction->isInitialized() == false) {
        this->transaction->open();
        chrootDir = transaction->getChrootDir();
    }
    cout << "Transaction Use Count: " << transaction.use_count() << endl;
}

TransactionalCommand::~TransactionalCommand() {
    cout << "Destruktor Transactional Command" << endl;
}
