# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

# Test tukit using the "directory" backend: this backend can be run
# unprivileged in a user namespace using unshare so that it can be called
# as part of the package build in OBS.

setup() {
	TESTDIR="${HOME}/testdir"
	mkdir -p "${TESTDIR}"
	tukit="../tukit/tukit"
	snapdir="${TESTDIR}/snapshots"
	lockfile="${TESTDIR}/tukit.lock"
	statefile="${TESTDIR}/transactional-update.state"
	extraunsharemounts=""

	# Options redirecting all system-wide paths into the test's temp directory
	tuopts=(-o SNAPSHOT_MANAGER=directory -o SNAPSHOTS_DIR="${snapdir}"
		-o LOCKFILE="${lockfile}" -o STATE_FILE="${statefile}" -o MOUNT_DEV=false)
}

setup_file() {
	setup

	# Version 1: Create minimal system
	mkdir -p "${snapdir}/1/snapshot"/{usr,etc,var/lib,var/cache,var/log}
	mkdir -p "$(dirname "${statefile}")"
	# Mirror the state file's directory inside the snapshot, so
	# StateStore::persist() can copy the state file into new snapshots.
	mkdir -p "${snapdir}/1/snapshot/$(dirname "${statefile#/}")"
	# Transaction::closeSnapshot() reads the snapshot's fstab to determine
	# whether the root file system - and thus the snapshot - is read-only.
	echo "/dev/vda1 / btrfs defaults 0 0" > "${snapdir}/1/snapshot/etc/fstab"


	# Version 2: Create reference state of current system (which hopefully is some *SUSE system)
#	mkdir -p "${snapdir}/1"
#	ret=0
#	rsync --quiet --archive --chmod=Du+w --one-file-system / "${snapdir}/1/snapshot" || ret="$?"
#	if [ "${ret}" -ne 0 -a "${ret}" -ne 23 ]; then
#		return "${ret}"
#	fi

	# Set meta information
	cat > "${snapdir}/1/info" <<-EOF
		description=base
		date=2026-01-01 00:00:00
		in-progress=no
		read-only=no
	EOF
	echo -n "1" > "${snapdir}/1/snapshot/base"
	echo -n "1" > "${snapdir}/current"
	echo -n "1" > "${snapdir}/default"

	# Set up some files for the test cases
	echo "File 1" > "${snapdir}/1/snapshot/etc/file1.txt"
}

teardown_file() {
	rm -rf "${TESTDIR}"
}

tukit() {
	# tukit bind mounts /proc and /sys recursively. The kernel locks the
	# submounts of such a recursive bind mount to their parent when they were
	# inherited into a user namespace, so they cannot be unmounted individually
	# again, which would make the teardown log dozens of errors.
	unshare=(unshare --map-root-user --mount --pid --fork --net --mount-proc
		sh -c 'mount -t sysfs sysfs /sys; '" ${extraunsharemounts} "' exec "$@"' sh)

	run "${unshare[@]}" "${tukit}" "${tuopts[@]}" "$@"
	echo "${output}"
}

getid() {
	NEWID=$(echo "${output}" | grep -e "^ID:" | cut -d " " -f 2-)
}

@test "tukit open (rw)" {
	tukit open

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Checking for new snapshot ID"
	[[ "${output}" == *"ID: 2"* ]]
	echo "# Verifying in-progress status is set"
	grep -qx "in-progress=yes" "${snapdir}/2/info"
}

@test "tukit callext" {
	tukit callext 2 touch "{}/usr/newfile"

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Verifying creation of file in new snapshot"
	[ -e "${snapdir}/2/snapshot/usr/newfile" ]
	echo "# Verifying file was not created in old snapshot"
	[ ! -e "${snapdir}/1/snapshot/usr/newfile" ]
	echo "# Verifying file did not leak into the host system"
	[ ! -e "/usr/newfile" ]
}

@test "tukit call" {
	skip "This test would need a full system in the snapshot"
	tukit call 2 touch "/usr/anotherfile"

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Verifying creation of file in new snapshot"
	[ -e "${snapdir}/2/snapshot/usr/anotherfile" ]
	echo "# Verifying file was not created in old snapshot"
	[ ! -e "${snapdir}/1/snapshot/usr/anotherfile" ]
	echo "# Verifying file did not leak into the host system"
	[ ! -e "/usr/anotherfile" ]
}

@test "tukit close" {
	tukit close 2

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Verifying snapshot has been set as new default"
	[ "$(cat "${snapdir}/default")" = "2" ]
	echo "# Verifying shapshot was closed"
	grep -qx "in-progress=no" "${snapdir}/2/info"
}

@test "tukit abort" {
	tukit open
	[ "${status}" -eq 0 ]
	[ -d "${snapdir}/3" ]
	tukit abort 3

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Verifying the snapshot is deleted"
	[ ! -e "${snapdir}/3" ]
	echo "# Verifying the default snapshot is unchanged"
	[ "$(cat "${snapdir}/default")" = "2" ]
}

@test "tukit snapshots" {
	tukit -q -f number,default,active,description snapshots

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Verifying snapshots are listed correctly"
	[[ "${lines[0]}" == "1"*"no"*"yes"*"base"* ]]
	[[ "${lines[1]}" == "2"*"yes"*"no"*"Snapshot Update of #1"* ]]
	[[ -z "${lines[2]}" ]]
}

@test "Open with --discard and close without any changes" {
	tukit --discard open && getid
	tukit close "${NEWID}"

	echo "Verifying the snapshot got deleted"
	[ ! -e "${snapdir}/${NEWID}" ]
}

@test "Create file in rootfs with --discard (rw)" {
	tukit -c1 --discard open && getid
	tukit callext "${NEWID}" touch "{}/usr/file1.txt"
	tukit close "${NEWID}"

	echo "# Verifying the new snapshot and file still exists"
	[ -e "${snapdir}/${NEWID}/snapshot/usr/file1.txt" ]
}

@test "Delete file in /etc with --discard (rw)" {
	tukit --discard open && getid
	tukit callext "${NEWID}" rm "{}/etc/file1.txt"
	tukit close "${NEWID}"

	echo "# Checking return value (if non-zero the sync probably tried to delete the file in the running system)"
	[ "${status}" -eq 0 ]
	echo "# Verifying the file got deleted"
	[ ! -e "${snapdir}/${NEWID}/snapshot/etc/file1.txt" ]
	echo "# Verifying the snapshot still exists"
	[ -e "${snapdir}/${NEWID}/snapshot" ]
}

@test "Create file in /etc after apply with --discard" {
	extraunsharemounts="mount --bind /etc /etc;"
	tukit -c1 --discard open && getid
	tukit callext "${NEWID}" touch "{}/etc/file1.txt"
	tukit close "${NEWID}"

	echo "# Verifying the new snapshot and file still exists"
	[ -e "${snapdir}/${NEWID}/snapshot/etc/file1.txt" ]
}

@test "Conversion to a read-only system" {
	tukit open && getid
	gawk -i inplace '$2 == "/" { sub(/\<ro(=[^\>]+)?\>/,"",$4); $4 = $4 ",ro=vfs"; sub(/\<defaults,/,"",$4); } { print }' "${snapdir}/${NEWID}/snapshot/etc/fstab"
	echo "/etc /etc none bind,x-initrd.mount 0 0" >> "${snapdir}/${NEWID}/snapshot/etc/fstab"
	tukit close "${NEWID}"
	[ "${status}" -eq 0 ]

	echo "# Verifying snapshot was set to read-only"
	grep -qx "read-only=yes" "${snapdir}/${NEWID}/info"

	# Set read-only snapshot as base for all following tests
	echo -n "${NEWID}" > "${snapdir}/current"
}

@test "Delete file in /etc with --discard (ro)" {
	extraunsharemounts="mount --bind /etc /etc;"
	tukit --discard open && getid
	tukit callext "${NEWID}" rm "{}/etc/file1.txt"

	echo "# Checking return value"
	[ "${status}" -eq 0 ]
	echo "# Verifying discard file is still there"
	[ -e "${snapdir}/${NEWID}/snapshot/"*discard* ]

	tukit close "${NEWID}" || true

	echo "# Verifying the snapshot got deleted"
	[ ! -e "${snapdir}/${NEWID}/snapshot" ]
	echo "# Verifying it tries to merge the file back to the current system"
	[[ "${output}" == *"No changes to the root file system - discarding snapshot."* ]]
	echo "# Verifying the file gets deleted in the parent fs"
	# Of course it's not intended that the test system will actually be modified, so
	# verify it at least tries to do so, but is prevented by the unshare sandbox.
	[[ "${output}" == *" delete_file: "*" failed: Permission denied"* ]]
}

@test "Create file in /etc after apply with --discard (ro)" {
	extraunsharemounts="mount --bind /etc /etc;"
	tukit open && getid
	BASEID="${NEWID}"
	tukit close "${BASEID}"
	tukit --discard --continue open && getid
	tukit callext "${NEWID}" touch "{}/etc/file1.txt"
	tukit close "${NEWID}"

	echo "# Verifying the file has been merged into the previous snapshot"
	[ -e "${snapdir}/${BASEID}/snapshot/etc/file1.txt" ]
	echo "# Verifying the new snapshot was deleted"
	[ ! -e "${snapdir}/${NEWID}/snapshot" ]
}
