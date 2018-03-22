#!/bin/bash

# Purge contents of etc overlay on first boot after creating new snapshot

ETC_OVERLAY="${NEWROOT}/var/lib/overlay/etc"
TA_FLAGFILE="${ETC_OVERLAY}/transactional-update.newsnapshot"

# Remove files from overlay with safety checks
# 
purge_overlay() {
  local dir="$1"
  local file

  cd "${ETC_OVERLAY}/${dir}"
  for file in .[^.]* ..?* *; do
    # Recursively process directories
    if [ -d "${file}" ]; then
      purge_overlay "${dir}/${file}"
      rmdir --ignore-fail-on-non-empty "${file}"
    # Overlayfs creates a character device for removed files
    elif [ -c "${file}" ]; then
      echo "transactional-update: ${dir}/${file} was deleted after snapshot creation."
    # Verify that files in the overlay haven't changed since taking the snapshot
    elif cmp --quiet "${file}" "${NEWROOT}/${PREV_SNAPSHOT_DIR}/etc/${dir}/${file}"; then
      echo "transactional-update: Deleting ${dir}/${file}..."
      rm "${file}"
    # File seems to have been modified, warn user
    else
      if [ -e "${file}" ]; then  # Filter unexpanded globs of "for" loop
        echo "transactional-update: ${dir}/${file} was modified after snapshot creation."
      fi
    fi
  done
  cd ..
}

if [ -e "${ETC_OVERLAY}" -a -e "${TA_FLAGFILE}" ]; then
  CURRENT_SNAPSHOT_ID="`btrfs subvolume get-default /${NEWROOT} | sed 's#.*/.snapshots/\(.*\)/snapshot#\1#g'`"
  mount -t btrfs -o ro,subvol=@/.snapshots ${root#block:*} ${NEWROOT}/.snapshots
  # TODO: Fehlerbehandlung
  PREV_SNAPSHOT_ID="`sed -n 's#.*<pre_num>\([^>]*\)</pre_num>#\1#p' ${NEWROOT}/.snapshots/${CURRENT_SNAPSHOT_ID}/info.xml`"
  PREV_SNAPSHOT_DIR="`btrfs subvolume list /${NEWROOT} | sed -n 's#.*\(/.snapshots/'${PREV_SNAPSHOT_ID}'/snapshot\)#\1#p'`"
  if [ -n "${PREV_SNAPSHOT_DIR}" ]; then
    mount -t btrfs -o ro,subvol=@${PREV_SNAPSHOT_DIR} ${root#block:*} "${NEWROOT}/${PREV_SNAPSHOT_DIR}"
    rm "${TA_FLAGFILE}"
    cd "${ETC_OVERLAY}"
    purge_overlay .
    mount -o remount "${NEWROOT}/etc"
  else
    echo "transactional-update: Previous snapshot ${PREV_SNAPSHOT_ID} not available any more - continuing..."
  fi
fi

umount "${NEWROOT}/${PREV_SNAPSHOT_DIR}"
umount "${NEWROOT}/.snapshots"
