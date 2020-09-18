/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Handling of /etc overlayfs layers
 */

#ifndef T_U_OVERLAY_H
#define T_U_OVERLAY_H

#include "Mount.h"
#include <memory>
#include <string>
#include <vector>

namespace TransactionalUpdate {

class Overlay {
public:
    Overlay(std::string snapshot);
    virtual ~Overlay() = default;
    void create(std::string base);
    std::string getOldestSnapshot();
    void sync(std::string snapshot);
    void setMountOptions(std::unique_ptr<Mount>& mount);
    void setMountOptionsForMount(std::unique_ptr<Mount>& mount);

    std::vector<std::filesystem::path> lowerdirs;
    std::filesystem::path upperdir;
    std::filesystem::path workdir;
private:
    static std::string getIdOfOverlayDir(const std::string dir);
};

} // namespace TransactionalUpdate

#endif // T_U_OVERLAY_H
