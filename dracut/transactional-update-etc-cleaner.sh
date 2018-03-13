#!/bin/sh

# Purge contents of etc overlay on first boot after a new snapshot

etc_overlay="/sysroot/var/lib/overlay/etc"
ta_flagfile="${etc_overlay}/transactional-update.newsnapshot"

if [ -e "${etc_overlay}" -a -e "${ta_flagfile}" ]; then
  rm "${ta_flagfile}"
  rm -rf "${etc_overlay}"/.[^.]* "${etc_overlay}"/..?* "${etc_overlay}"/*
  mount -o remount "/sysroot/etc"
fi
