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

if [ -e /etc/etc.syncpoint -o $# -eq 3 ]; then
  echo "First boot of snapshot: Merging /etc changes..."

  if [ $# -eq 3 ]; then
    parentdir="$1/"
    currentdir="$2/"
    syncpoint="$3/"
  else
    syncpoint="/etc/etc.syncpoint/"
    parent="$(< "${syncpoint}/transactional-update.comparewith")"
    parentdir="/.snapshots/${parent}/snapshot/etc/"
    currentdir="/etc/"
  fi

  # Check for files changed in new snapshot during update and create excludes list
  excludesfile="$(mktemp "${syncpoint}/transactional-update.sync.changedinnewsnap.XXXXXX)")"
  # TODO: Mount parent /etc here for migrations from old overlay to new subvolumes
  rsync --archive --inplace --xattrs --acls --out-format='%n' --dry-run "${currentdir}" "${syncpoint}" > "${excludesfile}"
  # `rsync` and `echo` are using a different format to represent octals ("\#xxx" vs. "\xxx"); convert to `echo` syntax
  # First escape already escaped characters, then convert the octals, then escape other rsync special characters in filenames
  sed -i 's/\\/\\\\\\\\/g;s/\\\\#\([0-9]\{3\}\)/\\0\1/g;s/\[/\\[/g;s/?/\\?/g;s/*/\\*/g;s/#/\\#/g' "${excludesfile}"
  # Escape all escapes because they will also be parsed by the following echo, and write them all into a nul-separated file, so that we don't mix up end of filename and newline because they are unescaped now; prepend a slash for absolute paths
  sed 's/\\/\\\\/g' "${excludesfile}" | while read file; do echo -en "- $file\0"; done > "${excludesfile}.tmp"
  # Replace the first character of each file with a character class to force rsync's parser to always interpret a backslash in a file name as an escape character
  sed 's/^\(- \)\(.\)/\1[\2]/g;s/\(\x00- \)\(.\)/\1[\2]/g' "${excludesfile}.tmp" > "${excludesfile}"
  # If the first character of a filename was an escaped character, then escape it again correctly by moving the bracket one charater further
  sed -i 's/^\(- \[\\\)\]\(.\)/\1\2]/g;s/\(\x00- \[\\\)\]\(.\)/\1\2]/g' "${excludesfile}"

  # Sync files changed in old snapshot before reboot, but don't overwrite the files from above
  rsync --archive --inplace --xattrs --acls --delete --from0 --exclude-from "${excludesfile}" --itemize-changes "${parentdir}" "${currentdir}"
fi
