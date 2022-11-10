# transactional-update
## Update the operating system in an atomic way
This project provides an application and library to update a Linux operating system in a transactional way, i.e. the update will be performed in the background while the system continues running as it is. Only if the update was the successful the system will boot into the new snapshot.

Originally developed for the openSUSE project as the update mechanism for all transactional / read-only systems (openSUSE MicroOS, SLE Micro, SLES / openSUSE Leap / openSUSE Tumbleweed "Transactional Server" role) the original *transactional-update* Bash script has since been split into several components:

* **libtukit**: A generic library for atomic system updates.
* **tukit**: A command line application to access the library functionality.
* **tukitd**: A D-Bus service to access the library functionality.
* **transactional-update**: An (open)SUSE specific tukit wrapper to call common tasks, e.g. updating the system or installing the boot loader.

## Supported Systems
Currently only systems running **Btrfs with Snapper** are supported, however the API is intentionally generic and able to support a wider range of backends for atomic / transactional systems.

## How does this work?
First a new snapshot of the system is created. Afterwards, this snapshot is changed from read-only to read-write and several special directories such as /dev, /sys and /proc are mounted. The proposed change(s) can the be performed in that snapshot in a chroot environment, on (open)SUSE systems for example zypper is wrapped in a *tukit* call to install, update or remove RPMs. If the update did succeed switch the snapshot to read-only (on ro systems) and make the subvolume the new default. On next boot, the system will boot the new snapshot. If the updated system should not boot (see also [health-checker](https://github.com/openSUSE/health-checker)), the system can simply be rolled back to the old snapshot.

## How to update an atomic system
Applications can integrate support directly (such as dnf or Cockpit - see below), otherwise any command can be wrapped with `tukit execute` (e.g. zypper).

## User Documentation
* [The Transactional Update Guide](https://kubic.opensuse.org/documentation/transactional-update-guide/transactional-update.html) provides general information on the concept of transactional-update.

## API Documentation
Developers that want to integrate support for transactional updates may be interested in the following official API ressources:
* C++: [Transaction.hpp](lib/Transaction.hpp) / [SnapshotManager](lib/SnapshotManager.hpp)
* C: [libtukit.h](lib/Bindings/libtukit.h) (C binding - see the C++ header files for documentation)
* D-Bus interface: [org.opensuse.tukit.Transaction.xml](dbus/org.opensuse.tukit.Transaction.xml) / [org.opensuse.tukit.Snapshot.xml](dbus/org.opensuse.tukit.Snapshot.xml)

## Known users
* **dnf**, Fedora's package management system, supports transactional systems directly via the [libdnf-plugin-txnupd](https://code.opensuse.org/microos/libdnf-plugin-txnupd) plugin (libtukit).
* **Cockpit** can update transactional systems via the [cockpit-tukit](https://github.com/openSUSE/cockpit-tukit) plugin (tukitd).
* **Salt** contains the [salt.modules.transactional\_update module](https://docs.saltproject.io/en/3004/ref/modules/all/salt.modules.transactional_update.html) module (transactional-update).
* **Ansible** also supports transactional-update via the the [community.general.zypper](https://docs.ansible.com/ansible/latest/collections/community/general/zypper_module.html) module (transactional-update).

## Caveats
* A transactional system needs strict separation of applications, configuration and user data. Data in /var must not be available during the update, as changes in there would necessarily modify the state of the currently running system.
