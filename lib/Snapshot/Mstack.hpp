/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  systemd-mstack backend for snapshot handling
 */

#ifndef T_U_MSTACK_H
#define T_U_MSTACK_H

#include "Snapper.hpp"

namespace TransactionalUpdate {

class Mstack: public Snapper {
public:
    ~Mstack() = default;

    // Snapshot
    Mstack(std::string snap): Snapper(snap) {};

    // SnapshotManager
    Mstack(): Snapper("") {};
    std::unique_ptr<Snapshot> create(std::string base, std::string description) override;

    /**
     * @brief Checks whether the tools required by this snapshot manager are installed
     */
    static bool available();
};

} // namespace TransactionalUpdate

#endif // T_U_MSTACK_H
