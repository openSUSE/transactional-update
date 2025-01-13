#!/bin/sh -e
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

mount --target-prefix /sysroot --fstab /sysroot/etc/fstab /.snapshots
mount --bind /sysroot/etc /sysroot/etc
mount -o remount,rw /sysroot/etc
parentid="$(cat /sysroot/etc/etc.syncpoint/transactional-update.comparewith)"

# Migration from overlayfs based system
if [ -e "/sysroot/var/lib/overlay/${parentid}" ]; then
  cp "/sysroot/var/lib/overlay/${parentid}"/etc/fstab /tmp/tu-fstab
  sed -i "s#:/sysroot/etc,#:/sysroot/.snapshots/${parentid}/snapshot/etc,#g" /tmp/tu-fstab
  mount --target-prefix "/sysroot/.snapshots/${parentid}/snapshot" --fstab /tmp/tu-fstab /etc
fi

/bin/transactional-update-sync-etc-state "/sysroot/.snapshots/${parentid}/snapshot/etc" "/sysroot/etc" "/sysroot/etc/etc.syncpoint"
