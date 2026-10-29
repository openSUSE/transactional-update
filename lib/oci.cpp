/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  OCI backend selection; picks "mstack" or "podman", whichever is
  available, with "mstack" taking priority
 */

#include "oci.hpp"
#include "Log.hpp"
#include "Snapshot/Podman.hpp"
#include "Snapshot/Mstack.hpp"
#include <stdexcept>

namespace TransactionalUpdate {

bool Oci::available() {
    return Mstack::available() || Podman::available();
}

std::unique_ptr<SnapshotManager> Oci::get() {
    if (Mstack::available()) {
        tulog.info("Using 'mstack' as OCI snapshot manager.");
        return std::make_unique<Mstack>();
    } else if (Podman::available()) {
        tulog.info("Using 'podman' as OCI snapshot manager.");
        return std::make_unique<Podman>();
    } else {
        throw std::runtime_error{"No OCI-based snapshot manager (mstack or podman) found."};
    }
}

} // namespace TransactionalUpdate
