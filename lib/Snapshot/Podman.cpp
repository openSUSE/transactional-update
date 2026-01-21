/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Podman backend for snapshot handling
 */

#include "Podman.hpp"
#include "Configuration.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Mount.hpp"
#include "Util.hpp"
#include <regex>

namespace TransactionalUpdate {

/* SnapshotManager methods */

std::unique_ptr<Snapshot> Podman::create(std::string base, std::string description) {
    auto snap = Snapper::create(base, description);

    try {
        tulog.info("Pulling image from: " + config.get("OCI_TARGET"));
        Util::exec("podman image pull " + config.get("OCI_TARGET"));
        std::string ocimount = Util::exec("podman image mount " + config.get("OCI_TARGET"));
        Util::rtrim(ocimount);
        tulog.info("Writing contents of " + config.get("OCI_TARGET") + " to snapshot directory " + getRoot().string() + "...");
        std::string excludes;
        for (auto path: MountList::getList()) {
            excludes += " --exclude ";
            excludes += path.string();
        }
        Util::exec("rsync --delete --archive --hard-links --xattrs --acls --inplace --one-file-system " + excludes + ocimount + "/ " + getRoot().string() + "/");
        Util::exec("rsync --delete --archive --hard-links --xattrs --acls --inplace --one-file-system --ignore-existing " + ocimount + "/etc/ " + getRoot().string() + "/etc/");
        tulog.info("Merging /etc from container image into existing snapshot, preserving existing configuration...");
        Util::exec("podman image unmount " + config.get("OCI_TARGET"));
        Util::exec("touch " + getRoot().string() + "/.autorelabel");
        return std::make_unique<Podman>(snapshotId);
    } catch (const std::exception &e) {
        Snapper::deleteSnap(snap.get()->getUid());
        throw std::runtime_error{"Syncing podman image failed."};
    }
}

} // namespace TransactionalUpdate
