/*
  Class for adding additional files, links or directories into the update
  environment.

  Copyright (c) 2016 - 2020 SUSE LLC

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
