/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2024 SUSE LLC */

/*
  Helper class
 */

#ifndef T_U_BLS_ENTRY_H
#define T_U_BLS_ENTRY_H

#include <string>
#include <utility>

namespace TransactionalUpdate {

struct BlsEntry {
    /// Parse a valid bls entry and return the pair of its kernel and initrd
    static std::pair<std::string, std::string> parse_bls_entry(std::string path);
};

} // namespace TransactionalUpdate

#endif // T_U_UTIL_H
