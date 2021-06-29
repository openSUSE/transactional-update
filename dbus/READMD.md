# Tukit DBUS Service
The tukitd service provides an DBUS interface which supports the same functionality as
the command line interface "tukit".

## Starting/Stopping Servive
### Starting
The service will be started automatically with the first DBUS call like:
> busctl introspect org.opensuse.tukit /org/opensuse/tukit

Sure, the normal `systemctl` is also working but not really needed:
> systemctl start tukitd.service
### Stopping
This `systemctl` call stops the service:
> systemctl stop tukitd.service

## DBUS API
The following sections describe each call which is available via DBUS.
The command line program `busctl` can be used for demonstrating the API calls
and showing the results.

### open
Creates a new transaction and returns its unique ID.

Parameter:
* base - Snapshot ID (string), "active" or "default". Base can be "active" to base the
  snapshot on the currently running system, "default" to the current default snapshot as a base
  (which may or may not be identical to "active") or any specific existing snapshot id.
  If base is not set (emtpy string) "active" will be used as the default.

Return value:
unique ID (string)

The return value will also be submitted via a signal.

`busctl` Example:

> busctl call org.opensuse.tukit /org/opensuse/tukit org.opensuse.tukit open "s" "default"

### call
Executes the given command from within the transaction's chroot environment, resuming the
transaction with the given ID; returns the exit status and the result of the given command.
But it will not delete the snapshot in case of errors.

Parameter:
* unique ID (string)
* command (string)
* kind (string) "synchron" or "asynchron".
  * "synchron". The command will be executed and the process waits for the result which will be
    returend.
  * "asynchron". The command will be executed in an own child process and the call will return
    at once.
    The result of the child process will be sent via a signal.

Return value while a `synchron` call:
* return value of the command (integer)
* output of the command (string)

Return value while a `asynchron` call:
* 0 (integer)
* fix string: "Results will be sent via signal."
The returned signal has the same content like the return value of `synchron` call.

`busctl` Example:

* `synchron` call with ID `536` and `ls` command::q
  > busctl call org.opensuse.tukit /org/opensuse/tukit org.opensuse.tukit call "sss" "536" "bash -c 'ls'" "asynchron"

* `asynchron` call with ID `536` and `ls` command:
  > busctl call org.opensuse.tukit /org/opensuse/tukit org.opensuse.tukit call "sss" "536" "bash -c 'ls'" "asynchron"
  
  The returned signal can be monitored by:
  > busctl --system --match "interface='org.opensuse.tukit'" monitor

### close
Closes the given transaction and sets the snapshot as the new default snapshot.

Parameter:
* unique ID (string)

Return value:
* return integer; 0 on success

`busctl` Example:

> busctl call org.opensuse.tukit /org/opensuse/tukit org.opensuse.tukit close "s" "420"

### abort
Deletes the given snapshot again.

Parameter:
* unique ID (string)

Return value:
* return integer; 0 on success

`busctl` Example:

> busctl call org.opensuse.tukit /org/opensuse/tukit org.opensuse.tukit abort "s" "420"