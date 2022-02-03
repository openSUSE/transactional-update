/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2021 SUSE LLC */

/*
  The SnapshotManager class is the central API class for general snapshot
  mangement, i.e. for everything which is not bound to a specific transaction
  such as listing all snapshots or getting the default snapshot.

  Use the Transaction class for creating new snapshots.
 */

#ifndef T_U_SNAPSHOTMANAGER_H
#define T_U_SNAPSHOTMANAGER_H

#include <Snapshot.hpp>
#include <deque>
#include <map>
#include <string>

namespace TransactionalUpdate {

class SnapshotManager
{
public:
    SnapshotManager() = default;
    virtual ~SnapshotManager() = default;
    virtual std::unique_ptr<Snapshot> create(std::string base) = 0;
    virtual std::unique_ptr<Snapshot> open(std::string id) = 0;
    virtual std::deque<std::map<std::string, std::string>> getList(std::string columns) = 0;
    virtual std::string getCurrent() = 0;
    virtual std::string getDefault() = 0;
};

class SnapshotFactory {
public:
    static std::unique_ptr<SnapshotManager> get();
};

struct SnapshotTable {

};

} // namespace TransactionalUpdate

#endif // T_U_SNAPSHOTMANAGER_H
