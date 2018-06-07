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

FILES_TO_DELETE=()
LEFTOVER_FILES=()

# Import common dracut variables
. /dracut-state.sh 2>/dev/null

same_file() {
  # Check if file exists in lowerdir
  if [ ! -e "$2" ]; then
    return 1
  fi

  if [ -d "$1" ]; then
    # Directories will be merged, so sizes won't match
    stat_format="%a %u:%g %t/%T %F %Y"
  else
    stat_format="%a %u:%g %s %t/%T %F %Y"
  fi

  # Primary indicators for changes: File size, attributes and modification time
  local overlay_stat="`stat --format="${stat_format}" -- "$1"`"
  local lowerdir_stat="`stat --format="${stat_format}" -- "$2"`"
  if [ "${overlay_stat}" != "${lowerdir_stat}" ]; then
    return 1
  fi

  # Compare extended attributes if available (without overlayfs attributes)
  if [ -x /usr/bin/getfattr ]; then
    overlay_stat="`getfattr --dump --no-dereference --absolute-names --match=- -- "$1" | sed '1d;/^trusted\.overlay\./d'`"
    lowerdir_stat="`getfattr --dump --no-dereference --absolute-names --match=- -- "$2" | sed '1d;/^trusted\.overlay\./d'`"
    if [ "${overlay_stat}" != "${lowerdir_stat}" ]; then
      return 1
    fi
  fi

  # Files seem to be identical
  return 0
}

dump_dir() {
  pushd "$1" >/dev/null
  if [ -e "$2" ]; then
    ls --almost-all --indicator-style=slash -1 --escape --recursive -- "$2"
  fi
  popd >/dev/null
}

# Remove files from overlay with safety checks
# etc directory mustn't be deleted completely, as it's still mounted and the
# kernel won't be able to recover from that (different inode?)
clean_overlay() {
  local dir="${1:-.}"
  local file
  local snapdir="${NEWROOT}/${PREV_SNAPSHOT_DIR}/etc/${dir}"
  local dir_was_same
  local mode="${2:-delete}"
  local can_be_removed=()
  local cannot_be_removed=()

  pushd "${ETC_OVERLAY}/${dir}" >/dev/null
  for file in .[^.]* ..?* *; do
    # Filter unexpanded globs of "for" loop
    if [ ! -e "${file}" ]; then
      continue
    fi

    # Recursively process directories
    if [ -d "${file}" ]; then
      if same_file "${file}" "${snapdir}/${file}"; then
        # If all files could be removed from the overlay then the containing
        # directory can be removed, too, if there have been no changes to the
        # directory itself; this, however, has to be checked before deleting
        # any files in that directory, as doing so will necessarily modify
        # the directory's attributes.
        # TODO: Reset timestamp after deleting containing file(s)
        dir_was_same=1
      else
        dir_was_same=0
      fi
      # Opaque directories will completely hide the original directory, so it
      # can only be removed if contents are absolutely identical to the
      # snapshot
      if [[ "`getfattr -n 'trusted.overlay.opaque' --dump --absolute-names -- "${file}" 2>/dev/null`" =~ =\"y\"$ ]] && [ "${mode}" != "dry" ]; then
        # Have any files been added or removed?
        if [ "`dump_dir . "${file}"`" != "`dump_dir "${snapdir}" "${file}"`" ]; then
          continue
        fi
        LEFTOVER_FILES=()
        # Have any files been modified?
        clean_overlay "${dir}/${file}" dry
        if [ ! -z "${LEFTOVER_FILES}" ]; then
          continue
        fi
      fi
      clean_overlay "${dir}/${file}" "${mode}"
      if [ "${dir_was_same}" -eq 1 ]; then
        can_be_removed+=("${dir}/${file}/")
      else
        cannot_be_removed+=("${dir}/${file}/")
      fi
    # Overlayfs creates a character device with device number 0/0 for removed files / directories
    elif [ -c "${file}" -a "`stat --format="%t/%T" -- "${file}"`" = "0/0" -a ! -e "${snapdir}/${file}" ]; then
      can_be_removed+=("${dir}/${file}")
    # Verify that files in the overlay haven't changed since taking the snapshot
    elif same_file "${file}" "${snapdir}/${file}"; then
      can_be_removed+=("${dir}/${file}")
    else
      cannot_be_removed+=("${dir}/${file}")
    fi
  done
  popd >/dev/null

  if [ "${mode}" = "dry" ]; then
    LEFTOVER_FILES+=("${cannot_be_removed[@]}")
  elif [ "${mode}" = "delete" ]; then
    FILES_TO_DELETE+=("${can_be_removed[@]}")
  fi
}

delete_files() {
  cd "${ETC_OVERLAY}"
  for file in "${FILES_TO_DELETE[@]}"; do
    if [ -d "${file}" ]; then
      rmdir --ignore-fail-on-non-empty -- "${file}"
      # Check if directory was actually removed
      if [ ! -e "${file}" ]; then
        echo "Removing ${file} from overlay..."
      fi
    else
      rm -- "${file}"
      echo "Removing ${file} from overlay..."
    fi
  done

  echo "The following files weren't removed due to changes after snapshot creation:"
  ls --almost-all --indicator-style=slash -1 --escape --recursive -- "${ETC_OVERLAY}"
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
  CURRENT_SNAPSHOT_ID="`findmnt /${NEWROOT} | sed -n 's#.*\[/@/\.snapshots/\([[:digit:]]\+\)/snapshot\].*#\1#p'`"
  . "${TU_FLAGFILE}"

  if [ "${CURRENT_SNAPSHOT_ID}" = "${EXPECTED_SNAPSHOT_ID}" ]; then
    rm "${TU_FLAGFILE}"
    if prepare_environment; then
      clean_overlay
      delete_files
    else
      # Previous snapshot may not be available; just delete all overlay contents in this case
      remove_overlay
    fi
    mount -o remount "${NEWROOT}/etc"
    release_environment
  fi
fi
