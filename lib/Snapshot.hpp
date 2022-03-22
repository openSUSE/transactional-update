/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Factory / interface class for snapshot generation implementations;
  implementations can be found in the "Snapshot" directory
 */

#ifndef T_U_SNAPSHOT_H
#define T_U_SNAPSHOT_H

#include <filesystem>
#include <memory>
#include <string>

namespace TransactionalUpdate {

class Snapshot {
public:
    Snapshot(std::string id): snapshotId{id} {};
    virtual ~Snapshot() = default;
    virtual void close() = 0;
    virtual void abort() = 0;
    virtual std::filesystem::path getRoot() = 0;
    virtual bool isInProgress() = 0;
    virtual bool isReadOnly() = 0;
    virtual void setDefault() = 0;
    virtual void setReadOnly(bool readonly) = 0;
    std::string getUid() { return snapshotId; };
protected:
    std::string snapshotId;
};

} // namespace TransactionalUpdate

#endif // T_U_SNAPSHOT_H
