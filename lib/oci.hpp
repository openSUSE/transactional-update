/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  OCI backend selection; picks "systemd" or "podman", whichever is
  available, with "systemd" taking priority
 */

#ifndef T_U_OCI_H
#define T_U_OCI_H

#include "SnapshotManager.hpp"
#include <memory>

namespace TransactionalUpdate {

class Oci {
public:
    /**
     * @brief Checks whether an OCI-based snapshot manager (systemd or podman) is available
     */
    static bool available();

    /**
     * @brief Returns the OCI-based snapshot manager to use; "systemd" has priority over "podman"
     */
    static std::unique_ptr<SnapshotManager> get();
};

} // namespace TransactionalUpdate

#endif // T_U_OCI_H
