/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Wrapper for reboot methods
 */

#include "Reboot.hpp"
#include "BlsEntry.hpp"
#include "Configuration.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Plugins.hpp"
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
        if (std::filesystem::exists("/usr/bin/rebootmgrctl")) {
            try {
                Util::exec("/usr/bin/rebootmgrctl is-active --quiet");
                method = "rebootmgr";
            } catch (ExecutionException &e) {
            }
        }
    }

    if (std::filesystem::exists("/run/reboot-needed")) {
        std::ifstream rebootfile;
        rebootfile.open("/run/reboot-needed");
        rebootfile >> type;
        rebootfile.close();
    }

    tulog.info("Minimally required reboot level: " + type);

    // Deprecated
    if (method == "kexec") {
        method = "systemd";
        type = "force-kexec";
    }

    if (method == "rebootmgr") {
        if (type == "soft-reboot" && config.get("REBOOT_ALLOW_SOFT_REBOOT") == "true") {
            tulog.info("Triggering reboot using rebootmgrctl soft-reboot.");
            command  = "/usr/bin/rebootmgrctl soft-reboot";
        } else {
            tulog.info("Triggering reboot using rebootmgrctl reboot.");
            command  = "/usr/bin/rebootmgrctl reboot";
        }
    } else if (method == "notify") {
        tulog.info("Triggering reboot using transactional-update-notifier client.");
        command  = "/usr/bin/transactional-update-notifier client";
    } else if (method == "systemd") {
        command  = "sync;";
        if (type == "soft-reboot" && config.get("REBOOT_ALLOW_SOFT_REBOOT") == "true") {
            tulog.info("Triggering reboot using systemctl soft-reboot.");
            command += "systemctl soft-reboot;";
        } else if (type == "force-kexec" || ((type == "kexec" || type == "soft-reboot") && config.get("REBOOT_ALLOW_KEXEC") == "true")) {
            auto sm = SnapshotFactory::get();
            sm->getDefault();
            std::unique_ptr<Snapshot> defaultSnap = sm->open(sm->getDefault());

            auto kernel = std::string(defaultSnap->getRoot() / "boot" / "vmlinuz");
            auto initrd = std::string(defaultSnap->getRoot() / "boot" / "initrd");
            if (!std::filesystem::exists(kernel)) {
                // If /boot/vmlinuz is not found, probably the system is using BLS entries
                // BLS entries are outside of snapshots
                auto efi = std::filesystem::path("/boot/efi");
                auto bls_entry_path = Util::exec("/usr/bin/sdbootutil list-entries --only-default");
                Util::trim(bls_entry_path);
                std::tie(kernel, initrd) =
                    BlsEntry::parse_bls_entry(efi / "loader" / "entries" / bls_entry_path);
                // relative_path strips the path of the root ("/"), otherwise the operator/
                // doesn't work and just returns the value of efi
                kernel = efi / std::filesystem::path(kernel).relative_path();
                Util::sanitize_quotes(kernel);
                initrd = efi / std::filesystem::path(initrd).relative_path();
                Util::sanitize_quotes(initrd);
            }
            tulog.info("Triggering reboot using systemctl kexec.");
            command  = "kexec --kexec-syscall-auto -l '" + kernel + "' --initrd='" + initrd + "' --reuse-cmdline;";
            command += "systemctl kexec;";
        } else {
            tulog.info("Triggering reboot using systemctl reboot.");
            command += "systemctl reboot;";
        }
    } else if (method == "kured") {
        tulog.info("Triggering reboot using kured.");
        command  = "touch /var/run/reboot-required";
    } else if (method == "none") {
        tulog.info("Reboots are disabled.");
        command  = "true;";
    } else {
        throw std::invalid_argument{"Unknown reboot method '" + method + "'."};
    }
}

void Reboot::reboot() {
    TransactionalUpdate::Plugins plugins{nullptr, false};
    plugins.run("reboot-pre", nullptr);
    Util::exec(command);
}

}
