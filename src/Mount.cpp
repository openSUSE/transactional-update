/*
  Wrapper for libmount

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

#include "Mount.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

Mount::Mount(std::string target, unsigned long flags)
    : mnt_cxt{NULL}, mnt_table{mnt_new_table()}, mnt_fs{NULL},
      target{target}, flags{flags}
{
}

Mount::Mount(Mount&& other) noexcept
    : mnt_cxt{NULL}, mnt_table{NULL}, mnt_fs{NULL}
{
    std::swap(mnt_table, other.mnt_table);
    std::swap(mnt_fs, other.mnt_fs);
    std::swap(mnt_cxt, other.mnt_cxt);
    std::swap(target, other.target);
    std::swap(flags, other.flags);
}

Mount::~Mount() {
    int mountStatus = 0;
    if (mnt_cxt && mnt_fs && mnt_context_is_fs_mounted(mnt_cxt, mnt_fs, &mountStatus) != 0)
        std::cerr << "Error determining mount status of " << target << std::endl;
    if (mountStatus == 1) {
        std::cout << "Unmounting " << target << "..." << std::endl;
        mnt_reset_context(mnt_cxt);
        if (mnt_context_set_fs(mnt_cxt, mnt_fs) != 0) {
            std::cerr << "Setting umount context for '" + target + "' failed." << std::endl;
        }
        int rc = mnt_context_umount(mnt_cxt);
        char buf[BUFSIZ] = { 0 };
        rc = mnt_context_get_excode(mnt_cxt, rc, buf, sizeof(buf));
        if (*buf)
                std::cerr << "Error unmounting '" + target + "': " + buf << std::endl;
    }

    mnt_free_context(mnt_cxt);
    mnt_unref_fs(mnt_fs);
    mnt_free_table(mnt_table);
}

void Mount::getFstabEntry() {
    // Has been found already
    if (mnt_fs != nullptr) return;

    if (mnt_table_parse_fstab(mnt_table, NULL) != 0)
        throw std::runtime_error{"Error reading fstab."};
    mnt_fs = mnt_table_find_target(mnt_table, target.c_str(), MNT_ITER_BACKWARD);
}

void Mount::find() {
    getFstabEntry();

    if (mnt_fs == nullptr)
        throw std::runtime_error{"File system " + target + " not found in fstab."};
}

void Mount::getMntFs()
{
    getFstabEntry();

    if (mnt_fs == nullptr)
        mnt_fs = mnt_new_fs();
}

std::string Mount::getFS() {
    find();
    return mnt_fs_get_fstype(mnt_fs);
}

std::string Mount::getTarget()
{
    return target;
}

bool Mount::isMounted() {
    getFstabEntry();
    return mnt_fs ? true : false;
}

void Mount::setSource(std::string source)
{
    getMntFs();
    if (mnt_fs_set_source(mnt_fs, source.c_str()) != 0) {
        throw std::runtime_error{"Setting source directory '" + source + "' for '" + target + "' failed."};
    }
}

void Mount::setType(std::string type)
{
    getMntFs();
    if (mnt_fs_set_fstype(mnt_fs, type.c_str()) != 0) {
        throw std::runtime_error{"Setting file system type '" + type + "' for '" + target + "' failed."};
    }
}

void Mount::mount(std::string prefix)
{
    std::cout << "Mounting " << target << "..." << std::endl;

    std::string mounttarget = prefix + target.c_str();
    if (mnt_fs_set_target(mnt_fs, mounttarget.c_str()) != 0) {
        throw std::runtime_error{"Setting target '" + mounttarget + "' for mountpoint failed."};
    }

    mnt_cxt = mnt_new_context();
    if (mnt_context_set_fs(mnt_cxt, mnt_fs) != 0) {
        throw std::runtime_error{"Setting mount context for '" + target + "' failed."};
    }

    if (mnt_context_set_mflags(mnt_cxt, flags) != 0) {
        throw std::runtime_error{"Setting mount flags for '" + target + "' failed."};
    }

    std::filesystem::create_directories(mounttarget);

    int rc = mnt_context_mount(mnt_cxt);
    char buf[BUFSIZ] = { 0 };
    rc = mnt_context_get_excode(mnt_cxt, rc, buf, sizeof(buf));
    if (*buf)
            throw std::runtime_error{"Mounting '" + target + "': " + buf};
}

BindMount::BindMount(std::string target, unsigned long flags)
    : Mount(target, flags | MS_BIND)
{
}

void BindMount::mount(std::string prefix)
{
    getMntFs();
    mnt_fs_set_source(mnt_fs, target.c_str());
    Mount::mount(prefix);
}
