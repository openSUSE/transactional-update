/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  systemd-mstack backend for snapshot handling
 */

#include "Systemd.hpp"
#include "Configuration.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Mount.hpp"
#include "Util.hpp"
#include <cstdlib>
#include <regex>

namespace TransactionalUpdate {

/* SnapshotManager methods */

bool Systemd::available() {
    return std::filesystem::exists("/usr/bin/importctl") && std::filesystem::exists("/usr/bin/systemd-mstack");
}

std::unique_ptr<Snapshot> Systemd::create(std::string base, std::string description) {
    if (! available()) {
        throw std::runtime_error{"'importctl' and 'systemd-mstack' are required for the systemd snapshot manager."};
    }

    auto snap = Snapper::create(base, description);

    std::string oci_target = config.get("OCI_TARGET");

    try {
        char mountTemplate[] = "/tmp/transactional-update-XXXXXX";
        std::string mountpoint = mkdtemp(mountTemplate);

        tulog.info("Pulling image via systemd from: " + oci_target);
        Util::exec("importctl pull-oci " + oci_target + " -m --force");
        Util::exec("systemd-mstack --mount /var/lib/machines/bootable.mstack " + mountpoint);
        tulog.info("Writing contents of " + oci_target + " to snapshot directory " + getRoot().string() + "...");
        std::string excludes;
        for (auto path: MountList::getList()) {
            excludes += " --exclude ";
            excludes += path.string();
        }
        Util::exec("rsync --delete --archive --hard-links --xattrs --acls --inplace --one-file-system " + excludes + " " + mountpoint + "/ " + getRoot().string() + "/");
        Util::exec("rsync --archive --hard-links --xattrs --acls --inplace --one-file-system --ignore-existing " + mountpoint + "/etc/ " + getRoot().string() + "/etc/");
        tulog.info("Merging /etc from container image into existing snapshot, preserving existing configuration...");
        Util::exec("systemd-mstack --umount " + mountpoint);
        std::filesystem::remove(mountpoint);
        Util::exec("touch " + getRoot().string() + "/.autorelabel");
        return std::make_unique<Systemd>(snapshotId);
    } catch (const std::exception &e) {
        Snapper::deleteSnap(snap.get()->getUid());
        throw std::runtime_error{"Syncing systemd image failed."};
    }
}

} // namespace TransactionalUpdate
