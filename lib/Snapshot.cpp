/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Factory / interface class for snapshot generation implementations;
  implementations can be found in the "Snapshot" directory
 */

#include "Snapshot.h"
#include "Snapshot/Snapper.h"
using namespace std;

// TODO: Make configurable to be able to force a certain implementation
unique_ptr<Snapshot> SnapshotFactory::get() {
    if (filesystem::exists("/usr/bin/snapper")) {
        return make_unique<Snapper>();
    } else {
        throw runtime_error{"No supported environment found."};
    }
}
