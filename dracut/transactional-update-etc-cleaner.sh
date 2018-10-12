#!/bin/bash -e
#
# Check for conflicts in etc overlay on first boot after creating new snapshot
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

TU_FLAGFILE="${NEWROOT}/var/lib/overlay/transactional-update.newsnapshot"

# Import common dracut variables
. /dracut-state.sh 2>/dev/null

warn_on_conflicting_files() {
  local dir="${1:-.}"
  local file
  local snapdir="${PREV_ETC_OVERLAY}/${dir}"

  pushd "${CURRENT_ETC_OVERLAY}/${dir}" >/dev/null
  for file in .[^.]* ..?* *; do
    # Filter unexpanded globs of "for" loop
    if [ ! -e "${file}" ]; then
      continue
    fi

    if [ -e "${snapdir}/${file}" -a "${snapdir}/${file}" -nt "${CURRENT_ETC_OVERLAY}" ]; then
      echo "WARNING: ${dir}/${file} or its contents changed in both old and new snapshot after snapshot creation!"
      continue
    fi

    # Recursively process directories
    if [ -d "${file}" ]; then
      warn_on_conflicting_files "${dir}/${file}"
    fi
  done
  popd >/dev/null
}

if [ -e "${TU_FLAGFILE}" ]; then
  CURRENT_SNAPSHOT_ID="`findmnt /${NEWROOT} | sed -n 's#.*\[/@/\.snapshots/\([[:digit:]]\+\)/snapshot\].*#\1#p'`"
  . "${TU_FLAGFILE}"

  CURRENT_ETC_OVERLAY="${NEWROOT}/var/lib/overlay/${CURRENT_SNAPSHOT_ID}/etc"
  PREV_ETC_OVERLAY="${NEWROOT}/var/lib/overlay/${PREV_SNAPSHOT_ID}/etc"

  if [ "${CURRENT_SNAPSHOT_ID}" = "${EXPECTED_SNAPSHOT_ID}" -a -e "${CURRENT_ETC_OVERLAY}" ]; then
    rm "${TU_FLAGFILE}"
    warn_on_conflicting_files
  fi
fi
