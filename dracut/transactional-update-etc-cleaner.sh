#!/bin/bash -e
#
# Check for conflicts in etc overlay on first boot after creating new snapshot
#
# Author: Ignaz Forster <iforster@suse.com>
# Copyright (C) 2024 SUSE LLC
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

if [ "$1" == "--dry-run" ] || [ "$1" == "-n" ]; then
  DRYRUN=1
  shift
fi

if [ -e /etc/etc.syncpoint ] || [ $# -eq 3 ]; then
  echo "First boot of snapshot: Merging /etc changes..."

  if [ $# -eq 3 ]; then
    # Allow overwriting default locations for testing
    parentdir="$1/"
    currentdir="$2/"
    syncpoint="$3/"
  else
    syncpoint="/etc/etc.syncpoint/"
    parent="$(< "${syncpoint}/transactional-update.comparewith")"
    parentdir="/.snapshots/${parent}/snapshot/etc/"
    currentdir="/etc/"
  fi

  declare -A REFERENCEFILES
  declare -A PARENTFILES
  declare -A CURRENTFILES
  declare -A DIFFTOCURRENT

  shopt -s globstar dotglob nullglob

  cd "${syncpoint}"
  for f in **; do
    REFERENCEFILES["${f}"]=.
  done
  cd "${parentdir}"
  for f in **; do
    PARENTFILES["${f}"]=.
  done
  cd "${currentdir}"
  for f in **; do
    CURRENTFILES["${f}"]=.
  done

  # Check which files have been changed in new snapshot
  for file in "${!REFERENCEFILES[@]}"; do
    if [ -z "${CURRENTFILES[${file}]}" ]; then
      echo "File '$file' got deleted in new snapshot."
      DIFFTOCURRENT[${file}]=recursiveskip
    elif [ -d "${currentdir}/${file}" ] && [ "$(stat --printf="%a %B %F %g %u %Y" "${syncpoint}/${file}")" != "$(stat --printf="%a %B %F %g %u %Y" "${currentdir}/${file}")" ]; then
      echo "Directory '$file' was changed in new snapshot."
      DIFFTOCURRENT[${file}]=skip
    elif [ ! -d "${currentdir}/${file}" ] && [ "$(stat --printf="%a %B %F %g %s %u %Y" "${syncpoint}/${file}")" != "$(stat --printf="%a %B %F %g %s %u %Y" "${currentdir}/${file}")" ]; then
      echo "File '$file' was changed in new snapshot."
      DIFFTOCURRENT[${file}]=skip
    elif [ "$(getfattr --no-dereference --dump --match='' "${syncpoint}/${file}" 2>&1 | tail --lines=+3)" != "$(getfattr --no-dereference --dump --match='' "${currentdir}/${file}" 2>&1 | tail --lines=+3)" ]; then
      echo "Extended file attributes of '$file' were changed in new snapshot."
      DIFFTOCURRENT[${file}]=skip
    fi
  done
  for file in "${!CURRENTFILES[@]}"; do
    if [ -z "${REFERENCEFILES[${file}]}" ]; then
      echo "File or directory '$file' was added in new snapshot."
      DIFFTOCURRENT[${file}]=skip
    fi
  done

  # Check which files have been changed in old snapshot
  for file in "${!REFERENCEFILES[@]}"; do
    if [ -z "${PARENTFILES[${file}]}" ]; then
      echo "File '$file' got deleted in old snapshot."
      if [ -z "${DIFFTOCURRENT[${file}]}" ]; then
        DIFFTOCURRENT[${file}]=delete
        if [ -d "${currentdir}/${file}" ]; then
          # If some file was changed or added in a subdir of the new snapshot, then don't delete
          for index in "${!DIFFTOCURRENT[@]}"; do
            if [[ ${index} == "${file}/"* ]]; then
              DIFFTOCURRENT[${file}]=skip
            fi
          done
        fi
      fi
    elif [ -d "${parentdir}/${file}" ] && [ "$(stat --printf="%a %B %F %g %u %Y" "${syncpoint}/${file}")" != "$(stat --printf="%a %B %F %g %u %Y" "${parentdir}/${file}")" ]; then
      echo "Directory '$file' was changed in old snapshot."
      if [ -z "${DIFFTOCURRENT[${file}]}" ]; then
        DIFFTOCURRENT[${file}]=copy # cp -a for file; touch, chmod, chown with reference file for directory
      fi
    elif [ ! -d "${parentdir}/${file}" ] && [ "$(stat --printf="%a %B %F %g %s %u %Y" "${syncpoint}/${file}")" != "$(stat --printf="%a %B %F %g %s %u %Y" "${parentdir}/${file}")" ]; then
      echo "File '$file' was changed in old snapshot."
      if [ -z "${DIFFTOCURRENT[${file}]}" ]; then
        DIFFTOCURRENT[${file}]=copy # cp -a for file; touch, chmod, chown with reference file for directory
      fi
    elif [ "$(getfattr --no-dereference --dump --match='' "${syncpoint}/${file}" 2>&1 | tail --lines=+3)" != "$(getfattr --no-dereference --dump --match='' "${parentdir}/${file}" 2>&1 | tail --lines=+3)" ]; then
      echo "Extended file attributes of '$file' were changed in old snapshot."
      if [ -z "${DIFFTOCURRENT[${file}]}" ]; then
        DIFFTOCURRENT[${file}]=copy # getfattr --dump & setfattr --restore
      fi
    fi
  done
  for file in "${!PARENTFILES[@]}"; do
    if [ -z "${REFERENCEFILES[${file}]}" ]; then
      echo "File or directory '$file' was added in old snapshot."
      if [ -z "${DIFFTOCURRENT[${file}]}" ]; then
        DIFFTOCURRENT[${file}]=copy
      fi
    fi
  done

  # Sort files to prevent processing a file before the directory was created
  readarray -d '' DIFFTOCURRENT_SORTED < <(printf '%s\0' "${!DIFFTOCURRENT[@]}" | sort -z)

  for file in "${DIFFTOCURRENT_SORTED[@]}"; do
    if [ "${DIFFTOCURRENT[${file}]}" = "recursiveskip" ]; then
      for index in "${!DIFFTOCURRENT[@]}"; do
        if [[ ${index} == "${file}/"* ]]; then
          DIFFTOCURRENT[${index}]=skip
        fi
      done
    elif [ "${DIFFTOCURRENT[${file}]}" = "delete" ]; then
      rm -rf "${currentdir:?}/${file}"
    elif [ "${DIFFTOCURRENT[${file}]}" = "copy" ]; then
      if { [ -f "${parentdir}/${file}" ] && [ -d "${currentdir}/${file}" ]; } || { [ -d "${parentdir}/${file}" ] && [ -f "${currentdir}/${file}" ]; }; then
        echo "File ${file} changed type between file and directory."
        if [ -z "${DRYRUN}" ]; then
          rm -r "${currentdir:?}/${file}"
        fi
      fi
      if [ -d "${parentdir}/${file}" ]; then
        if [ -z "${DRYRUN}" ]; then
          mkdir --parents "${currentdir}/${file}"
          touch --no-dereference --reference="${parentdir}/${file}" "${currentdir}/${file}"
          chmod --no-dereference --reference="${parentdir}/${file}" "${currentdir}/${file}"
          chown --no-dereference --reference="${parentdir}/${file}" "${currentdir}/${file}"

          pushd "${parentdir}" >/dev/null
          extattrs="$(getfattr --no-dereference --dump -- "${file}")"
          pushd "${currentdir}" >/dev/null
          echo "${extattrs}" | setfattr --no-dereference --restore=-
          popd >/dev/null
          popd >/dev/null
        fi
      else
        if [ -z "${DRYRUN}" ]; then
          cp --no-dereference --archive "${parentdir}/${file}" "${currentdir}/${file}"
        fi
      fi
    fi
  done

  # Sync for this snapshot was done, so syncpoint can be removed
  if [ -z "${DRYRUN}" ]; then
    rm -rf "${syncpoint}"
  fi

# Border cases, which are defined as follows for now (mostly matching overlayfs' behavior):
# * If a directory was newly created both in old and new after snapshot creation, then the contents of both are merged
# * If a directory was deleted in new, but has changes or new files in old, then it stays deleted
# * If a directory was deleted in old, but has changes or new files in new, then take contents of new

fi
