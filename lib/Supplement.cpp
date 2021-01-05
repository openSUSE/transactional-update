/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Class for adding additional files, links or directories into the update
  environment.
 */

#include "Supplement.hpp"
#include "Log.hpp"
#include <exception>

namespace TransactionalUpdate {

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
        auto copyOptions = fs::copy_options::none;
        createDirs(file.parent_path());
        fs::path target = snapshot / file.relative_path();
        if (fs::is_regular_file(file))
            copyOptions = fs::copy_options::overwrite_existing;
        fs::copy(file, target, copyOptions);
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

} // namespace TransactionalUpdate
