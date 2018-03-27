#!/bin/bash -e

# Purge contents of etc overlay on first boot after creating new snapshot

ETC_OVERLAY="${NEWROOT}/var/lib/overlay/etc"
TU_FLAGFILE="${ETC_OVERLAY}/transactional-update.newsnapshot"

# Import common dracut variables
. /dracut-state.sh 2>/dev/null

# Remove files from overlay with safety checks
# etc directory mustn't be deleted completely, as it's still mounted and the kernel won't be able to recover from that (different inode?)
clean_overlay() {
  local dir="${1:-.}"
  local file
  local snapdir="${NEWROOT}/${PREV_SNAPSHOT_DIR}/etc/${dir}"

  pushd "${ETC_OVERLAY}/${dir}" >/dev/null
  for file in .[^.]* ..?* *; do
    # Filter unexpanded globs of "for" loop
    if [ ! -e "${file}" ]; then
      continue
    fi

    # Recursively process directories
    if [ -d "${file}" ]; then
      clean_overlay "${dir}/${file}"
      rmdir --ignore-fail-on-non-empty -- "${file}"
    # Overlayfs creates a character device for removed files / directories
    elif [ -c "${file}" -a ! -e "${snapdir}/${file}" ]; then
      echo "Removing character device ${dir}/${file} from overlay..."
      rm -- "${file}"
    # Verify that files in the overlay haven't changed since taking the snapshot
    elif cmp --quiet -- "${file}" "${snapdir}/${file}"; then
      echo "Removing copy of ${dir}/${file} from overlay..."
      rm -- "${file}"
    # File seems to have been modified, warn user
    else
      echo "Warning: Not removing - ${dir}/${file} file modified after snapshot creation."
    fi
  done
  popd >/dev/null
}

# Delete all contents of the overlay
remove_overlay() {
  echo "Previous snapshot ${PREV_SNAPSHOT_ID} not available; deleting all overlay contents."
  cd "${ETC_OVERLAY}"
  rm -rf -- .[^.]* ..?* *
}

# Mount directories necessary for file comparison
# Note: Will not break execution if snapshot is not available
prepare_environment() {
  if ! findmnt "${NEWROOT}/.snapshots" >/dev/null; then
    if ! mount -t btrfs -o ro,subvol=@/.snapshots ${root#block:*} ${NEWROOT}/.snapshots; then
      echo "Could not mount .snapshots!"
      return 1
    fi
    UMOUNT_DOT_SNAPSHOTS=1
  fi
  CURRENT_SNAPSHOT_ID="`btrfs subvolume get-default /${NEWROOT} | sed 's#.*/.snapshots/\(.*\)/snapshot#\1#g'`"
  PREV_SNAPSHOT_ID="`sed -n 's#.*<pre_num>\([^>]\+\)</pre_num>#\1#p' ${NEWROOT}/.snapshots/${CURRENT_SNAPSHOT_ID}/info.xml`"
  PREV_SNAPSHOT_DIR="`btrfs subvolume list /${NEWROOT} | sed -n 's#.*\(/.snapshots/'${PREV_SNAPSHOT_ID}'/snapshot\)#\1#p'`"
  if [ -n "${PREV_SNAPSHOT_DIR}" ] && ! findmnt "${NEWROOT}/${PREV_SNAPSHOT_DIR}" >/dev/null; then
    if ! mount -t btrfs -o ro,subvol=@${PREV_SNAPSHOT_DIR} ${root#block:*} "${NEWROOT}/${PREV_SNAPSHOT_DIR}"; then
      echo "Warning: Could not mount old snapshot directory ${PREV_SNAPSHOT_DIR}!"
      return 1
    fi
    UMOUNT_PREV_SNAPSHOT=1
  fi
  return 0
}

release_environment() {
  if [ -n "${UMOUNT_PREV_SNAPSHOT}" ]; then
    umount "${NEWROOT}/${PREV_SNAPSHOT_DIR}"
  fi
  if [ -n "${UMOUNT_DOT_SNAPSHOTS}" ]; then
    umount "${NEWROOT}/.snapshots"
  fi
}

if [ -e "${ETC_OVERLAY}" -a -e "${TU_FLAGFILE}" ]; then
  # Previous snapshot may not be available; just delete all overlay contents in this case
  rm "${TU_FLAGFILE}"
  if prepare_environment; then
    clean_overlay
  else
    remove_overlay
  fi
  mount -o remount "${NEWROOT}/etc"
  release_environment
fi
