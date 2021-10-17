/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Handling of /etc overlayfs layers
 */

#ifndef T_U_OVERLAY_H
#define T_U_OVERLAY_H

#include "Mount.hpp"
#include "SnapshotManager.hpp"
#include <memory>
#include <string>
#include <vector>

namespace TransactionalUpdate {

class Overlay {
public:
    Overlay(std::string snapshot);
    virtual ~Overlay() = default;
    void create(std::string base, std::string snapshot, std::filesystem::path snapRoot);
    std::string getPreviousSnapshotOvlId();
    bool references(std::string snapshot);
    void sync(std::string base, std::filesystem::path snapRoot);
    void setMountOptions(std::unique_ptr<Mount>& mount);
    void setMountOptionsForMount(std::unique_ptr<Mount>& mount);

    std::vector<std::filesystem::path> lowerdirs;
    std::filesystem::path upperdir;
    std::filesystem::path workdir;
private:
    static std::string getIdOfOverlayDir(const std::string dir);
    std::unique_ptr<SnapshotManager> snapMgr;
};

} // namespace TransactionalUpdate

#endif // T_U_OVERLAY_H
