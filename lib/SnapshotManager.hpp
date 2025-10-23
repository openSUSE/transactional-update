/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  The SnapshotManager class is the central API class for general snapshot
  mangement, i.e. for everything which is not bound to a specific transaction
  such as listing all snapshots or getting the default snapshot.

  Use the Transaction class for creating new snapshots.
 */

#ifndef T_U_SNAPSHOTMANAGER_H
#define T_U_SNAPSHOTMANAGER_H

#include <deque>
#include <map>
#include <memory>
#include <string>

namespace TransactionalUpdate {

class Snapshot;

/**
 * @brief The SnapshotManager class is an abstract class and may return different implementations, though
 * currently only snapper is implemented as a backend.
 * To retrieve an implementation suitable for the running system call
 * @example: std::unique_ptr<TransactionalUpdate::SnapshotManager> snapshotMgr = TransactionalUpdate::SnapshotFactory::get();
 */
class SnapshotManager
{
public:
    /* Internal methods */
    SnapshotManager() = default;
    virtual ~SnapshotManager() = default;
    virtual std::unique_ptr<Snapshot> create(std::string base, std::string description) = 0;
    virtual std::unique_ptr<Snapshot> open(std::string id) = 0;

    /**
     * @brief getList List of snapshots
     * @param columns The columns to show, separated by colons; permitted values are the values accepted by
     * the `snapper list --columns` command; if an empty string is given 'number,date,description' are
     * used as a default.
     * @return The list of snapshots with the processed columns.
     */
    virtual std::deque<std::map<std::string, std::string>> getList(std::string columns) = 0;

    /**
     * @brief getCurrent
     * @return Returns the ID of the currently running snapshot.
     */
    virtual std::string getCurrent() = 0;

    /**
     * @brief getDefault
     * @return Returns the ID of the currently set default snapshot.
     */
    virtual std::string getDefault() = 0;

    /**
     * @brief deleteSnap Deletes the given snapshot; note that the method may fail if the snapshot
     * cannot be deleted because it is currently in use or set as the default snapshot, so
     * getCurrent() and getDefault() should be used beforehand.
     * @param id ID of the snapshot to be deleted.
     */
    virtual void deleteSnap(std::string id) = 0;

    /**
     * @brief Set the given snapshot ID as the default snapshot ID
     * @param id ID of the snapshot to be rolled back to.
     */
    virtual void rollbackTo(std::string id) = 0;
};

class SnapshotFactory {
public:
    /**
     * @brief get See example for SnapshotManager
     * @param (optional) sets the SnapshotManager implementation
     * @return Returns a SnapshotManager instance suitable for the system
     */
    static std::unique_ptr<SnapshotManager> get(std::string sm = "auto");
};

struct SnapshotTable {

};

} // namespace TransactionalUpdate

#endif // T_U_SNAPSHOTMANAGER_H
