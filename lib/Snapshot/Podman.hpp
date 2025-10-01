/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Podman backend for snapshot handling
 */

#ifndef T_U_PODMAN_H
#define T_U_PODMAN_H

#include "Snapper.hpp"

namespace TransactionalUpdate {

class Podman: public Snapper {
public:
    ~Podman() = default;

    // Snapshot
    Podman(std::string snap): Snapper(snap) {};

    // SnapshotManager
    Podman(): Snapper("") {};
    std::unique_ptr<Snapshot> create(std::string base, std::string description) override;
};

} // namespace TransactionalUpdate

#endif // T_U_PODMAN_H
