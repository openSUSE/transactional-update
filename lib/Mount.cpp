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

#include "Log.h"
#include "Mount.h"
#include <cstring>
#include <filesystem>
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
    int rc;
    int mountStatus = 0;
    if (mnt_cxt && mnt_fs && (rc = mnt_context_is_fs_mounted(mnt_cxt, mnt_fs, &mountStatus)) != 0)
        tulog.error("Error determining mount status of ", target, ": ", rc);
    if (mountStatus == 1) {
        tulog.debug("Unmounting ", target, "...");
        mnt_reset_context(mnt_cxt);
        if ((rc = mnt_context_set_fs(mnt_cxt, mnt_fs)) != 0) {
            tulog.error("Setting umount context for '", target, "' failed: ", rc);
        }
        int rc = mnt_context_umount(mnt_cxt);
        char buf[BUFSIZ] = { 0 };
        rc = mnt_context_get_excode(mnt_cxt, rc, buf, sizeof(buf));
        if (*buf)
                tulog.error("Error unmounting '", target, "': ", buf);
    }

    mnt_free_context(mnt_cxt);
    mnt_unref_fs(mnt_fs);
    mnt_free_table(mnt_table);
}

void Mount::getTabEntry() {
    // Has been found already
    if (mnt_fs != nullptr) return;

    int rc;
    if (tabsource.empty()) {
        if ((rc = mnt_table_parse_fstab(mnt_table, NULL)) != 0)
            throw std::runtime_error{"Error reading " + target + " entry from fstab : " + std::to_string(rc)};
    } else {
        if ((rc = mnt_table_parse_file(mnt_table, tabsource.c_str())) != 0)
            throw std::runtime_error{"Error reading " + target + " entry from " + tabsource + ": " + std::to_string(rc)};
    }
    mnt_fs = mnt_table_find_target(mnt_table, target.c_str(), MNT_ITER_BACKWARD);
}

void Mount::find() {
    getTabEntry();

    if (mnt_fs == nullptr)
        throw std::runtime_error{"File system " + target + " not found in fstab."};
}

void Mount::getMntFs()
{
    getTabEntry();

    if (mnt_fs == nullptr)
        mnt_fs = mnt_new_fs();
}

std::string Mount::getFS() {
    find();
    return mnt_fs_get_fstype(mnt_fs);
}

void Mount::removeOption(std::string option) {
    find();

    int rc;
    const char* current_opts;
    if ((current_opts = mnt_fs_get_options(mnt_fs)) == NULL)
        throw std::runtime_error{"Options for file system " + target + "not found."};

    char* new_opts = strdup(current_opts);
    if ((rc = mnt_optstr_remove_option(&new_opts, option.c_str())) != 0) {
        free(new_opts);
        throw std::runtime_error{"File system option " + option + "could not be removed: " + std::to_string(rc)};
    }
    if ((rc = mnt_fs_set_options(mnt_fs, new_opts)) != 0) {
        free(new_opts);
        throw std::runtime_error{"Could not set new options " + std::string(new_opts) + " for file system " + target + ": " + std::to_string(rc)};
    }
    free(new_opts);
}

std::string Mount::getOption(std::string option) {
    find();

    char* opt;
    size_t len = 0;
    int rc = mnt_fs_get_option(mnt_fs, option.c_str(), &opt, &len);
    if (rc < 0)
        throw std::runtime_error{"Error retrieving options for file system " + target + ": " + std::to_string(rc)};
    else if (rc > 0)
        throw std::range_error{"Option " + option + " not found for file system " + target + "."};
    return std::string(opt).substr(0, len);
}

void Mount::setOption(std::string option, std::string value) {
    find();

    int rc;
    const char* current_opts;
    if ((current_opts = mnt_fs_get_options(mnt_fs)) == NULL)
        throw std::runtime_error{"Options for file system " + target + "not found."};

    char* new_opts = strdup(current_opts);
    if ((rc = mnt_optstr_set_option(&new_opts, option.c_str(), value.c_str())) != 0) {
        free(new_opts);
        throw std::runtime_error{"File system option " + option + "could not be set to " + value + ": " + std::to_string(rc)};
    }
    if ((rc = mnt_fs_set_options(mnt_fs, new_opts)) != 0) {
        free(new_opts);
        throw std::runtime_error{"Could not set new options " + std::string(new_opts) + " for file system " + target + ": " + std::to_string(rc)};
    }
    free(new_opts);
}

