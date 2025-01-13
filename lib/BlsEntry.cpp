/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

#include "BlsEntry.hpp"
#include "Util.hpp"
#include <fstream>
#include <string>

namespace TransactionalUpdate {

std::pair<std::string, std::string> BlsEntry::parse_bls_entry(std::string path) {
    std::ifstream is(path);
    std::string str;
    std::string kernel;
    std::string initrd;
    while(getline(is, str))
    {
        Util::trim(str);
        const auto LINUX_NEEDLE = std::string{"linux "};
        const auto INITRD_NEEDLE = std::string{"initrd "};
        if (str.substr(0, LINUX_NEEDLE.size()) == LINUX_NEEDLE) {
            kernel = str.substr(LINUX_NEEDLE.size(), std::string::npos);
            Util::trim(kernel);
        } else if (str.substr(0, INITRD_NEEDLE.size()) == INITRD_NEEDLE){
            initrd = str.substr(INITRD_NEEDLE.size(), std::string::npos);
            Util::trim(initrd);
        }
    }
    return std::pair(kernel, initrd);
}

}
