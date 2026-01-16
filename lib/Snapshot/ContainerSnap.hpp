/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

#ifndef T_U_CONTAINER_SNAP_H
#define T_U_CONTAINER_SNAP_H

#include "Snapshot.hpp"
#include "SnapshotManager.hpp"

namespace TransactionalUpdate {

class ContainerSnap : public SnapshotManager {
public:
  ~ContainerSnap() = default;

  ContainerSnap() : SnapshotManager() {};
  std::unique_ptr<Snapshot> create(std::string base,
                                   std::string description) override;
  virtual std::unique_ptr<Snapshot> open(std::string id) override;

  /** Returns the list of snapshots, ignores the input columns (unsupported by
   * container-snap)
   */
  std::deque<std::map<std::string, std::string>>
  getList(std::string columns) override;
  std::string getCurrent() override;
  std::string getDefault() override;
  void deleteSnap(std::string id) override;
  void rollbackTo(std::string id) override;
};

class ContainerSnapshot : public Snapshot {
private:
  bool inProgress = true;

public:
  ~ContainerSnapshot() = default;

  ContainerSnapshot(std::string snap) : Snapshot(snap) {};
  void close() override;
  void abort() override;
  std::filesystem::path getRoot() override;
  bool isInProgress() override;
  bool isReadOnly() override;
  void setDefault() override;
  void setReadOnly(bool readonly) override;
};

} // namespace TransactionalUpdate

#endif // MACRO
