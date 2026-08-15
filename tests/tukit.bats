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

	# Options redirecting all system-wide paths into the test's temp directory
	tuopts=(-o SNAPSHOT_MANAGER=directory -o SNAPSHOTS_DIR="${snapdir}"
		-o LOCKFILE="${lockfile}" -o STATE_FILE="${statefile}" -o MOUNT_DEV=false)

	# tukit bind mounts /proc and /sys recursively. The kernel locks the
	# submounts of such a recursive bind mount to their parent when they were
	# inherited into a user namespace, so they cannot be unmounted individually
	# again, which would make the teardown log dozens of errors.
	unshare=(unshare --map-root-user --mount --pid --fork --net --mount-proc
		sh -c 'mount -t sysfs sysfs /sys 2>/dev/null; exec "$@"' sh)

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

	echo -n "1" > "${snapdir}/1/snapshot/base"

	# Version 2: Create reference state of current system (which hopefully is some *SUSE system)
#	mkdir -p "${snapdir}/1"
#	ret=0
#	rsync --quiet --archive --chmod=Du+w --one-file-system / "${snapdir}/1/snapshot" || ret="$?"
#	if [ "${ret}" -ne 0 -a "${ret}" -ne 23 ]; then
#		return "${ret}"
#	fi

	cat > "${snapdir}/1/info" <<-EOF
		description=base
		date=2026-01-01 00:00:00
		in-progress=no
		read-only=no
	EOF
	echo 1 > "${snapdir}/current"
	echo 1 > "${snapdir}/default"
}

teardown_file() {
	rm -rf "${TESTDIR}"
}

tukit() {
	if [ "${EUID}" -eq 0 ]; then
		run "${tukit}" "${tuopts[@]}" "$@"
	else
		run "${unshare[@]}" "${tukit}" "${tuopts[@]}" "$@"
	fi
	echo "${output}"
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
