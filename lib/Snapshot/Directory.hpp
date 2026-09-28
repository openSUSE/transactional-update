/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Directory backend for snapshot handling

  A snapshot manager backend which stores snapshots as plain directories
  instead of btrfs subvolumes. It requires neither root privileges nor any
  special file system features and is primarily meant for testing tukit in
  unprivileged environments, where a real btrfs / snapper setup is not available.

  The metadata is stored as follows:

    <SNAPSHOTS_DIR>/<id>/snapshot/   the actual file tree ("root")
    <SNAPSHOTS_DIR>/<id>/info        per-snapshot metadata (key=value)
    <SNAPSHOTS_DIR>/default          id of the default snapshot
    <SNAPSHOTS_DIR>/current          id of the "booted" snapshot (set by tests)
 */

#ifndef T_U_DIRECTORY_H
#define T_U_DIRECTORY_H

#include "SnapshotManager.hpp"
#include "Snapshot.hpp"
#include <filesystem>
#include <string>

namespace TransactionalUpdate {

class Directory: public SnapshotManager, public Snapshot {
public:
    ~Directory() = default;

    // Snapshot
    Directory(std::string snap): Snapshot(snap) {};
    void close() override;
    void abort() override;
    std::filesystem::path getRoot() override;
    bool isInProgress() override;
    bool isReadOnly() override;
    void setDefault() override;
    void setReadOnly(bool readonly) override;
    void cleanup(bool important) override;

    // SnapshotManager
    Directory(): Snapshot("") {};
    std::unique_ptr<Snapshot> create(std::string base, std::string description) override;
    std::unique_ptr<Snapshot> open(std::string id) override;
    std::deque<std::map<std::string, std::string>> getList(std::string columns) override;
    std::string getCurrent() override;
    std::string getDefault() override;
    void deleteSnap(std::string id) override;
    std::string rollback(std::string id) override;

private:
    std::string readMeta(const std::string& id, const std::string& key);
    void writeMeta(const std::string& id, const std::string& key, const std::string& value);
    void writeDefault(const std::string& id);
    std::string nextId();
};

} // namespace TransactionalUpdate

#endif // T_U_DIRECTORY_H
