/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Helper class
 */

#ifndef T_U_BLS_ENTRY_H
#define T_U_BLS_ENTRY_H

#include <filesystem>
#include <string>
#include <utility>

namespace TransactionalUpdate {

struct BlsEntry {
    /// Parse a valid bls entry and return the pair of its kernel and initrd
    static std::pair<std::string, std::string> parse_bls_entry(std::string path);

    /**
     * Find the entry in a directory of BLS entries that is booted by default
     * @param entries Directory containing the *.conf files (loader/entries)
     * @param grubenv GRUB environment file, may not exist
     * @return Path of the entry file
     * @throw std::runtime_error if there is no entry
     *
     * The entry GRUB has been told to boot (saved_entry in grubenv) is used if
     * it exists, otherwise the one that sorts first according to the BLS
     * specification (sort-key ascending, then version descending).
     */
    static std::filesystem::path find_default_entry(const std::filesystem::path &entries,
                                                    const std::filesystem::path &grubenv);
};

} // namespace TransactionalUpdate

#endif // T_U_UTIL_H
