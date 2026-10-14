/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

#include "BlsEntry.hpp"
#include "Util.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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

namespace {

// Value of the first line "<key><separator><value>" of a file; empty if there is none.
std::string getValue(const std::filesystem::path &file, const std::string &key, const std::string &separators) {
    std::ifstream is(file);
    std::string line;
    while (std::getline(is, line)) {
        Util::trim(line);
        if (line.compare(0, key.size(), key) != 0 || line.size() <= key.size())
            continue;
        if (separators.find(line[key.size()]) == std::string::npos)
            continue;
        std::string value = line.substr(key.size() + 1);
        Util::trim(value);
        return value;
    }
    return "";
}

}

std::filesystem::path BlsEntry::find_default_entry(const std::filesystem::path &entries,
                                                   const std::filesystem::path &grubenv) {
    if (!std::filesystem::is_directory(entries))
        throw std::runtime_error{"BLS entries directory " + entries.string() + " does not exist."};

    // The entry that has been selected for the next boots
    if (std::filesystem::exists(grubenv)) {
        std::string saved = getValue(grubenv, "saved_entry", "=");
        if (!saved.empty()) {
            auto candidate = entries / (saved + ".conf");
            if (std::filesystem::is_regular_file(candidate))
                return candidate;
        }
    }

    struct Candidate {
        std::filesystem::path path;
        std::string sortKey;
        std::string version;
    };
    std::vector<Candidate> candidates;
    for (const auto &file : std::filesystem::directory_iterator(entries)) {
        if (!file.is_regular_file() || file.path().extension() != ".conf")
            continue;
        candidates.push_back({file.path(),
                              getValue(file.path(), "sort-key", " \t"),
                              getValue(file.path(), "version", " \t")});
    }
    if (candidates.empty())
        throw std::runtime_error{"No BLS entries found in " + entries.string() + "."};

    // Specification order: entries with a sort-key first, sort-key ascending,
    // then version descending; the file name decides in the end.
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.sortKey.empty() != b.sortKey.empty())
            return b.sortKey.empty();
        if (a.sortKey != b.sortKey)
            return a.sortKey < b.sortKey;
        int cmp = strverscmp(a.version.c_str(), b.version.c_str());
        if (cmp != 0)
            return cmp > 0;
        return a.path.filename() > b.path.filename();
    });
    return candidates.front().path;
}

}
