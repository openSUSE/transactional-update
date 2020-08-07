/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Class for adding additional files, links or directories into the update
  environment.
 */

#ifndef SUPPLEMENT_H
#define SUPPLEMENT_H

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class Supplements
{
public:
    Supplements() = default;
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

#endif // SUPPLEMENT_H
