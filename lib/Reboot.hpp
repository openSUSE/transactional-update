/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Wrapper for reboot methods
 */

#ifndef T_U_REBOOT_H
#define T_U_REBOOT_H

#include <string>

namespace TransactionalUpdate {

class Reboot
{
public:
    /**
     * @brief Reboot the system using the given reboot method
     * @param method reboot method; possible values: auto|rebootmgr|systemd|kured|kexec
     *
     * "auto" will either use rebootmgr if active, or systemd otherwise. Will throw an exception if
     * the requested method is not available.
     */
    Reboot(std::string method);

    virtual ~Reboot() = default;

    /**
     * @brief Trigger the actual reboot.
     */
    void reboot();
protected:
    /**
     * @brief Contains the command which will be triggered during reboot().
     */
    std::string command;
};

} // namespace TransactionalUpdate

#endif // T_U_REBOOT_H
