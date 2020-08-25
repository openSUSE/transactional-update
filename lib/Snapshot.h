/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Factory / interface class for snapshot generation implementations;
  implementations can be found in the "Snapshot" directory
 */

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <filesystem>
#include <memory>
#include <string>

class Snapshot {
public:
    Snapshot() = default;
    virtual ~Snapshot() = default;
    virtual void create(std::string base) = 0;
    virtual void open(std::string id) = 0;
    virtual void close() = 0;
    virtual void abort() = 0;
    virtual std::filesystem::path getRoot() = 0;
    virtual std::string getCurrent() = 0;
    virtual std::string getDefault() = 0;
    virtual bool isInProgress() = 0;
    virtual bool isReadOnly() = 0;
    virtual void setReadOnly(bool readonly) = 0;
    std::string getUid() { return snapshotId; };
protected:
    std::string snapshotId;
};

class SnapshotFactory {
public:
    static std::unique_ptr<Snapshot> get();
};

#endif /* SNAPSHOT_H */
