/*
 SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  A Transaction is unique instance, shared between all classes derived from
  the "TransactionalCommand" base class; that way it is made sure that all
  commands operate on the same snapshot. In case the destructor should be
  called before the transaction instance is closed, an eventual snapshot will
  be deleted again.
 */

#ifndef T_U_TRANSACTION_H
#define T_U_TRANSACTION_H

#include "Mount.h"
#include "Snapshot.h"
#include "Supplement.h"
#include <algorithm>
#include <memory>
#include <vector>

namespace TransactionalUpdate {

class Transaction {
public:
    Transaction();
    virtual ~Transaction();
    void init(std::string base);
    void resume(std::string uuid);
    int execute(const char* argv[]);
    void finalize();
    void keep();
    bool isInitialized();
    std::string getSnapshot();
private:
    class impl;
    std::unique_ptr<impl> pImpl;
};

} // namespace TransactionalUpdate

#endif // T_U_TRANSACTION_H
