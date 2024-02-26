# transactional-update
## Update the operating system in an atomic way
transactional-update provides an application and library to update a Linux operating system in a transactional way, i.e. the update will be performed in the background while the system continues running as it is. Only if the update was the successful as a whole the system will boot into the new snapshot.

It consists of the following components:

* **libtukit**: A generic library for atomic system updates.
* **tukit**: A command line application to access the library's functionality directly.
* **tukitd**: A D-Bus service to access the library's functionality.
* **transactional-update**: An (open)SUSE specific tukit wrapper to call common tasks, e.g. updating the system, installing RPM packages or refreshing the boot loader.

## What's a "transaction" when thinking about OS updates?
You may be familiar with the term "transaction" in the context of a classical database transaction: Only if all the single changes to the database tables could be applied successfully, then a final COMMIT will activate them, otherwise a ROLLBACK will just discard everything again.

In the context of operating system updates this is equivalent: Only if all updates (or other changes) could be applied successfully, then the system will switch into that new updated state. If any error occured - think about failed package post scripts or running out of disk space - the updated system will just be discarded again. All of this is happening in the background, i.e. the currently running system just continues to run all the time.

Or in a more formal way: A transactional update is an update that
* is atomic
  * Either fully applied, or not applied at all
  * Update does not influence the running system
* can be rolled back
  * A failed or incompatible update can be quickly discarded to restore the previous system condition

## Supported Systems
* **Snapper**: Uses snapper to create new snapshots (with support for BTRFS and bcachefs). In contrast to classical A/B partitioning mechanisms snapper can handle a large number of snapshots, and snapshot handling of BTRFS is very fast and space efficient.
* The API is intentionally generic and able to support a wider range of backends for atomic / transactional systems.

## How does this work?
With the Snapper implementation, first a new snapshot of the system is created. Afterwards, this snapshot is changed from read-only to read-write and several special directories such as /dev, /sys and /proc are mounted. The proposed change(s) will be performed in that snapshot in a chroot environment; on (open)SUSE systems for example *zypper* is wrapped into a *tukit* call to install, update or remove RPMs. If the update did succeed, then switch the snapshot to read-only (on ro systems) and make the subvolume the new default. On next boot, the system will boot the new snapshot. If the updated system should not boot (see also [health-checker](https://github.com/openSUSE/health-checker)), the system can simply be rolled back to the old snapshot.

## How to update an atomic system
Applications can integrate support directly (such as dnf or Cockpit - see [Known Users](#known-users) below), otherwise any command can be wrapped with `tukit execute` (e.g. zypper).

## User Documentation
* [The Transactional Update Guide](https://kubic.opensuse.org/documentation/transactional-update-guide/transactional-update.html) provides general information on the concept of transactional-update.
* [Various talks](https://media.ccc.de/search/?q=transactional-update) are available online.

## API Documentation
Developers that want to integrate support for transactional updates may be interested in the following official API ressources:
* C++: [Transaction.hpp](lib/Transaction.hpp) / [SnapshotManager.hpp](lib/SnapshotManager.hpp) / [Reboot.hpp](lib/Reboot.hpp)
* C: [libtukit.h](lib/Bindings/libtukit.h) (C binding - see the C++ header files for documentation)
* D-Bus interface: [org.opensuse.tukit.Transaction.xml](dbus/org.opensuse.tukit.Transaction.xml) / [org.opensuse.tukit.Snapshot.xml](dbus/org.opensuse.tukit.Snapshot.xml)

## Known Users
transactional-update was originally developed for the **openSUSE project** as the update mechanism for all transactional / read-only systems ([openSUSE MicroOS](https://microos.opensuse.org/), [SUSE Linux Enterprise Micro](https://www.suse.com/products/micro/), SUSE Linux Enterprise Server / openSUSE Leap / openSUSE Tumbleweed "Transactional Server" role) and is used as the update mechanism there.

Additionally the following components support transactional-update directly:
* **dnf**, Fedora's package management system, supports transactional systems directly via the [libdnf-plugin-txnupd](https://code.opensuse.org/microos/libdnf-plugin-txnupd) plugin (libtukit).
* **Cockpit** can update transactional systems via the [cockpit-tukit](https://github.com/openSUSE/cockpit-tukit) plugin (tukitd).
* **Salt** contains the [salt.modules.transactional\_update module](https://docs.saltproject.io/en/3004/ref/modules/all/salt.modules.transactional_update.html) module (transactional-update).
* **Ansible** also supports transactional-update via the the [community.general.zypper](https://docs.ansible.com/ansible/latest/collections/community/general/zypper_module.html) module (transactional-update).

## Caveats
* A transactional system needs strict separation of applications, configuration and user data. Data in /var must not be available during the update, as changes in there would necessarily modify the state of the currently running system. For better handling of package and admin configuration files see the UAPI Group's [Configuration File Specification](https://uapi-group.org/specifications/specs/configuration_files_specification/).
