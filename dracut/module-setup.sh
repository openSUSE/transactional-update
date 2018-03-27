#!/bin/bash

# called by dracut
check() {
    test -f /etc/fstab.sys || [[ -n $add_fstab  ||  -n $fstab_lines ]]
}

# called by dracut
depends() {
    echo fstab-sys
}

# called by dracut
install() {
    inst_script "$moddir/transactional-update-etc-cleaner.sh" /bin/transactional-update-etc-cleaner
    inst_simple "$moddir/transactional-update-etc-cleaner.service" $systemdsystemunitdir/transactional-update-etc-cleaner.service
    mkdir -p "${initdir}/$systemdsystemunitdir/initrd.target.wants"
    ln_r "$systemdsystemunitdir/transactional-update-etc-cleaner.service" "$systemdsystemunitdir/initrd.target.wants/transactional-update-etc-cleaner.service"
    inst_multiple cmp rmdir
}
