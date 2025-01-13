#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

# called by dracut
install() {
    inst_script "/usr/libexec/transactional-update-sync-etc-state" /bin/transactional-update-sync-etc-state
    inst_simple "$moddir/transactional-update-sync-etc-state.service" $systemdsystemunitdir/transactional-update-sync-etc-state.service
    inst_simple "$moddir/transactional-update-sync-etc-state.sh" /bin/transactional-update-sync-etc-state.sh
    mkdir -p "${initdir}/$systemdsystemunitdir/initrd.target.wants"
    ln_r "$systemdsystemunitdir/transactional-update-sync-etc-state.service" "$systemdsystemunitdir/initrd.target.wants/transactional-update-sync-etc-state.service"
}
