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

#ifndef TRANSACTION_H_
#define TRANSACTION_H_

#include "Snapshot.h"
#include <algorithm>

class Transaction {
public:
    Transaction();
    virtual ~Transaction();
    void open();
    void abort();
    bool isInitialized();
private:
    std::unique_ptr<Snapshot> snapshot;
};

#endif /* TRANSACTION_H_ */
