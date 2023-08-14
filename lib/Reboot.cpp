/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Wrapper for reboot methods
 */

#include "Reboot.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Util.hpp"
#include <filesystem>

namespace TransactionalUpdate {

Reboot::Reboot(std::string method) {
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

    if (method == "rebootmgr") {
        command  = "/usr/sbin/rebootmgrctl reboot";
    } else if (method == "notify") {
        command  = "/usr/bin/transactional-update-notifier client";
    } else if (method == "systemd") {
        command  = "sync;";
        command += "systemctl reboot;";
    } else if (method == "kured") {
        command  = "touch /var/run/reboot-required";
    } else if (method == "kexec") {
        command  = "kexec -a -l /boot/vmlinuz --initrd=/boot/initrd --reuse-cmdline;";
        command += "sync;";
        command += "systemctl kexec;";
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
