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
    inst_hook pre-pivot 10 "$moddir/transactional-update-etc-cleaner.sh"
}
