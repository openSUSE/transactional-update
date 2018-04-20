#!/bin/bash -e
#
# Purge contents of etc overlay on first boot after creating new snapshot
#
# Author: Ignaz Forster <iforster@suse.de>
# Copyright (C) 2018 SUSE Linux GmbH
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

ETC_OVERLAY="${NEWROOT}/var/lib/overlay/etc"
TU_FLAGFILE="${NEWROOT}/var/lib/overlay/transactional-update.newsnapshot"

# Import common dracut variables
. /dracut-state.sh 2>/dev/null

same_file() {
  # Primary indicators for changes: File size, attributes and modification time
  local overlay_stat="`stat --format="%a %u:%g %s %t/%T %F %Y" -- "$1"`"
  local lowerdir_stat="`stat --format="%a %u:%g %s %t/%T %F %Y" -- "$2"`"
  if [ "${overlay_stat}" != "${lowerdir_stat}" ]; then
    return 1
  fi

  # Compare extended attributes if available
  if [ -x /usr/bin/getfattr ]; then
    overlay_stat="`getfattr --dump --no-dereference --absolute-names -- "$1" | sed 1d`"
    lowerdir_stat="`getfattr --dump --no-dereference --absolute-names -- "$2" | sed 1d`"
    if [ "${overlay_stat}" != "${lowerdir_stat}" ]; then
      return 1
    fi
  fi

  # Files seem to be identical
  return 0
}

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
      if same_file "${file}" "${snapdir}/${file}"; then
        rmdir --ignore-fail-on-non-empty -- "${file}"
      fi
    # Overlayfs creates a character device with device number 0/0 for removed files / directories
    elif [ -c "${file}" -a "`stat --format="%t/%T" -- "${file}"`" = "0/0" -a ! -e "${snapdir}/${file}" ]; then
      echo "Removing character device ${dir}/${file} from overlay..."
      rm -- "${file}"
    # Verify that files in the overlay haven't changed since taking the snapshot
    elif same_file "${file}" "${snapdir}/${file}"; then
      echo "Removing copy of ${dir}/${file} from overlay..."
      rm -- "${file}"
    # File seems to have been modified, warn user
    else
      echo "Warning: Not removing ${dir}/${file} - modified after snapshot creation."
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
