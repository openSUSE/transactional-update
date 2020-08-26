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
#include <vector>

class Transaction {
public:
    Transaction();
    Transaction(std::string uuid);
    virtual ~Transaction();
    void init(std::string base);
    int execute(const char* argv[]);
    void finalize();
    void keep();
    bool isInitialized();
    std::string getSnapshot();
private:
    void addSupplements();
    void mount(std::string base = "");
    std::unique_ptr<Snapshot> snapshot;
    std::string bindDir;
    std::vector<std::unique_ptr<Mount>> dirsToMount;
    Supplements supplements;
};

#endif // T_U_TRANSACTION_H
