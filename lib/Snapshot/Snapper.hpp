/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#ifndef T_U_SNAPPER_H
#define T_U_SNAPPER_H

#include "SnapshotManager.hpp"
#include <filesystem>
#include <string>

namespace TransactionalUpdate {

class Snapper: public SnapshotManager, public Snapshot {
public:
    ~Snapper() = default;

    // Snapshot
    Snapper(std::string snap): Snapshot(snap) {};
    void close() override;
    void abort() override;
    std::filesystem::path getRoot() override;
    bool isInProgress() override;
    bool isReadOnly() override;
    void setDefault() override;
    void setReadOnly(bool readonly) override;

    // SnapshotManager
    Snapper(): Snapshot("") {};
    std::unique_ptr<Snapshot> create(std::string base) override;
    virtual std::unique_ptr<Snapshot> open(std::string id) override;
    std::deque<std::map<std::string, std::string>> getList(std::string columns) override;
    std::string getCurrent() override;
    std::string getDefault() override;
    void deleteSnap(std::string id) override;
private:
    std::string callSnapper(std::string);
    inline static bool snapperNoDbus;
};

} // namespace TransactionalUpdate

#endif // T_U_SNAPPER_H
