/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <sched.h>
#include <string>
#include <sys/mount.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <utime.h>

using namespace std;

enum SYNC_ACTIONS {
    SKIP,
    RECURSIVE_SKIP,
    DELETE,
    COPY
};

bool diff_attrs(filesystem::path ref, filesystem::path cmp) {
    struct stat stat_ref;
    struct stat stat_cmp;
    if (lstat(ref.c_str(), &stat_ref) == -1) {
        cerr << "Error while processing " << ref << ": ";
        perror("lstat ref");
        return false;
    }
    if (lstat(cmp.c_str(), &stat_cmp) == -1) {
        cerr << "Error while processing " << cmp << ": ";
        perror("lstat cmp");
        return false;
    }
    if (stat_ref.st_mode != stat_cmp.st_mode ||
            stat_ref.st_uid != stat_cmp.st_uid ||
            stat_ref.st_gid != stat_cmp.st_gid ||
            stat_ref.st_mtim.tv_sec != stat_cmp.st_mtim.tv_sec ||
            (!S_ISDIR(stat_ref.st_mode) && stat_ref.st_size != stat_cmp.st_size)) {
        cout << "File changed: " << cmp << endl;
        return true;
    }
    return false;
}

bool diff_xattrs(filesystem::path ref, filesystem::path cmp) {
    ssize_t buflen_ref, keylen_ref, vallen_ref, buflen_cmp, vallen_cmp;
    buflen_ref = llistxattr(ref.c_str(), NULL, 0);
    if (buflen_ref == -1) {
        cerr << "Error while processing " << ref << ": ";
        perror("llistxattr ref len");
        return false;
    }
    buflen_cmp = llistxattr(cmp.c_str(), NULL, 0);
    if (buflen_cmp == -1) {
        cerr << "Error while processing " << cmp << ": ";
        perror("llistxattr cmp len");
        return false;
    }
    if (buflen_ref == 0 && buflen_cmp == 0) {
        return false;
    }
    if (buflen_ref != buflen_cmp) {
        cout << "Extented attribute count changed: " << cmp << endl;
        return true;
    }
    std::unique_ptr<char[]> buf_ref(new char[buflen_ref]);
    std::unique_ptr<char[]> buf_cmp(new char[buflen_cmp]);

    /*
     * Copy the list of attribute keys to the buffer.
     */
    buflen_ref = llistxattr(ref.c_str(), buf_ref.get(), buflen_ref);
    if (buflen_ref == -1) {
        cerr << "Error while processing " << ref << ": ";
        perror("llistxattr ref list");
        return false;
    }
    buflen_cmp = llistxattr(cmp.c_str(), buf_cmp.get(), buflen_cmp);
    if (buflen_cmp == -1) {
        cerr << "Error while processing " << cmp << ": ";
        perror("llistxattr cmp list");
        return false;
    }

    /*
     * Loop over the list of zero terminated strings with the
     * attribute keys. Use the remaining buffer length to determine
     * the end of the list.
     */
    auto key = buf_ref.get();
    while (buflen_ref > 0) {
        /*
         * Determine length of the value.
         */
        vallen_ref = lgetxattr(ref.c_str(), key, NULL, 0);
        if (vallen_ref == -1) {
            cerr << "Error while processing " << ref << ": ";
            perror("lgetxattr ref len");
            return false;
        }
        vallen_cmp = lgetxattr(cmp.c_str(), key, NULL, 0);
        if (vallen_cmp == -1) {
            if (errno == ENODATA) { // The named attribute does not exist
                cout << "Extended attribute key changed: " << endl;
                return true;
            } else {
                cerr << "Error while processing " << cmp << ": ";
                perror("lgetxattr cmp len");
                return false;
            }
        }
        if (vallen_ref > 0) {
            std::unique_ptr<char[]> val_ref(new char[vallen_ref]);
            vallen_ref = lgetxattr(ref.c_str(), key, val_ref.get(), vallen_ref);
            if (vallen_ref == -1) {
                cerr << "Error while processing " << ref << ": ";
                perror("lgetxattr ref get");
                return false;
            }
            std::unique_ptr<char[]> val_cmp(new char[vallen_cmp]);
            vallen_cmp = lgetxattr(cmp.c_str(), key, val_cmp.get(), vallen_cmp);
            if (vallen_cmp == -1) {
                cerr << "Error while processing " << cmp << ": ";
                perror("lgetxattr cmp get");
                return false;
            }
            if (memcmp(val_ref.get(), val_cmp.get(), vallen_ref) != 0) {
                cout << "Extended attribute value changed: " << cmp << endl;
                return true;
            }
        }
        keylen_ref = strlen(key) + 1;
        buflen_ref -= keylen_ref;
        key += keylen_ref;
    }
    return false;
}

