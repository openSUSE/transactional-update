# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

setup() {
	cd "$( dirname "$BATS_TEST_FILENAME" )"

	mockdir_old_etc="$(mktemp --directory /tmp/transactional-update.synctest.olddir.XXXX)" # Simulates changed files after snapshot creation
	mockdir_new_etc="$(mktemp --directory /tmp/transactional-update.synctest.newdir.XXXX)"
	mockdir_syncpoint="$(mktemp --directory /tmp/transactional-update.synctest.syncdir.XXXX)"

	totest="../dracut/transactional-update-sync-etc-state"

	umask 022
}

teardown() {
	rm -rf "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"
}

SPECIAL_FILENAMES=('NULL' '- looks like an exclude' '# looks like a comment' '; looks like a comment' '[ is a bracket as a first character' '\ is a backslash as the first character' 'File
                with spaces,
                newlines and tabs.txt' 'rsync special characters: * \ ?.txt' 'rsync escape sequence in an actual file name: \#012.txt' 'Bash special characters: " '"'"' ( ).txt' '**' '.hiddenfile' '-a' ' ')

createFile() {
	echo "# Creating file '${PWD}/$1'..."
	echo "Test" > "$1"
}

createFilesIn() {
	pushd $1 >/dev/null
	shift
	arr=("$@")
	for file in "${arr[@]}"; do
		createFile "${file}"
	done
	popd >/dev/null
}

syncTimestamps() {
	local referencedate="$(date -d "-10 seconds" +"%Y%m%d%H%M.%S")"
	for file in "$@"; do
		touch --no-dereference -t "${referencedate}" -- "$file"
	done
}

checkFilesWithSpecialFilenames() {
	local ret=0
	for file in "${SPECIAL_FILENAMES[@]}"; do
		echo -n "# Checking for existence of '${PWD}/${file}' - "
		if [ -e "${mockdir_new_etc}/${file}" ]; then
			echo "Found"
		else
			echo "Not found"
			ret=1
		fi
	done
	return ${ret}
}

debug() {
	echo
	echo "# Contents of exclude file:"
	cat "${mockdir_syncpoint}/transactional-update.sync.changedinnewsnap."*
	echo
	#echo "# Directory listing:"
	#find /tmp/transactional-update.synctest*
	#echo
}

@test "Special characters in file names (must not be deleted)" {
	createFilesIn "${mockdir_new_etc}" "${SPECIAL_FILENAMES[@]}"

	run $totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	#debug

	pushd "${mockdir_new_etc}" >/dev/null
	checkFilesWithSpecialFilenames
	popd >/dev/null
}

@test "Special characters in file names (to be synced)" {
	createFilesIn "${mockdir_old_etc}" "${SPECIAL_FILENAMES[@]}"

	run $totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	#debug

	pushd "${mockdir_new_etc}" >/dev/null
	checkFilesWithSpecialFilenames
	popd >/dev/null
}

@test "File contents and properties" {
	shopt -s globstar dotglob
	FILES=(File0.txt File1.txt File2.txt File3.txt File4.txt File5.txt)
	createFilesIn "${mockdir_old_etc}" "${FILES[@]}"
	createFilesIn "${mockdir_new_etc}" "${FILES[@]}"
	createFilesIn "${mockdir_syncpoint}" "${FILES[@]}"
	syncTimestamps "${mockdir_old_etc}"/** "${mockdir_new_etc}"/** "${mockdir_syncpoint}"/**

	# File contents (added)
	echo "more file contents" >> "${mockdir_old_etc}/${FILES[0]}"
	touch --reference="${mockdir_syncpoint}/${FILES[0]}" "${mockdir_old_etc}/${FILES[0]}"

	# Different file contents, but same length
	echo "asdf" > "${mockdir_old_etc}/${FILES[1]}"

	# Timestamps
	touch -a "${mockdir_old_etc}/${FILES[2]}"
	touch -m "${mockdir_old_etc}/${FILES[3]}"
	touch -a "${mockdir_old_etc}/${FILES[3]}"

	# No changes for File4.txt

	# Permissions
	chmod 777 "${mockdir_old_etc}/${FILES[5]}"
	touch --reference="${mockdir_syncpoint}/${FILES[5]}" "${mockdir_new_etc}/${FILES[5]}"

	run $totest --keep-syncpoint "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	echo "# Verifying appending contents to 'File0.txt' is detected"
	[[ "${lines[*]}" == *'File changed: "'${mockdir_old_etc}'/./File0.txt"'* ]]
	echo "# Verifying changing the contents with same size to 'File1.txt' is detected"
	[[ "${lines[*]}" == *'File changed: "'${mockdir_old_etc}'/./File1.txt"'* ]]
	echo "# Verifying changing the access time of 'File2.txt' doesn't update the file"
	[[ "${lines[*]}" != *'"./File2.txt"'* ]]
	echo "# Verifying changing the modification time of 'File3.txt' is detected"
	[[ "${lines[*]}" == *'File changed: "'${mockdir_old_etc}'/./File3.txt"'* ]]
	echo "# Verifying 'File4.txt' wasn't changed"
	[[ "${lines[*]}" != *'"./File4.txt"'* ]]
	echo "# Verifying changing permissions of 'File5.txt' is detected"
	[[ "${lines[*]}" == *'File changed: "'${mockdir_old_etc}'/./File5.txt"'* ]]

	echo "# Verify changes to just atime are ignored"
	# The human-readable %x/%y format has higher precision than %X/%Y
	[ "$(stat -c %x "${mockdir_new_etc}/${FILES[2]}")" != "$(stat -c %x "${mockdir_old_etc}/${FILES[2]}")" ]
	[ "$(stat -c %x "${mockdir_new_etc}/${FILES[2]}")" = "$(stat -c %x "${mockdir_syncpoint}/${FILES[2]}")" ]
	echo "# Verify atime + mtime changes are detected and applied properly"
	stat "${mockdir_new_etc}/${FILES[3]}" "${mockdir_old_etc}/${FILES[3]}" "${mockdir_syncpoint}/${FILES[3]}"
	# Bug: The sync process changes atime in the source directory (https://github.com/openSUSE/transactional-update/issues/147)
	# [ "$(stat -c %x "${mockdir_new_etc}/${FILES[3]}")" = "$(stat -c %x "${mockdir_old_etc}/${FILES[3]}")" ]
	[ "$(stat -c %x "${mockdir_new_etc}/${FILES[3]}")" != "$(stat -c %x "${mockdir_syncpoint}/${FILES[3]}")" ]
	[ "$(stat -c %y "${mockdir_new_etc}/${FILES[3]}")" = "$(stat -c %y "${mockdir_old_etc}/${FILES[3]}")" ]
	[ "$(stat -c %y "${mockdir_new_etc}/${FILES[3]}")" != "$(stat -c %y "${mockdir_syncpoint}/${FILES[3]}")" ]
	echo "# Verify that mode changes are detected and applied properly"
	[ "$(stat -c %a "${mockdir_new_etc}/${FILES[5]}")" = "777" ]

	rm -r "${mockdir_syncpoint}"
}

# bats test_tags=needroot
@test "File contents and properties (needs root)" {
	shopt -s globstar dotglob
	FILES=(File0.txt)
	createFilesIn "${mockdir_old_etc}" "${FILES[@]}"
	createFilesIn "${mockdir_new_etc}" "${FILES[@]}"
	createFilesIn "${mockdir_syncpoint}" "${FILES[@]}"
	syncTimestamps "${mockdir_old_etc}"/** "${mockdir_new_etc}"/** "${mockdir_syncpoint}"/**

	# Ownership - running in fakeroot because the user may not be part of any non-default group
	chown :audio "${mockdir_old_etc}/${FILES[0]}"
	touch --reference="${mockdir_syncpoint}/${FILES[0]}" "${mockdir_new_etc}/${FILES[0]}"

	run $totest --keep-syncpoint "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	echo "# Verify that owner changes are detected and applied properly"
	[ "$(stat -c %G "${mockdir_new_etc}/${FILES[0]}")" = "audio" ]

	rm -r "${mockdir_syncpoint}"
}

@test "Extended attributes" {
	FILES=(File0.txt File1.txt File2.txt)
	createFilesIn "${mockdir_old_etc}" "${FILES[@]}"
	createFilesIn "${mockdir_new_etc}" "${FILES[@]}"
	createFilesIn "${mockdir_syncpoint}" "${FILES[@]}"
	syncTimestamps "${mockdir_old_etc}"/** "${mockdir_new_etc}"/** "${mockdir_syncpoint}"/**

	setfattr --name="user.test" --value="test" "${mockdir_old_etc}/${FILES[0]}"
	mkdir "${mockdir_old_etc}/Dir0"
	setfattr --name="user.test" --value="test" "${mockdir_old_etc}/Dir0"
	setfacl -m u:lp:w "${mockdir_old_etc}/${FILES[2]}"

	run $totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	echo "# Verifying changing xattrs of 'File0.txt' are detected"
	[[ "${lines[*]}" == *'Extented attribute count changed: "'${mockdir_old_etc}'/./File0.txt'* ]]
	[[ "$(getfattr --no-dereference --dump --match='' "${mockdir_new_etc}/${FILES[0]}" 2>&1)" == *'user.test="test"'* ]]
	echo "# Verifying extended attributes are applied to directory"
	[[ "$(getfattr --no-dereference --dump --match='' "${mockdir_new_etc}/Dir0" 2>&1)" == *'user.test="test"'* ]]
	echo "# Verifying ACL is being preserved"
	[[ "$(getfattr -m - --dump "${mockdir_new_etc}/${FILES[2]}" 2>&1)" == *'system.posix_acl_access='* ]]
}

@test "Special file types" {
	for dir in "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"; do
		pushd "${dir}" >/dev/null
		touch File1
		ln -s File1 File2
		ln -s File1 File3
		ln -s File1 File4
		touch File5
		popd >/dev/null
	done
	syncTimestamps "${mockdir_old_etc}"/** "${mockdir_new_etc}"/** "${mockdir_syncpoint}"/**

	cd "${mockdir_old_etc}"
	echo "Test" > File1
	touch --no-dereference File3
	ln -sf File5 File4
	mknod File6 p
	cd - >/dev/null

	run $totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	echo "# Verifying symlink 'File2' wasn't changed"
	[[ "${lines[*]}" != *'"File2"'* ]]
	echo "# Verifying changing the modification time of symlink 'File3' is detected"
	[[ "${lines[*]}" == *'File changed: "'${mockdir_old_etc}'/./File3"'* ]]
	echo "# Verifying symlink 'File4' points to the new file"
	[[ "$(readlink --no-newline "${mockdir_new_etc}/File4")" == "File5" ]]
	echo "# Verifying copying FIFO file correctly"
	# Unsuppored for now
	#[ -p "${mockdir_new_etc}/File6" ]
	[[ "${lines[*]}" == *'Unsupported file type for file "./File6". Skipping...'* ]]
}

@test "Directory handling" {
	for dir in "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"; do
		mkdir "${dir}/Dir1"
		mkdir "${dir}/Dir2"
		touch "${dir}/Dir2/FileExisting"
		mkdir "${dir}/Dir3"
		# Dir4 does not exist during snapshot creation
		mkdir "${dir}/Dir5"
		mkdir "${dir}/Dir5/Subdir"
		touch "${dir}/Dir5/Subdir/FileInSubdir"
		mkdir "${dir}/Dir6"
		mkdir "${dir}/Dir6/Subdir"
		touch "${dir}/Dir6/Subdir/FileInSubdir"
		mkdir "${dir}/Dir7"
		echo old > "${dir}/Dir7/ChangeInOld"
		echo old > "${dir}/Dir7/ChangeInNew"
		echo old > "${dir}/Dir7/noChange"
	done
	syncTimestamps "${mockdir_old_etc}"/** "${mockdir_new_etc}"/** "${mockdir_syncpoint}"/**

	# Create a new directory in both old and new
	mkdir "${mockdir_old_etc}/Dir2/DirInOld"
	mkdir "${mockdir_new_etc}/Dir2/DirInNew"

	# Create same new file in both old and new
	echo -n "old" > "${mockdir_old_etc}/Dir3/FileInBoth"
	echo -n "new" > "${mockdir_new_etc}/Dir3/FileInBoth"

	# Create same new directory both in old and new
	mkdir "${mockdir_old_etc}/Dir4"
	chmod 700 "${mockdir_old_etc}/Dir4"
	touch "${mockdir_old_etc}/Dir4/SomeFile"
	mkdir "${mockdir_new_etc}/Dir4"
	chmod 770 "${mockdir_new_etc}/Dir4"
	touch "${mockdir_new_etc}/Dir4/SomeOtherFile"

	# Delete directory in new snapshot, while in old snapshot add an additional file
	rm -r "${mockdir_new_etc}/Dir5/Subdir"
	touch "${mockdir_old_etc}/Dir5/Subdir/NewFileInSubdir"

	# Delete directory in old snapshot, while in new snapshot add an additional file
	rm -r "${mockdir_old_etc}/Dir6"
	touch "${mockdir_new_etc}/Dir6/Subdir/NewFileInSubdir"

	# Verify touching a directory doesn't trigger overwrite
	echo -n new > "${mockdir_old_etc}/Dir7/ChangeInOld"
	echo -n new > "${mockdir_new_etc}/Dir7/ChangeInNew"
	touch "${mockdir_new_etc}"

	run $totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	echo "# Verify new directory in old Dir2 directory was copied over"
	[ -d "${mockdir_new_etc}/Dir2/DirInOld" ]
	echo "# Verify new directory in new Dir2 wasn't overwritten"
	[ -d "${mockdir_new_etc}/Dir2/DirInNew" ]
	echo "# Verify existing file in Dir2 wasn't touched"
	[[ "${lines[*]}" != *"'Dir2/FileExisting'"* ]]
	echo "# Verify content of file in new if file was modified in both"
	[ "$(cat "${mockdir_new_etc}/Dir3/FileInBoth")" = "new" ]
	echo "# Verify directory permissions of new if dir was created in both"
	[ "$(stat --format="%a" "${mockdir_new_etc}/Dir4")" = "770" ]
	echo "# Verify merged files if new dir was created in both"
	[ -e "${mockdir_new_etc}/Dir4/SomeFile" ]
	[ -e "${mockdir_new_etc}/Dir4/SomeOtherFile" ]
	echo "# Verify directory stays deleted if deleted in new"
	[ ! -e "${mockdir_new_etc}/Dir5/Subdir" ]
	echo "# Verify deleting dir in old doesn't delete new / changed contents in new"
	[ -e "${mockdir_new_etc}/Dir6/Subdir/NewFileInSubdir" ]
	[ ! -e "${mockdir_new_etc}/Dir6/Subdir/FileInSubdir" ]
	echo "# Verify modifying a directory doesn't affect contained files"
	[ "$(cat "${mockdir_new_etc}/Dir7/ChangeInOld")" = "new" ]
	[ "$(cat "${mockdir_new_etc}/Dir7/ChangeInNew")" = "new" ]
	[ "$(cat "${mockdir_new_etc}/Dir7/noChange")" = "old" ]
}

@test "File type changes" {
	for dir in "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"; do
		pushd "${dir}" >/dev/null
		touch File1
		touch File2
		touch File3
		touch File4
		touch File6
		mkdir File7
		touch File7/test
		ln -s File3 File8
		popd >/dev/null
	done
	syncTimestamps "${mockdir_old_etc}"/** "${mockdir_new_etc}"/** "${mockdir_syncpoint}"/**

	# Delete file in old, make sure it is also deleted in new
	rm "${mockdir_old_etc}/File1"

	# Delete file in new, make sure it stays deleted
	rm "${mockdir_new_etc}/File2"

	# File -> Symlink in old
	rm "${mockdir_old_etc}/File3"
	pushd "${mockdir_old_etc}" >/dev/null
	ln -s File4 File3
	popd >/dev/null

	# Nonexistent symlink
	ln -s broken "${mockdir_old_etc}/File5"

	# File -> directory in old
	rm "${mockdir_old_etc}/File6"
	mkdir "${mockdir_old_etc}/File6"
	touch "${mockdir_old_etc}/File6/test"

	# Directory -> file in old
	rm -r "${mockdir_old_etc}/File7"
	touch "${mockdir_old_etc}/File7"

	# Symlink -> Dir
	rm "${mockdir_old_etc}/File8"
	mkdir "${mockdir_old_etc}/File8"
	touch "${mockdir_old_etc}/File8/test"

	$totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	echo "# Verify file deleted in old also gets deleted in new (if not modified in new)"
	[ ! -e "${mockdir_new_etc}/File1" ]
	echo "# Verify file deleted in new stays deleted"
	[ ! -e "${mockdir_new_etc}/File2" ]
	echo "# Verify File3 got changed from file to symlink"
	[ -L "${mockdir_new_etc}/File3" ]
	echo "# Verfify copying up broken symlink"
	[ -L "${mockdir_new_etc}/File5" ]
	echo "# Verify File6 got changed from file to directory"
	[ -e "${mockdir_new_etc}/File6/test" ]
	echo "# Verify File7 got changed from directory to file"
	[ -f "${mockdir_new_etc}/File7" ]
	echo "# Verify File8 got changed from symlink to directory"
	[ -f "${mockdir_old_etc}/File8/test" ]
}

@test "Command line options" {
	# Dry run
	touch "${mockdir_old_etc}/File1"
	$totest --dry-run "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"
	$totest -n "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"
	echo "# Verify no files were changed during dry-run"
	[ ! -e "${mockdir_new_etc}/File1" ]
}

@test "etc.syncpoint is skipped" {
	mkdir -p "${mockdir_old_etc}/etc.syncpoint"
	echo bla > "${mockdir_old_etc}/etc.syncpoint/file"

	$totest "${mockdir_old_etc}" "${mockdir_new_etc}" "${mockdir_syncpoint}"

	[ ! -e "${mockdir_new_etc}/etc.syncpoint/file" ]
	[ ! -e "${mockdir_new_etc}/etc.syncpoint" ]
}

#cd /etc
#
## Step 1: Prepare environment
#if [ $1 = 0 ]; then
#	rm -rf existing_* insnap_* aftersnap_*
#	echo Test > existing_nochanges.txt
#	echo Test > existing_change_content.txt
#	echo Test > existing_attribute_changes.txt
#	echo Test > existing_xattr_changes.txt
#	mkdir -p existing_directory/existing_subdir
#	cd existing_directory
#	echo Test > existing_nochanges.txt
#	echo Test > existing_change_content.txt
#	echo Test > existing_attribute_changes.txt
#	echo Test > existing_xattr_changes.txt
#	cd existing_subdir
#	echo Test > existing_nochanges.txt
#	echo Test > existing_change_content.txt
#	echo Test > existing_attribute_changes.txt
#	echo Test > existing_xattr_changes.txt
#	cd ../..
#	mkdir -p existing_dir_for_move/dir1
#	touch existing_dir_for_move/file1.txt
#
#	sync
#	transactional-update grub.cfg
#fi
#
#if [ $1 = 1 ]; then
#	echo Hello > insnap_new_file.txt
#	echo "# `date`" >> existing_change_content.txt
#	chmod u+x existing_attribute_changes.txt
#	setfattr -n user.test -v "`date`" existing_xattr_changes.txt
#
#	# Create files for modification after snapshot creation
#	touch insnap_change_permissions.txt
#	touch insnap_change_xattrs.txt
#	mkdir -p insnap_new_dir_for_changes/will_be_merged
#	mkdir -p insnap_new_dir_for_changes/will_be_empty
#	mkdir -p insnap_new_dir_for_changes/was_empty_dir
#	mkdir -p insnap_new_dir_for_changes/change_dir_permissions
#	mkdir -p insnap_new_dir_for_changes/to_be_removed
#	mkdir -p insnap_new_dir_for_changes/change_xattr
#	mkdir -p insnap_new_dir_for_changes/nochange
#	touch insnap_new_dir_for_changes/file1.txt
#	touch insnap_new_dir_for_changes/will_be_merged/file1.txt
#	touch insnap_new_dir_for_changes/will_be_empty/file1.txt
#	touch insnap_new_dir_for_changes/change_dir_permissions/file1.txt
#	touch insnap_new_dir_for_changes/to_be_removed/file1.txt
#	touch insnap_new_dir_for_changes/change_xattr/file1.txt
#	touch insnap_new_dir_for_changes/nochange/file1.txt
#	mkdir -p insnap_new_dir_without_changes/somedir
#	touch insnap_new_dir_without_changes/somefile.txt
#	touch insnap_new_dir_without_changes/somedir/somefile.txt
#	mkdir -p insnap_dir_for_move/dir1
#	touch insnap_dir_for_move/file1.txt
#
#	# Create snapshot
#	sync
#	transactional-update grub.cfg
#	sync
#
#	touch aftersnap_new_file.txt
#	touch insnap_new_dir_for_changes/will_be_merged/file2.txt
#	rm insnap_new_dir_for_changes/will_be_empty/file1.txt
#	touch insnap_new_dir_for_changes/was_empty_dir/file2.txt
#	chmod g+w insnap_new_dir_for_changes/change_dir_permissions
#	chmod g+w insnap_change_permissions.txt
#	rm -r insnap_new_dir_for_changes/to_be_removed
#	setfattr -n user.test -v "`date`" insnap_new_dir_for_changes/change_xattr
#	setfattr -n user.test -v "`date`" insnap_change_xattrs.txt
#	mkdir -p aftersnap_new_dir/dir1
#	touch aftersnap_new_dir/file1.txt
#	mv existing_dir_for_move aftersnap_new_existing_location
#	mv insnap_dir_for_move aftersnap_new_insnap_location
#fi
