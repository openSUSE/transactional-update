# Tukit plugins

## Motivation

Sometimes it is useful to inspect the content of a transaction in
certain points.  For example, before `transactional-update dup` we
want to add new files into the system, and after the update we want to
collect the list of new packages.

With a plugin system we can provide scripts that can be executed
before or after each action supported by `transactional-update`, like
package installation, creation of new `initrd`, etc.  This can work
for most of the use cases, but there are certain tasks that cannot be
done at this level.

One example of those tasks is inspecting the `/run` directory after a
full system upgrade to look for files that can signalize a condition,
like the one that will trigger the `initrd` creation in a
post-transaction scriptlet.  The `/run` directory that is alive inside
the new transaction is different from the one running in the host, but
in both cases it is a `tmpfs` filesystem.  This means that every time
that it is unmounted we lost the information stored in there.  Sadly,
every time that `transactional-update` calls `tukit` to realize an
action inside the snapshot, those directories are mounted and
unmounted.

The solution for this is to have the plugins at the `tukit` level.

## Directories and shadowing

The plugins can be installed in two different places:

* `/usr/lib/tukit/plugins`: place for plugins that comes from packages
* `/etc/tukit/plugins`: for user defined plugins

The plugins in `/etc` can shadow the ones from `/usr` using the same
name.  For example, if the plugin `get_status` is in both places with
the executable attribute, `tukit` will use the code from `/etc`,
shadowing the one from the packages.

One variation of shadowing is when the plugin in `/etc` is a soft link
to `/dev/null`.  This will be used as a mark to completely disable
this plugin, and would not be called by `tukit`.

The plugins in `/etc` will be called before the ones in `/usr` but the
user should not depend on the calling order.

## Stages

The actions are based on the low-level API of libtukit, not of the
user API level.  This means that some verbs like `tukit execute <cmd>`
will be presented as several of those low level actions: create
snapshot, execute command, keep snapshot.

Some actions will trigger a plugin call before and after the action
itself, depending if it makes sense in the context of this action.

The next table summarizes the action, the stage and different
parameters sent to the plugin.

| Action   | Stage | Parameters                        | Notes |
|----------|-------|-----------------------------------|-------|
| init     | -pre  |                                   |       |
|          | -post | path, snapshot\_id                |       |
| resume   | -pre  | snapshot\_id                      |       |
|          | -post | path, snapshot\_id                |       |
| execute  | -pre  | path, snapshot\_id, action params |       |
|          | -post | path, snapshot\_id, action params |       |
| callExt  | -pre  | path, snapshot\_id, action params | [1]   |
|          | -post | path, snapshot\_id, action params |       |
| finalize | -pre  | path, snapshot\_id                |       |
|          | -post | snapshot\_id, [discarded]         | [2]   |
| abort    | -post | snapshot\_id                      | [3]   |
| keep     | -pre  | path, snapshot\_id                |       |
|          | -post | snapshot\_id                      |       |
| reboot   | -pre  |                                   |       |

[1] The {} placeholder gets expanded in the arguments passed to the
plugin

[2] If the snapshot is discarded, the second parameter for the -post
is "discarded"

[3] abort-pre cannot be captured from the libtukit level


## Example

```bash
#!/bin/bash

exec_pre() {
    local path="$1"; shift
    local snapshot_id="$1"; shift
    local cmd="$@"

    # The live snapshot is in "$path", and the future closed snapshot in
    # "/.snapshots/${snapshot_id}/snapshot

    mkdir -p /var/lib/report
    echo "${snapshot_id}: $cmd" >> /var/lib/report/all_commands
}

declare -A commands

commands['execute-pre']=exec_pre
commands['callExt-pre']=exec_pre

cmd="$1"
shift
[ -n "$cmd" ] || cmd=help
if [ "${#commands[$cmd]}" -gt 0 ]; then
    ${commands[$cmd]} "$@"
fi
```