bool copy_xattrs(filesystem::path ref, filesystem::path target) {
    ssize_t buflen_ref, keylen_ref, vallen_ref, vallen_target;
    buflen_ref = llistxattr(ref.c_str(), NULL, 0);
    if (buflen_ref == -1) {
        cerr << "Error while processing " << ref << ": ";
        perror("llistxattr len");
        return false;
    }
    if (buflen_ref == 0) {
        return true;
    }
    std::unique_ptr<char[]> buf_ref(new char[buflen_ref]);

    /*
     * Copy the list of attribute keys to the buffer.
     */
    buflen_ref = llistxattr(ref.c_str(), buf_ref.get(), buflen_ref);
    if (buflen_ref == -1) {
        cerr << "Error while processing " << ref << ": ";
        perror("llistxattr list");
        return false;
    }

    auto key = buf_ref.get();
    while (buflen_ref > 0) {
        vallen_ref = lgetxattr(ref.c_str(), key, NULL, 0);
        if (vallen_ref == -1) {
            cerr << "Error while processing " << ref << ": ";
            perror("lgetxattr len");
            return false;
        }
        if (vallen_ref > 0) {
            std::unique_ptr<char[]> val_ref(new char[vallen_ref]);
            vallen_ref = lgetxattr(ref.c_str(), key, val_ref.get(), vallen_ref);
            if (vallen_ref == -1) {
                cerr << "Error while processing " << ref << ": ";
                perror("lgetxattr get");
                return false;
            }
            std::unique_ptr<char[]> val_cmp(new char[vallen_ref]);
            vallen_target = lsetxattr(target.c_str(), key, val_ref.get(), vallen_ref, 0);
            if (vallen_target == -1) {
                cerr << "Error while processing " << ref << ": ";
                perror("lsetxattr");
                return false;
            }
        }
        keylen_ref = strlen(key) + 1;
        buflen_ref -= keylen_ref;
        key += keylen_ref;
    }
    return true;
}

std::string rtrim(std::string s)
{
    std::string copy;
    auto i = s.rbegin();
    while (*i == '\n' || *i == ' ') i++;

    std::copy(s.begin(), s.end() - (i - s.rbegin()), std::back_inserter(copy));
    return copy;
}

std::string exec(const char* cmd) {
    char buffer[128];
    std::string result;
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer, 128, pipe.get()) != nullptr) {
        result += buffer;
    }
    return result;
}

bool bind_mount(filesystem::path& dir) {
    std::error_code err;
    filesystem::path bind_dir;
    bool mounted = false;

    cout << "Creating bind mount for " << dir << endl;
    bind_dir = rtrim(exec("mktemp --directory /tmp/transactional-update.XXXX"));
    if (mount(dir.c_str(), bind_dir.c_str(), "none", MS_BIND, "none") != 0) {
        cerr << "Error while bind mounting " << dir << " in " << bind_dir << ": ";
        perror("mount");
        goto fail;
    }
    mounted = true;
    if (mount(bind_dir.c_str(), bind_dir.c_str(), "none", MS_BIND | MS_RDONLY | MS_REMOUNT, "none") != 0) {
        cerr << "Error while remounting " << bind_dir << ": ";
        perror("mount");
        goto fail;
    }

    dir = bind_dir;

    return true;
fail:
    if (mounted && umount(bind_dir.c_str()) == 0) {
        cerr << "Error while unmounting " << dir << " in " << bind_dir << ": ";
        perror("umount");
    }
    filesystem::remove(bind_dir, err);
    return false;
}

void unmount(filesystem::path& dir) {
    std::error_code err;
    if (umount2(dir.c_str(), MNT_DETACH) != 0) {
        cerr << "Error while unmounting " << dir << ": ";
        perror("umount");
    } else {
        if (filesystem::remove(dir, err)) {
            cerr << "Error while removing directory " << dir << ": " << err.message() << endl;
        }
    }
}

