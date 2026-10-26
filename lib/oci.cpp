/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  OCI backend selection; picks "systemd" or "podman", whichever is
  available, with "systemd" taking priority
 */

#include "oci.hpp"
#include "Log.hpp"
#include "Snapshot/Podman.hpp"
#include "Snapshot/Systemd.hpp"
#include <stdexcept>

namespace TransactionalUpdate {

bool Oci::available() {
    return Systemd::available() || Podman::available();
}

std::unique_ptr<SnapshotManager> Oci::get() {
    if (Systemd::available()) {
        tulog.info("Using 'systemd' as OCI snapshot manager.");
        return std::make_unique<Systemd>();
    } else if (Podman::available()) {
        tulog.info("Using 'podman' as OCI snapshot manager.");
        return std::make_unique<Podman>();
    } else {
        throw std::runtime_error{"No OCI-based snapshot manager (systemd or podman) found."};
    }
}

} // namespace TransactionalUpdate
