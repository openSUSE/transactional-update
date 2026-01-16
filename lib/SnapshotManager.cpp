/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Factory / interface class for snapshot management;
  implementations can be found in the "Snapshot" directory
 */

#include "Snapshot/ContainerSnap.hpp"
#include "Snapshot/Snapper.hpp"
using namespace std;

namespace TransactionalUpdate {

// TODO: Make configurable to be able to force a certain implementation
unique_ptr<SnapshotManager> SnapshotFactory::get() {
    if (filesystem::exists("/usr/bin/snapper")) {
        return make_unique<Snapper>();
    } else if (filesystem::exists("/usr/bin/container-snap")) {
      return make_unique<ContainerSnap>();
    } else {
        throw runtime_error{"No supported environment found."};
    }
}

} // namespace TransactionalUpdate
