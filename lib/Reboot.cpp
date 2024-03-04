/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Wrapper for reboot methods
 */

#include "Reboot.hpp"
#include "Configuration.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Mount.hpp"
#include "Snapshot.hpp"
#include "SnapshotManager.hpp"
#include "Util.hpp"
#include <filesystem>
#include <fstream>

namespace TransactionalUpdate {

Reboot::Reboot(std::string method) {
    std::string type = "reboot";
    if (method == "auto") {
        method = "systemd"; // Default
        if (std::filesystem::exists("/usr/sbin/rebootmgrctl")) {
            try {
                Util::exec("/usr/sbin/rebootmgrctl is-active --quiet");
                method = "rebootmgr";
            } catch (ExecutionException &e) {
            }
        }
    }

    tulog.info("Triggering reboot using " + method);

    if (std::filesystem::exists("/run/reboot-needed")) {
        std::ifstream rebootfile;
        rebootfile.open("/run/reboot-needed");
        rebootfile >> type;
        rebootfile.close();
    }

    // Deprecated
    if (method == "kexec") {
        method = "systemd";
        type = "force-kexec";
    }

    if (type == "soft-reboot" && config.get("REBOOT_ALLOW_SOFT_REBOOT") == "true") {
        auto sf = SnapshotFactory::get();
        std::unique_ptr<Snapshot> newSnap = sf->open(sf->getDefault());
        auto nextroot = new BindMount("/run/nextroot");
        nextroot->setSource(newSnap->getRoot());
        nextroot->mount();
    }

    if (method == "rebootmgr") {
        if (type == "soft-boot" && config.get("REBOOT_ALLOW_SOFT_REBOOT") == "true") {
            command  = "/usr/sbin/rebootmgrctl soft-reboot";
        } else {
            command  = "/usr/sbin/rebootmgrctl reboot";
        }
    } else if (method == "notify") {
        command  = "/usr/bin/transactional-update-notifier client";
    } else if (method == "systemd") {
        command  = "sync;";
        if (type == "soft-boot" && config.get("REBOOT_ALLOW_SOFT_REBOOT") == "true") {
            command += "systemctl soft-reboot;";
        } else if (type == "force-kexec" || ((type == "kexec" || type == "soft-boot") && config.get("REBOOT_ALLOW_KEXEC") == "true")) {
            command  = "kexec --kexec-syscall-auto -l /boot/vmlinuz --initrd=/boot/initrd --reuse-cmdline;";
            command += "systemctl kexec;";
        } else {
            command += "systemctl reboot;";
        }
    } else if (method == "kured") {
        command  = "touch /var/run/reboot-required";
    } else if (method == "none") {
        command  = "true;";
    } else {
        throw std::invalid_argument{"Unknown reboot method '" + method + "'."};
    }
}

void Reboot::reboot() {
    Util::exec(command);
}

}
