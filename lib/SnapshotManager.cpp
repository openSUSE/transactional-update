/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Factory / interface class for snapshot management;
  implementations can be found in the "Snapshot" directory
 */

#include "Configuration.hpp"
#include "Log.hpp"
#include "Snapshot/Snapper.hpp"
#include "Snapshot/Podman.hpp"
using namespace std;

namespace TransactionalUpdate {

unique_ptr<SnapshotManager> SnapshotFactory::get() {
    auto sm = config.get("SNAPSHOT_MANAGER");
    if (sm == "auto") {
        if (filesystem::exists("/usr/bin/snapper"))
            sm = "snapper";
        else if (filesystem::exists("/usr/bin/podman"))
            sm = "podman";
        else
            throw runtime_error{"No snapshot manager found using 'auto'."};
    }

    tulog.info("Using '" + sm + "' as snapshot manager.");
    if (sm == "snapper") {
        return make_unique<Snapper>();
    } else if (sm == "podman") {
        return make_unique<Podman>();
    } else {
        throw runtime_error{"Unsupported snapshot manager '" + sm + "'."};
    }
}

} // namespace TransactionalUpdate
