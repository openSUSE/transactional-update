/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Factory / interface class for snapshot management;
  implementations can be found in the "Snapshot" directory
 */

#include "Configuration.hpp"
#include "Snapshot/Snapper.hpp"
#include "Snapshot/Podman.hpp"
using namespace std;

namespace TransactionalUpdate {

unique_ptr<SnapshotManager> SnapshotFactory::get() {
    string sm = config.get("SNAPSHOT_MANAGER");
    if (sm == "snapper" && filesystem::exists("/usr/bin/snapper")) {
        return make_unique<Snapper>();
    } else if (sm == "podman" && filesystem::exists("/usr/bin/podman")) {
        return make_unique<Podman>();
    } else {
        throw runtime_error{"No supported environment found."};
    }
}

} // namespace TransactionalUpdate