int main(int argc, const char* argv[])
{
    bool dry_run = false;
    bool keep_syncpoint = false;
    bool bind_mounted = false;
    int argpos = 1;
    filesystem::path syncpoint = "/etc/etc.syncpoint";
    filesystem::path currentdir = "/etc";
    filesystem::path parentdir;

    if (argc > 1) {
        if (string(argv[1]) == "--dry-run" || string(argv[1]) == "-n") {
            dry_run = true;
            argpos++;
        } else if (string(argv[1]) == "--keep-syncpoint") {
            keep_syncpoint = true;
            argpos++;
        }
    }
    if (argc - argpos != 3) {
        cerr << "Wrong number of arguments." << endl;
        cerr << "Arguments: [--dry-run|-n] [--keep-syncpoint] <parent etc> <current etc> <reference etc>" << endl;
        _exit(1);
    }

    string parent;

    parentdir = argv[argpos];
    currentdir = argv[argpos + 1];
    syncpoint = argv[argpos + 2];

    filesystem::path bind_parentdir = parentdir;
    filesystem::path bind_currentdir = currentdir;

    if (bind_mount(bind_parentdir)) {
        if (bind_mount(bind_currentdir)) {
            bind_mounted = true;
        } else {
            unmount(bind_parentdir);
        }
    }
    cout << "Using new snapshot - syncing from old parent " << parentdir << "..." << endl;

    map<filesystem::path, SYNC_ACTIONS> DIFFTOCURRENT;

    // Check which files have been changed in new snapshot
    filesystem::current_path(syncpoint);
    auto it = filesystem::recursive_directory_iterator(".");
    for (const filesystem::directory_entry& dir_entry : it) {
        if (dir_entry.path().native() == "./etc.syncpoint") {
            it.disable_recursion_pending();
            continue;
        }

        if (! filesystem::exists(filesystem::symlink_status(bind_currentdir / dir_entry))) {
            cout << "Deleted in new snapshot: " << currentdir / dir_entry << endl;
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::RECURSIVE_SKIP);
            continue;
        }

        if (diff_attrs(dir_entry.path(), currentdir / dir_entry)) {
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::SKIP);
            continue;
        }

        if (diff_xattrs(dir_entry.path(), currentdir / dir_entry)) {
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::SKIP);
            continue;
        }
    }
    filesystem::current_path(bind_currentdir);
    it = filesystem::recursive_directory_iterator(".");
    for (const filesystem::directory_entry& dir_entry : it) {
        if (dir_entry.path().native() == "./etc.syncpoint") {
            it.disable_recursion_pending();
            continue;
        }

        if (! filesystem::exists(filesystem::symlink_status(syncpoint / dir_entry))) {
            cout << "Added in new snapshot: " << currentdir / dir_entry << endl;
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::SKIP);
        }
    }

    // Check which files have been changed in old snapshot
    filesystem::current_path(syncpoint);
    it = filesystem::recursive_directory_iterator(".");
    for (const filesystem::directory_entry& dir_entry : it) {
        // The syncpoint shouldn't have syncpoint inside, but for good measure let's just skip that too
        if (dir_entry.path().native() == "./etc.syncpoint") {
            it.disable_recursion_pending();
            continue;
        }

        if (! filesystem::exists(filesystem::symlink_status(bind_parentdir / dir_entry))) {
            cout << "Deleted in old snapshot: " << parentdir / dir_entry << endl;
            if (DIFFTOCURRENT.count(dir_entry) == 0) {
                DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::DELETE);
                if (filesystem::is_directory(currentdir / dir_entry)) {
                    // If some file was changed or added within that directory in the new snapshot, then don't delete dir.
                    // First retrieve the lexicographically closest entry, then delete anything starting with that name.
                    auto potentialContains = DIFFTOCURRENT.lower_bound(dir_entry);
                    while (potentialContains->first.native().rfind(dir_entry.path().native(), 0) == 0) {
                        DIFFTOCURRENT[dir_entry] = SYNC_ACTIONS::SKIP;
                        potentialContains++;
                    }
                }
            }
            continue;
        }

        // map.emplace doesn't overwrite existing elements, so this is a noop if the file is marked as SKIP already
        if (diff_attrs(dir_entry.path(), parentdir / dir_entry)) {
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::COPY);
            continue;
        }

        if (diff_xattrs(dir_entry.path(), parentdir / dir_entry)) {
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::COPY);
            continue;
        }
    }
    filesystem::current_path(bind_parentdir);
    it = filesystem::recursive_directory_iterator(".");
    for (const filesystem::directory_entry& dir_entry : it) {
        if (dir_entry.path().native() == "./etc.syncpoint") {
            it.disable_recursion_pending();
            continue;
        }

        if (! filesystem::exists(filesystem::symlink_status(syncpoint / dir_entry))) {
            cout << "Added in old snapshot: " << parentdir / dir_entry << endl;
            DIFFTOCURRENT.emplace(dir_entry, SYNC_ACTIONS::COPY);
        }
    }

    cout << "Processing files..." << endl;

    // Process generated list
    if (!dry_run) {
        for(auto it = DIFFTOCURRENT.begin(); it != DIFFTOCURRENT.end(); ++it) {
            if (it->second == SYNC_ACTIONS::RECURSIVE_SKIP) {
                auto potentialContains = DIFFTOCURRENT.lower_bound(it->first);
                if (it->first != potentialContains->first) {
                    while (! filesystem::relative(it->first, potentialContains->first).empty()) {
                        cout << it->first << " " << potentialContains->first << endl;
                        DIFFTOCURRENT[potentialContains->first] = SYNC_ACTIONS::SKIP;
                    }
                }
                continue;
            }
            if (it->second == SYNC_ACTIONS::DELETE) {
                if (filesystem::exists(filesystem::symlink_status(currentdir / it->first))) {
                    cout << "Deleting " << it->first << endl;
                    filesystem::remove_all(currentdir / it->first);
                }
                continue;
            }
            if (it->second == SYNC_ACTIONS::COPY) {
                cout << "Copying " << it->first << endl;
                struct statx sourcestat;
                struct statx targetstat = {};
                if (statx(AT_FDCWD, (bind_parentdir / it->first).c_str(), AT_SYMLINK_NOFOLLOW, STATX_ATIME | STATX_MTIME | STATX_MODE | STATX_UID | STATX_GID, &sourcestat) == -1) {
                    cerr << "Error while processing " << it->first << ": ";
                    perror("statx source");
                    continue;
                }
                if (filesystem::exists(filesystem::symlink_status(currentdir / it->first))) {
                    if (statx(AT_FDCWD, (bind_currentdir / it->first).c_str(), AT_SYMLINK_NOFOLLOW, STATX_MODE, &targetstat) == -1) {
                        cerr << "Error while processing " << it->first << ": ";
                        perror("statx target");
                        continue;
                    }
                    if ((sourcestat.stx_mode & S_IFMT) != (targetstat.stx_mode & S_IFMT)) {
                        cout << it->first << " changed type." << endl;
                        filesystem::remove_all(currentdir / it->first);
                    }
                }
                if ((sourcestat.stx_mode & S_IFMT) == S_IFDIR) {
                    filesystem::create_directory(currentdir / it->first, parentdir / it->first);
                } else if ((sourcestat.stx_mode & S_IFMT) == S_IFLNK) {
                    if (filesystem::exists(filesystem::symlink_status(currentdir / it->first))) {
                        filesystem::remove(currentdir / it->first);
                    }
                    filesystem::copy(bind_parentdir / it->first, currentdir / it->first, filesystem::copy_options::copy_symlinks);
                } else if ((sourcestat.stx_mode & S_IFMT) == S_IFREG) {
                    if (filesystem::exists((filesystem::symlink_status((currentdir / it->first).parent_path())))) {
                        filesystem::copy_file(bind_parentdir / it->first, currentdir / it->first, filesystem::copy_options::overwrite_existing);
                        if (lchmod((currentdir / it->first).c_str(), sourcestat.stx_mode) == -1) {
                            cerr << "Error while processing " << it->first << ": ";
                            perror("lchmod");
                        }
                    } else {
                        cout << "Parent directory of " << it->first << " was deleted in new snapshot - skipping file..." << endl;
                    }
                } else {
                    cerr << "Unsupported file type for file " << it->first << ". Skipping..." << endl;
                    continue;
                }
                if (lchown((currentdir / it->first).c_str(), sourcestat.stx_uid, sourcestat.stx_gid) == -1) {
                    cerr << "Error while processing " << it->first << ": ";
                    perror("lchown");
                }
                copy_xattrs(bind_parentdir / it->first, currentdir / it->first);

                const struct timespec newtimes[2] = {{.tv_sec = sourcestat.stx_atime.tv_sec, .tv_nsec = sourcestat.stx_atime.tv_nsec},{.tv_sec = sourcestat.stx_mtime.tv_sec, .tv_nsec = sourcestat.stx_mtime.tv_nsec}};
                if (utimensat(AT_FDCWD, (currentdir / it->first).c_str(), newtimes, AT_SYMLINK_NOFOLLOW) == -1) {
                    cerr << "Error while processing " << it->first << ": ";
                    perror("utimensat");
                }
            }
        }

        if (!keep_syncpoint) {
            filesystem::remove_all(syncpoint);
        }
    }

    if (bind_mounted) {
        // Sync to avoid pending operations
        sync();
        unmount(bind_parentdir);
        unmount(bind_currentdir);
    }

    return 0;
}
