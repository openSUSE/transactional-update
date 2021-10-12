/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Wrapper for libmount
 */

#include "Log.hpp"
#include "Mount.hpp"
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace TransactionalUpdate {

Mount::Mount(std::string mountpoint, unsigned long flags)
    : mnt_table{mnt_new_table()}, mountpoint{std::move(mountpoint)},
      flags{std::move(flags)}
{
}

Mount::Mount(Mount&& other) noexcept
{
    std::swap(mnt_table, other.mnt_table);
    std::swap(mnt_fs, other.mnt_fs);
    std::swap(mnt_cxt, other.mnt_cxt);
    std::swap(mountpoint, other.mountpoint);
    std::swap(flags, other.flags);
}

Mount::~Mount() {
    if (mnt_fs) {
        struct libmnt_table* umount_table = mnt_new_table();
        if ((mnt_table_parse_mtab(umount_table, nullptr)) != 0)
            tulog.error("Error reading mtab for umount");
        struct libmnt_fs* umount_fs = mnt_table_find_target(umount_table,  mnt_fs_get_target(mnt_fs), MNT_ITER_BACKWARD);
        umountRecursive(umount_table, umount_fs);
        mnt_free_fs(umount_fs);
        mnt_free_table(umount_table);
    }

    if (!directoryCreated.empty()) {
        try {
            std::filesystem::remove_all(std::filesystem::path{directoryCreated});
        }  catch (const std::exception &e) {
            tulog.error("ERROR: ", e.what());
        }
    }

    mnt_free_context(mnt_cxt);
    mnt_unref_fs(mnt_fs);
    mnt_free_table(mnt_table);
}

struct libmnt_fs* Mount::getTabEntry() {
    // Has been found already
    if (mnt_fs != nullptr) return mnt_fs;

    int rc;
    if (tabsource.empty()) {
        if ((rc = mnt_table_parse_fstab(mnt_table, nullptr)) != 0)
            throw std::runtime_error{"Error reading " + mountpoint + " entry from fstab : " + std::to_string(rc)};
    } else {
        if ((rc = mnt_table_parse_file(mnt_table, tabsource.c_str())) != 0)
            throw std::runtime_error{"Error reading " + mountpoint + " entry from " + tabsource + ": " + std::to_string(rc)};
    }
    return mnt_table_find_target(mnt_table, mountpoint.c_str(), MNT_ITER_BACKWARD);
}

struct libmnt_fs* Mount::findFS() {
    struct libmnt_fs* mnt_fs = getTabEntry();

    if (mnt_fs == nullptr)
        throw std::runtime_error{"File system " + mountpoint + " not found in fstab."};

    return mnt_fs;
}

struct libmnt_fs* Mount::newFS() {
    if (mnt_fs == nullptr)
        return mnt_new_fs();
    return mnt_fs;
}

std::string Mount::getFilesystem() {
    mnt_fs = findFS();
    return mnt_fs_get_fstype(mnt_fs);
}

void Mount::removeOption(std::string option) {
    mnt_fs = findFS();

    int rc;
    const char* current_opts;
    if ((current_opts = mnt_fs_get_options(mnt_fs)) == nullptr)
        throw std::runtime_error{"Options for file system " + mountpoint + "not found."};

    char* new_opts = strdup(current_opts);
    if ((rc = mnt_optstr_remove_option(&new_opts, option.c_str())) != 0) {
        free(new_opts);
        throw std::runtime_error{"File system option " + option + "could not be removed: " + std::to_string(rc)};
    }
    if ((rc = mnt_fs_set_options(mnt_fs, new_opts)) != 0) {
        std::string snew_opts = std::string(new_opts);
        free(new_opts);
        throw std::runtime_error{"Could not set new options " + snew_opts + " for file system " + mountpoint + ": " + std::to_string(rc)};
    }
    free(new_opts);
}

std::string Mount::getOption(std::string option) {
    mnt_fs = findFS();

    char* opt;
    size_t len = 0;
    int rc = mnt_fs_get_option(mnt_fs, option.c_str(), &opt, &len);
    if (rc < 0)
        throw std::runtime_error{"Error retrieving options for file system " + mountpoint + ": " + std::to_string(rc)};
    else if (rc > 0)
        throw std::range_error{"Option " + option + " not found for file system " + mountpoint + "."};
    else if (opt == nullptr)
        return "";
    return std::string(opt, len);
}

void Mount::setOption(std::string option, std::string value) {
    mnt_fs = findFS();

    int rc;
    const char* current_opts;
    if ((current_opts = mnt_fs_get_options(mnt_fs)) == nullptr)
        throw std::runtime_error{"Options for file system " + mountpoint + " not found."};

    char* new_opts = strdup(current_opts);
    if ((rc = mnt_optstr_set_option(&new_opts, option.c_str(), value.c_str())) != 0) {
        free(new_opts);
        throw std::runtime_error{"File system option " + option + "could not be set to " + value + ": " + std::to_string(rc)};
    }
    if ((rc = mnt_fs_set_options(mnt_fs, new_opts)) != 0) {
        std::string snew_opts = std::string(new_opts);
        free(new_opts);
        throw std::runtime_error{"Could not set new options " + std::string(snew_opts) + " for file system " + mountpoint + ": " + std::to_string(rc)};
    }
    free(new_opts);
}

void Mount::setTabSource(std::string source) {
    if (mnt_fs != nullptr) {
        throw std::logic_error{"Cannot set tab source for " + mountpoint + ": fs has been initialized already"};
    }
    tabsource = source;
}

bool Mount::isMount() {
    mnt_fs = getTabEntry();
    return mnt_fs != nullptr;
}

void Mount::setSource(std::string source) {
    mnt_fs = newFS();

    int rc;
    if ((rc = mnt_fs_set_source(mnt_fs, source.c_str())) != 0) {
        throw std::runtime_error{"Setting source directory '" + source + "' for '" + mountpoint + "' failed: " + std::to_string(rc)};
    }
}

void Mount::setType(std::string type) {
    mnt_fs = newFS();

    int rc;
    if ((rc = mnt_fs_set_fstype(mnt_fs, type.c_str())) != 0) {
        throw std::runtime_error{"Setting file system type '" + type + "' for '" + mountpoint + "' failed: " + std::to_string(rc)};
    }
}

void Mount::mount(std::string prefix) {
    tulog.debug("Mounting ", mountpoint, "...");

    int rc;
    std::string mounttarget = prefix + mountpoint.c_str();
    if ((rc = mnt_fs_set_target(mnt_fs, mounttarget.c_str())) != 0) {
        throw std::runtime_error{"Setting target '" + mounttarget + "' for mountpoint failed: " + std::to_string(rc)};
    }

    mnt_cxt = mnt_new_context();
    if ((rc = mnt_context_set_fs(mnt_cxt, mnt_fs)) != 0) {
        throw std::runtime_error{"Setting mount context for '" + mountpoint + "' failed: " + std::to_string(rc)};
    }

    if ((rc = mnt_context_set_mflags(mnt_cxt, flags)) != 0) {
        throw std::runtime_error{"Setting mount flags for '" + mountpoint + "' failed: " + std::to_string(rc)};
    }

    if (! std::filesystem::is_directory(mounttarget)) {
        tulog.debug("Mount target ", mounttarget, " does not exist - creating...");
        directoryCreated = mounttarget;
    }
    std::filesystem::create_directories(mounttarget);

    rc = mnt_context_mount(mnt_cxt);
    char buf[BUFSIZ] = { 0 };
    mnt_context_get_excode(mnt_cxt, rc, buf, sizeof(buf));
    if (*buf)
            throw std::runtime_error{"Mounting '" + mountpoint + "': " + buf};
}

void Mount::persist(std::filesystem::path file) {
    int rc = 0;
    std::string err;

    struct libmnt_table* snap_table = mnt_new_table();

    if ((rc = mnt_table_parse_file(snap_table, file.c_str())) != 0)
        err = "No mount table found in '" + std::string(file) + "': " + std::to_string(rc);
    struct libmnt_fs* old_fs_entry = mnt_table_find_target(snap_table, mountpoint.c_str(), MNT_ITER_BACKWARD);

    struct libmnt_fs* new_fs = mnt_copy_fs(nullptr, mnt_fs);
    if (!rc && (rc = mnt_table_remove_fs(snap_table, old_fs_entry)) != 0)
        err = "Removing old '" + mountpoint + "' from target table failed: " + std::to_string(rc);
    if (!rc && (rc = mnt_table_add_fs(snap_table, new_fs)) != 0)
        err = "Adding new '" + mountpoint + "' to target table failed: " + std::to_string(rc);

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

void Mount::umountRecursive(libmnt_table* umount_table, libmnt_fs* umount_fs) {
    int rc;
    struct libmnt_context* umount_cxt = mnt_new_context();
    if (mnt_cxt && umount_fs) {
        // Check for child mounts
        struct libmnt_fs* child_fs;
        struct libmnt_iter *iter = mnt_new_iter(MNT_ITER_BACKWARD);
        if (!iter)
            tulog.error("Error allocating umount iter");
        while ((rc = mnt_table_next_child_fs(umount_table, iter, umount_fs, &child_fs)) != 1) {
            if (rc < 0) {
                tulog.error("Error determining child mounts of ", mnt_fs_get_target(umount_fs), ": ", rc);
                break;
            } else if (rc == 0) {
                umountRecursive(umount_table, child_fs);
            }
        }
        mnt_free_iter(iter);

        // Unmount
        tulog.debug("Unmounting ", mnt_fs_get_target(umount_fs), "...");
        if ((rc = mnt_context_set_fs(umount_cxt, umount_fs)) != 0) {
            tulog.error("Setting umount context for '", mnt_fs_get_target(umount_fs), "' failed: ", rc);
        }
        int rc = mnt_context_umount(umount_cxt);
        char buf[BUFSIZ] = { 0 };
        mnt_context_get_excode(umount_cxt, rc, buf, sizeof(buf));
        if (*buf)
            tulog.error("Error unmounting '", mnt_fs_get_target(umount_fs), "': ", buf);
    }
    mnt_free_context(umount_cxt);
}

BindMount::BindMount(std::string mountpoint, unsigned long flags)
    : Mount(mountpoint, flags | MS_BIND)
{
}

void BindMount::mount(std::string prefix) {
    if (mnt_fs == nullptr) {
        setSource(mountpoint);
    }
    Mount::mount(prefix);
}

PropagatedBindMount::PropagatedBindMount(std::string mountpoint, unsigned long flags)
    : BindMount(mountpoint, flags | MS_REC | MS_SLAVE)
{
}

std::vector<std::filesystem::path> MountList::getList(std::filesystem::path prefix) {
    int rc = 0;
    std::string err;

    std::vector<std::filesystem::path> list;
    struct libmnt_table* mount_table = mnt_new_table();
    struct libmnt_iter* mount_iter = mnt_new_iter(MNT_ITER_FORWARD);
    struct libmnt_fs* mount_fs;

    if ((rc = mnt_table_parse_mtab(mount_table, NULL)) != 0)
        err = "Couldn't read fstab for reading mount points: " + std::to_string(rc);
    while (rc == 0) {
        if ((rc = mnt_table_next_fs(mount_table, mount_iter, &mount_fs)) == 0) {
            std::filesystem::path target;
            if ((target = mnt_fs_get_target(mount_fs)) == "") {
                err = "Could't read target for mount point list.";
                break;
            }
            if (target == "/")
                continue;
            list.push_back(prefix / target.relative_path());
        } else if (rc < 0) {
            err = "Error iterating fstab: " + std::to_string(rc);
            break;
        }
    }

    mnt_free_iter(mount_iter);
    mnt_free_table(mount_table);

    if (!err.empty()) {
        throw std::runtime_error{err};
    }
    return list;
}

} // namespace TransactionalUpdate
