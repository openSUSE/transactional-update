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
    this->method = method;
}

void Reboot::reboot() {
    std::string command;

    tulog.info("Triggering reboot using " + method);

    if (method == "rebootmgr") {
        command  = "/usr/sbin/rebootmgrctl reboot";
    } else if (method == "systemd") {
        command  = "sync;";
        command += "systemctl reboot;";
    } else if (method == "kured") {
        command  = "touch /var/run/reboot-required";
    } else if (method == "kexec") {
        command  = "kexec -l /boot/vmlinuz --initrd=/boot/initrd --reuse-cmdline;";
        command += "sync;";
        command += "systemctl kexec;";
    } else if (method == "none") {
        command  = "true;";
    } else {
        throw std::invalid_argument{"Unknown reboot method '" + method + "'."};
    }

    Util::exec(command);
}

bool Reboot::isRebootScheduled() {
    if (method == "rebootmgr") {
        try {
            Util::exec("rebootmgrctl status --quiet");
        } catch (ExecutionException &e) {
            if (e.getReturnCode() > 0 && e.getReturnCode() < 4) {
                return true;
            }
        }
        return false;
    } else if (method == "systemd") {
        return false;
    } else if (method == "kured") {
        if (std::filesystem::exists("/var/run/reboot-required")) {
            return true;
        } else {
            return false;
        }
    } else if (method == "kexec") {
        return false;
    } else if (method == "none") {
        return false;
    } else {
        throw std::invalid_argument{"Unknown reboot method '" + method + "'."};
    }
}

}
