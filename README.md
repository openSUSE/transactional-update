# transactional-update
Transactional Updates with btrfs snapshots and zypper for openSUSE
and SUSE Linux Enterprise products

## Definition
A "transactional update" is a kind of update that:
* is atomic - the update does not influence your running system
* can be rolled back - if the upgrade fails or if the newer software version is not compatible with your infrastructure, you can quickly  restore the situation as it was before the upgrade.


## Prerequires
* SLES12 SP2, openSUSE Leap 42.2 or a current openSUSE Tumbleweed
  * The installation needs to be done with at least this versions, updating older versions to this version is not enough!
* btrfs with snapshots and rollback enabled by default
* snapper
* zypper
* this package

## How does this work?
The script creates at first a new snapshot of the system. Afterwards, this
snapshot is changed from read-only to read-write and several special
directories like /dev, /sys and /proc are mounted. zypper is called with
the --root option and the snapshot as argument.
If the update did succeed, we cleanup the snapshot, switch it to
read-only if the original root filesystem is read-only and make this
subvolume the new default. With the next boot, the updated snapshot is
the new default root filesystem.
If the update does not boot, the old root filesystem can be booted from the
grub2 menu and made as new default again, means a rollback to the old
state.

## Caveats
* The RPMs needs to be aware, that they are updated in a chroot environment
and not in the running system. This means:
  * %post install sections:
    *  if a pre/post install scripts modifies data not inside the snapshot, but a subvolume, it could be that either the data is not modified at all (not accessible) or that the running system breaks. This data modification has to happen a the first boot.
    * Don't restart services. This would interrupt the running system and the old daemon will be restarted, not the new one.
* RPM installs into directories, which are subvolumes and not accessible
* Strict seperation of applications, configuration and user data
* RPMs installed in /opt or /usr/local can be updated while the system is executing them.
