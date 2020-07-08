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

#include "Supplement.h"
#include "Log.h"
#include <exception>

Supplements::Supplements(fs::path snapshot):
    snapshot{snapshot}
{
}

// Creates temporary directories that do not exist in the chroot environment and makes sure
// those directories are deleted in the end by adding them to the list of temporary files
void Supplements::createDirs(fs::path dir) {
    fs::path stump{snapshot};
    for (auto& component: dir.relative_path()) {
        stump /= component;
        if (! fs::exists(stump)) {
            fs::create_directories(stump);
            supplementalFiles.push_back(std::move(stump));
            break;
        }
    }
    fs::create_directories(snapshot / dir.relative_path());
}

void Supplements::addDir(fs::path dir) {
    fs::path target = snapshot / dir.relative_path();
    if (! fs::exists(target)) {
        createDirs(dir);
    }
}

void Supplements::addFile(fs::path file) {
    if (fs::exists(file)) {
        createDirs(file.parent_path());
        fs::path target = snapshot / file.relative_path();
        fs::copy(file, target);
        supplementalFiles.push_back(std::move(target));
    }
}

void Supplements::addLink(fs::path source, fs::path target) {
    if (fs::exists(snapshot / source.relative_path()) && ! fs::exists(snapshot / target.relative_path())) {
        createDirs(target.parent_path());
        target = snapshot / target.relative_path();
        if (fs::is_directory(source)) {
            fs::create_directory_symlink(source, target);
        } else {
            fs::create_symlink(source, target);
        }
        supplementalFiles.push_back(std::move(target));
    }
}

void Supplements::cleanup() {
    for (auto it = supplementalFiles.rbegin(); it != supplementalFiles.rend(); ++it) {
        try {
            tulog.debug("Removing supplemental file ", *it);
            fs::remove_all(*it);
        }  catch (const std::exception &e) {
            tulog.error("ERROR: Removing supplemental file failed: ", e.what());
        }
    }
}