void Mount::setTabSource(std::string source) {
    tabsource = source;
}

std::string Mount::getTarget()
{
    return target;
}

bool Mount::isMount() {
    getTabEntry();
    return mnt_fs ? true : false;
}

void Mount::setSource(std::string source)
{
    getMntFs();

    int rc;
    if ((rc = mnt_fs_set_source(mnt_fs, source.c_str())) != 0) {
        throw std::runtime_error{"Setting source directory '" + source + "' for '" + target + "' failed: " + std::to_string(rc)};
    }
}

void Mount::setType(std::string type)
{
    getMntFs();

    int rc;
    if ((rc = mnt_fs_set_fstype(mnt_fs, type.c_str())) != 0) {
        throw std::runtime_error{"Setting file system type '" + type + "' for '" + target + "' failed: " + std::to_string(rc)};
    }
}

void Mount::mount(std::string prefix)
{
    tulog.debug("Mounting ", target, "...");

    int rc;
    std::string mounttarget = prefix + target.c_str();
    if ((rc = mnt_fs_set_target(mnt_fs, mounttarget.c_str())) != 0) {
        throw std::runtime_error{"Setting target '" + mounttarget + "' for mountpoint failed: " + std::to_string(rc)};
    }

    mnt_cxt = mnt_new_context();
    if ((rc = mnt_context_set_fs(mnt_cxt, mnt_fs)) != 0) {
        throw std::runtime_error{"Setting mount context for '" + target + "' failed: " + std::to_string(rc)};
    }

    if ((rc = mnt_context_set_mflags(mnt_cxt, flags)) != 0) {
        throw std::runtime_error{"Setting mount flags for '" + target + "' failed: " + std::to_string(rc)};
    }

    std::filesystem::create_directories(mounttarget);

    rc = mnt_context_mount(mnt_cxt);
    char buf[BUFSIZ] = { 0 };
    mnt_context_get_excode(mnt_cxt, rc, buf, sizeof(buf));
    if (*buf)
            throw std::runtime_error{"Mounting '" + target + "': " + buf};
}

void Mount::persist(std::filesystem::path file) {
    int rc = 0;
    std::string err;

    struct libmnt_table* snap_table = mnt_new_table();

    if ((rc = mnt_table_parse_file(snap_table, file.c_str())) != 0)
        err = "No mount table found in '" + std::string(file) + "': " + std::to_string(rc);
    struct libmnt_fs* old_fs_entry = mnt_table_find_target(snap_table, target.c_str(), MNT_ITER_BACKWARD);

    struct libmnt_fs* new_fs = mnt_copy_fs(NULL, mnt_fs);
    if (!rc && (rc = mnt_table_remove_fs(snap_table, old_fs_entry)) != 0)
        err = "Removing old '" + target + "' from target table failed: " + std::to_string(rc);
    if (!rc && (rc = mnt_table_add_fs(snap_table, new_fs)) != 0)
        err = "Adding new '" + target + "' to target table failed: " + std::to_string(rc);

    FILE *f = fopen(file.c_str(), "w");
    if (!rc && (rc = mnt_table_write_file(snap_table, f)) != 0) {
        fclose(f);
        err = "Writing new mount table '" + std::string(file) + "' failed: " + std::to_string(rc);
    }
    fclose(f);

    mnt_unref_fs(new_fs);
    mnt_free_table(snap_table);

    if (!err.empty()) {
        throw std::runtime_error{err};
    }
}

BindMount::BindMount(std::string target, unsigned long flags)
    : Mount(target, flags | MS_BIND)
{
}

void BindMount::mount(std::string prefix)
{
    int rc;
    if (mnt_fs == nullptr) {
        mnt_fs = mnt_new_fs();
        if ((rc = mnt_fs_set_source(mnt_fs, target.c_str())) != 0) {
            throw std::runtime_error{"Setting source for " + target + " mount failed: " + std::to_string(rc)};
        }
    }
    Mount::mount(prefix);
}
