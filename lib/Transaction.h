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

#ifndef TRANSACTION_H_
#define TRANSACTION_H_

#include "Mount.h"
#include "Snapshot.h"
#include <algorithm>
#include <vector>

class Transaction {
public:
    Transaction();
    virtual ~Transaction();
    void open();
    void execute(std::string command);
    void close();
    bool isInitialized();
    std::string getChrootDir();
private:
    std::unique_ptr<Snapshot> snapshot;
    std::string bindDir;
    std::vector<std::unique_ptr<Mount>> dirsToMount;
};

#endif /* TRANSACTION_H_ */
