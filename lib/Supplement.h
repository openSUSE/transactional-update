/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Class for adding additional files, links or directories into the update
  environment.
 */

#ifndef T_U_SUPPLEMENT_H
#define T_U_SUPPLEMENT_H

#include <filesystem>
#include <string>
#include <vector>

namespace TransactionalUpdate {

namespace fs = std::filesystem;

class Supplements
{
public:
    Supplements() = default;
    virtual ~Supplements() = default;
    Supplements(fs::path snapshot);
    void addDir(fs::path dir);
    void addFile(fs::path file);
    void addLink(fs::path source, fs::path target);
    void cleanup();
protected:
    fs::path snapshot;
    std::vector<fs::path> supplementalFiles;
    void createDirs(fs::path dir);
};

} // namespace TransactionalUpdate

#endif // T_U_SUPPLEMENT_H
