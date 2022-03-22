# Tukit DBUS Service
The tukitd service provides an DBUS interface which supports the same functionality as
the command line interface "tukit".

## Starting/Stopping Servive
### Starting
The service will be started automatically with the first DBUS call, e.g.
> busctl introspect org.opensuse.tukit /org/opensuse/tukit/Transaction

The status can also be checked via
> systemctl start tukitd.service
### Stopping
This `systemctl` call stops the service:
> systemctl stop tukitd.service

## DBUS API
The following sections describe each call which is available via DBUS.
The command line program `busctl` can be used for demonstrating the API calls
and showing the results.

### Transaction

#### open
Creates a new transaction and returns its unique ID.

Parameter:
* base - Snapshot ID (string), "active" or "default". Base can be "active" to base the
  snapshot on the currently running system, "default" to the current default snapshot as a base
  (which may or may not be identical to "active") or any specific existing snapshot id.
  If base is not set (emtpy string) "active" will be used as the default.

Return value:
* unique ID (string)

Signal:
* TransactionOpened

`busctl` example:

> busctl call org.opensuse.tukit /org/opensuse/tukit/Transaction org.opensuse.tukit.Transaction open "s" "default"

### call
Executes the given command from within the transaction's **chroot environment**, resuming the
transaction with the given ID; returns the exit status and the result of the given command.
In case of errors the snapshot will not be deleted.

Parameter:
* unique ID (string)
* command (string)

Return value:
None

Signal:
* CommandExecuted - As this may be a long running operation, the results of the command are
  returned via signal only.

`busctl` example:

* call `ls` in open transaction with ID `536`:
  > busctl call org.opensuse.tukit /org/opensuse/tukit/Transaction org.opensuse.tukit.Transaction call "ss" "536" "bash -c 'ls'"
  
  The returned signal can be monitored by:
  > busctl --system --match "path\_namespace='/org/opensuse/tukit'" monitor

### callext
Executes the given command. The command is **not** executed in a **chroot environment**, but instead runs
in the current system, replacing '{}' with the mount directory of the given snapshot.
In case of errors the snapshot will not be deleted.

Parameter:
* unique ID (string)
* command (string)

Return value:
None

Signal:
* CommandExecuted - As this may be a long running operation, the results of the command are
  returned via signal only.

`busctl` example:

* copy file from active system into transaction with ID `536`:
  > busctl call org.opensuse.tukit /org/opensuse/tukit/Transaction org.opensuse.tukit.Transaction callext "ss" "536" "bash -c 'mv /tmp/mylib {}/usr/lib'"

  The returned signal can be monitored by:
  > busctl --system --match "path\_namespace='/org/opensuse/tukit'" monitor

### close
Closes the given transaction and sets the snapshot as the new default snapshot.

Parameter:
* unique ID (string)

Return value:
* return integer; 0 on success

`busctl` Example:

> busctl call org.opensuse.tukit /org/opensuse/tukit/Transaction org.opensuse.tukit.Transaction close "s" "420"

### abort
Deletes the given snapshot.

Parameter:
* unique ID (string)

Return value:
* return integer; 0 on success

`busctl` Example:

> busctl call org.opensuse.tukit /org/opensuse/tukit/Transaction org.opensuse.tukit.Transaction abort "s" "420"
