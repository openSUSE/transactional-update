/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  The Transaction class is the central API class for the lifecycle of a
  transaction: It will open and close transactions and execute commands in the
  correct context.
 */

#include "Transaction.hpp"
#include "Configuration.hpp"
#include "Log.hpp"
#include "Mount.hpp"
#include "Plugins.hpp"
#include "SnapshotManager.hpp"
#include "Snapshot.hpp"
#include "Supplement.hpp"
#include "Util.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ftw.h>
#include <limits.h>
#include <poll.h>
#include <sched.h>
#include <selinux/restorecon.h>
#include <selinux/selinux.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
using namespace TransactionalUpdate;
namespace fs = std::filesystem;

static int inotifyFd;
std::vector<std::filesystem::path> inotifyExcludes;

class Transaction::impl {
public:
    void addSupplements();
    void snapMount();
    int runCommand(char* argv[], bool inChroot, std::string* buffer);
    static int inotifyAdd(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb);
    int inotifyRead();
    std::unique_ptr<SnapshotManager> snapshotMgr;
    std::unique_ptr<Snapshot> snapshot;
    fs::path bindDir;
    std::vector<std::unique_ptr<Mount>> dirsToMount;
    Supplements supplements;
    pid_t pidCmd;
    bool discardIfNoChange = false;
};

Transaction::Transaction() : pImpl{std::make_unique<impl>()} {
    tulog.debug("Constructor Transaction");
    if (getenv("TRANSACTIONAL_UPDATE") != NULL) {
        throw std::runtime_error{"Cannot open a new transaction from within a running transaction."};
    }
    pImpl->snapshotMgr = SnapshotFactory::get();
}

Transaction::~Transaction() {
    tulog.debug("Destructor Transaction");

    if (inotifyFd != 0)
        close(inotifyFd);

    pImpl->dirsToMount.clear();
    if (!pImpl->bindDir.empty()) {
        try {
            fs::remove(pImpl->bindDir);
        }  catch (const std::exception &e) {
            tulog.error("ERROR: ", e.what());
        }
    }
    try {
        if (isInitialized() && !getSnapshot().empty() && fs::exists(getRoot())) {
            tulog.info("Discarding snapshot ", pImpl->snapshot->getUid(), ".");
            pImpl->snapshot->abort();
            TransactionalUpdate::Plugins plugins{nullptr};
            plugins.run("abort-post", pImpl->snapshot->getUid());
        }
    }  catch (const std::exception &e) {
        tulog.error("ERROR: ", e.what());
    }
}

bool Transaction::isInitialized() {
    return pImpl->snapshot ? true : false;
}

std::string Transaction::getSnapshot() {
    return pImpl->snapshot->getUid();
}

fs::path Transaction::getRoot() {
    return pImpl->snapshot->getRoot();
}

fs::path Transaction::getBindDir() {
    return pImpl->bindDir;
}

void Transaction::impl::snapMount() {
    if (unshare(CLONE_NEWNS) < 0) {
        throw std::runtime_error{"Creating new mount namespace failed: " + std::string(strerror(errno))};
    }

    // GRUB needs to have an actual mount point for the root partition, so
    // mount the snapshot directory on a temporary mount point
    char bindTemplate[] = "/tmp/transactional-update-XXXXXX";
    bindDir = mkdtemp(bindTemplate);
    std::unique_ptr<BindMount> mntBind{new BindMount{bindDir, MS_PRIVATE, true}};
    mntBind->setSource(snapshot->getRoot());
    mntBind->mount();

    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/dev"));
    dirsToMount.push_back(std::make_unique<BindMount>("/var/log"));

    Mount mntVar{"/var"};
    if (mntVar.isMount()) {
        if (fs::is_directory("/var/lib/zypp"))
            dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/zypp"));
        dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/ca-certificates"));
        if (fs::is_directory("/var/lib/alternatives"))
            dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/alternatives"));
        if (fs::is_directory("/var/lib/selinux"))
            dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/selinux"));
        if (is_selinux_enabled()) {
            // If packages installed files into /var (which is not allowed, but still happens), they will end
            // up in the root file system, but will always be shadowed by the real /var mount. Due to that they
            // also won't be relabelled at any time. During updates this may cause problems if packages try to
            // access those leftover directories with wrong permissions, so they have to be relabelled manually...
            BindMount selinuxVar("/var/lib/selinux", 0, true);
            selinuxVar.mount(bindDir);
            BindMount selinuxEtc("/etc/selinux", 0, true);
            selinuxEtc.mount(bindDir);

            // restorecon keeps open file handles, so execute it in a child process - umount will fail otherwise
            pid_t childPid = fork();
            if (childPid < 0) {
                throw std::runtime_error{"Forking for SELinux relabelling failed: " + std::string(strerror(errno))};
            } else if (childPid == 0) {
                if (chroot(bindDir.c_str()) < 0) {
                    tulog.error("Chrooting to " + bindDir.native() + " for SELinux relabelling failed: " + std::string(strerror(errno)));
                    _exit(errno);
                }
                unsigned int restoreconOptions = SELINUX_RESTORECON_RECURSE | SELINUX_RESTORECON_IGNORE_DIGEST;
                if (tulog.level >= TULogLevel::Info)
                    restoreconOptions |= SELINUX_RESTORECON_VERBOSE;
                if (selinux_restorecon("/var", restoreconOptions) < 0) {
                    tulog.error("Relabelling of snapshot /var failed: " + std::string(strerror(errno)));
                    _exit(errno);
                }
                _exit(0);
            }
            else {
                int status;
                waitpid(childPid, &status, 0);
                if ((WIFEXITED(status) && WEXITSTATUS(status) != 0) || WIFSIGNALED(status)) {
                    throw std::runtime_error{"SELinux relabelling failed."};
                }
            }
        }
    }

    // Set up temporary directories, so changes will be discarded automatically
    dirsToMount.push_back(std::make_unique<BindMount>("/var/cache"));
    std::unique_ptr<Mount> mntTmp{new Mount{"/tmp"}};
    mntTmp->setType("tmpfs");
    mntTmp->setSource("tmpfs");
    dirsToMount.push_back(std::move(mntTmp));
    std::unique_ptr<Mount> mntRun{new Mount{"/run"}};
    mntRun->setType("tmpfs");
    mntRun->setSource("tmpfs");
    dirsToMount.push_back(std::move(mntRun));
    std::unique_ptr<Mount> mntVarTmp{new Mount{"/var/tmp"}};
    mntVarTmp->setType("tmpfs");
    mntVarTmp->setSource("tmpfs");
    dirsToMount.push_back(std::move(mntVarTmp));

    // Mount platform specific GRUB directories for GRUB updates
    if (fs::exists("/boot/grub2")) {
        for (auto& path: fs::directory_iterator("/boot/grub2")) {
            if (fs::is_directory(path)) {
                if (BindMount{path.path()}.isMount())
                    dirsToMount.push_back(std::make_unique<BindMount>(path.path()));
            }
        }
    }
    if (BindMount{"/boot/efi"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/boot/efi"));
    if (BindMount{"/boot/zipl"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/boot/zipl"));

    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/proc"));
    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/sys"));

    if (BindMount{"/root"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/root"));

    if (BindMount{"/boot/writable"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/boot/writable"));

    std::vector<std::string> customDirs = config.getArray("BINDDIRS");
    for (auto it = customDirs.begin(); it != customDirs.end(); ++it) {
        if (fs::is_directory(*it))
            dirsToMount.push_back(std::make_unique<BindMount>(*it));
        else
            tulog.info("Not bind mounting directory '" + *it + "' as it doesn't exist.");
    }

    dirsToMount.push_back(std::make_unique<BindMount>("/.snapshots"));

    for (auto it = dirsToMount.begin(); it != dirsToMount.end(); ++it) {
        it->get()->mount(bindDir);
    }

    dirsToMount.push_back(std::move(mntBind));
}

void Transaction::impl::addSupplements() {
    supplements = Supplements(bindDir);

    Mount mntVar{"/var"};
    if (mntVar.isMount()) {
        supplements.addDir(fs::path{"/var/tmp"});
        supplements.addLink(fs::path{"/run"}, fs::path{"/var/run"});
    }
    supplements.addLink(fs::path{"../../usr/lib/sysimage/rpm"}, fs::path{"/var/lib/rpm"});
    supplements.addFile(fs::path{"/run/netconfig"});
    supplements.addFile(fs::path{"/run/systemd/resolve/resolv.conf"});
    supplements.addFile(fs::path{"/run/systemd/resolve/stub-resolv.conf"});
    supplements.addDir(fs::path{"/var/spool"});
}

// Callback function for nftw to register all directories for inotify
int Transaction::impl::inotifyAdd(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    if (!(type == FTW_D))
        return 0;
    std::vector<std::filesystem::path>::iterator it;
    for (it = inotifyExcludes.begin(); it != inotifyExcludes.end(); it++) {
        if (std::string(pathname).find(*it) == 0)
            return 0;
    }
    int num;
    if ((num = inotify_add_watch(inotifyFd, pathname, IN_MODIFY | IN_MOVE | IN_CREATE | IN_DELETE | IN_ATTRIB | IN_ONESHOT | IN_ONLYDIR | IN_DONT_FOLLOW)) == -1)
        tulog.info("WARNING: Cannot register inotify watch for ", pathname);
    else
        tulog.debug("Watching ", pathname, " with descriptor number ", num);
    return 0;
}

void Transaction::init(std::string base, std::optional<std::string> description) {
    TransactionalUpdate::Plugins plugins{nullptr};
    plugins.run("init-pre", nullptr);

    if (base == "active")
        base = pImpl->snapshotMgr->getCurrent();
    else if (base == "default")
        base = pImpl->snapshotMgr->getDefault();
    if (!description)
        description = "Snapshot Update of #" + base;
    pImpl->snapshot = pImpl->snapshotMgr->create(base, description.value());

    tulog.info("Using snapshot " + base + " as base for new snapshot " + pImpl->snapshot->getUid() + ".");

    pImpl->snapMount();
    pImpl->addSupplements();
    if (pImpl->discardIfNoChange) {
        std::unique_ptr<Snapshot> prevSnap = pImpl->snapshotMgr->open(base);
        std::unique_ptr<Mount> oldEtc{new Mount{prevSnap->getRoot() / "etc"}};
        if (oldEtc->getFilesystem() == "overlayfs") {
            tulog.info("Can not merge back changes in /etc into old overlayfs system - ignoring 'discardIfNoChange'.");
        } else {
            // Flag file to indicate this snapshot was initialized with discard flag
            std::ofstream output(getRoot() / "discardIfNoChange");
            output << base;
            output.close();
        }
    }

    TransactionalUpdate::Plugins plugins_with_transaction{this};
    plugins_with_transaction.run("init-post", nullptr);
}

void Transaction::resume(std::string id) {
    TransactionalUpdate::Plugins plugins{nullptr};
    plugins.run("resume-pre", id);

    pImpl->snapshot = pImpl->snapshotMgr->open(id);
    if (! pImpl->snapshot->isInProgress()) {
        pImpl->snapshot.reset();
        throw std::invalid_argument{"Snapshot " + id + " is not an open transaction."};
    }
    pImpl->snapMount();
    pImpl->addSupplements();
    if (fs::exists(getRoot() / "discardIfNoChange")) {
        pImpl->discardIfNoChange = true;
    }

    TransactionalUpdate::Plugins plugins_with_transaction{this};
    plugins_with_transaction.run("resume-post", nullptr);
}

void Transaction::setDiscardIfUnchanged(bool discard) {
    pImpl->discardIfNoChange = discard;
}

int Transaction::impl::inotifyRead() {
    const size_t bufLen = sizeof(struct inotify_event) + NAME_MAX + 1;
    char buf[bufLen] __attribute__((aligned(8)));
    ssize_t numRead;
    int ret;

    struct pollfd pfd = {inotifyFd, POLLIN, 0};
    ret = (poll(&pfd, 1, 500));
    if (ret == -1) {
        throw std::runtime_error{"Polling inotify file descriptor failed: " + std::string(strerror(errno))};
    } else if (ret > 0) {
        numRead = read(inotifyFd, buf, bufLen);
        if (numRead == 0)
            throw std::runtime_error{"Read() from inotify fd returned 0!"};
        if (numRead == -1)
            throw std::runtime_error{"Reading from inotify fd failed: " + std::string(strerror(errno))};
        tulog.debug("inotify: Exiting after event on descriptor number ", ((struct inotify_event *)buf)->wd, " in ", ((struct inotify_event *)buf)->name);
    }
    return ret;
}

int Transaction::impl::runCommand(char* argv[], bool inChroot, std::string* output) {
    if (discardIfNoChange) {
        inotifyFd = inotify_init();
        if (inotifyFd == -1)
            throw std::runtime_error{"Couldn't initialize inotify."};

        // Recursively register all directories of the root file system
        inotifyExcludes = MountList::getList(snapshot->getRoot());
        inotifyExcludes.push_back(snapshot->getRoot() / "etc");
        nftw(snapshot->getRoot().c_str(), inotifyAdd, 20, FTW_MOUNT | FTW_PHYS);
    }

    std::string opts = "Executing `";
    int i = 0;
    while (argv[i]) {
        if (i > 0)
            opts.append(" ");
        opts.append(argv[i]);
        i++;
    }
    opts.append("`:");
    tulog.info(opts);

    int status = 1;
    int ret;
    int pipefd[2];

    ret = pipe(pipefd);
    if (ret < 0) {
        throw std::runtime_error{"Error opening pipe for command output: " + std::string(strerror(errno))};
    }

    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error{"fork() failed: " + std::string(strerror(errno))};
    } else if (pid == 0) {
        if (output != nullptr) {
            ret = dup2(pipefd[1], STDOUT_FILENO);
            if (ret < 0) {
                tulog.error("Redirecting stdout failed: " + std::string(strerror(errno)));
                _exit(errno);
            }
            ret = dup2(pipefd[1], STDERR_FILENO);
            if (ret < 0) {
                tulog.error("Redirecting stderr failed: " + std::string(strerror(errno)));
                _exit(errno);
            }
            ret = close(pipefd[0]);
            if (ret < 0) {
                tulog.error("Closing pipefd failed: " + std::string(strerror(errno)));
                _exit(errno);
            }
        }

        if (inChroot) {
            auto currentPathRel = std::filesystem::current_path().relative_path();
            if (!std::filesystem::exists(bindDir / currentPathRel) || chdir((bindDir / currentPathRel).c_str()) < 0) {
                if (chdir(bindDir.c_str()) < 0) {
                    tulog.info("Warning: Couldn't set working directory: ", std::string(strerror(errno)));
                }
            }
            if (chroot(bindDir.c_str()) < 0) {
                tulog.error("Chrooting to " + bindDir.native() + " failed: " + std::string(strerror(errno)));
                _exit(errno);
            }
            // Prevent mounts from within the chroot environment influence the tukit organized mounts
            if (unshare(CLONE_NEWNS) < 0) {
                tulog.error("Creating new mount namespace failed: " + std::string(strerror(errno)));
                _exit(errno);
            }
            if (mount("none", "/", NULL, MS_REC|MS_PRIVATE, NULL) < 0) {
                tulog.error("Setting private mount for command execution failed: " + std::string(strerror(errno)));
                _exit(errno);
            }
        }

        // Set indicator for RPM pre/post sections to detect whether we run in a
        // transactional update
        if (setenv("TRANSACTIONAL_UPDATE", "true", 1) < 0) {
            tulog.error("Setting environment variable TRANSACTIONAL_UPDATE failed: " + std::string(strerror(errno)));
            _exit(errno);
        }
        if (setenv("TRANSACTIONAL_UPDATE_ROOT", snapshot->getRoot().c_str(), 1)) {
            tulog.error("Setting environment variable TRANSACTIONAL_UPDATE_ROOT failed: " + std::string(strerror(errno)));
            _exit(errno);
        }

        if (execvp(argv[0], (char* const*)argv) < 0) {
            tulog.error("Calling " + std::string(argv[0]) + " failed: " + std::string(strerror(errno)));
            _exit(errno);
        }
        ret = -1;
    } else {
        ret = close(pipefd[1]);
        if (ret < 0) {
            throw std::runtime_error{"Closing pipefd failed: " + std::string(strerror(errno))};
        }
        if (output != nullptr) {
            char buffer[2048];
            ssize_t len;
            while((len = read(pipefd[0], buffer, 2048)) > 0) {
                output->append(buffer, len);
            }
        }

        this->pidCmd = pid;
        ret = waitpid(pid, &status, 0);
        this->pidCmd = 0;
        if (ret < 0) {
            throw std::runtime_error{"waitpid() failed: " + std::string(strerror(errno))};
        } else {
            if (WIFEXITED(status)) {
                ret = WEXITSTATUS(status);
                tulog.info("Application returned with exit status ", ret, ".");
            }
            if (WIFSIGNALED(status)) {
                ret = WTERMSIG(status);
                tulog.info("Application was terminated by signal ", ret, ".");
            }
        }
    }
    return ret;
}

int Transaction::execute(char* argv[], std::string* output) {
    TransactionalUpdate::Plugins plugins{this};
    plugins.run("execute-pre", argv);
    int status = this->pImpl->runCommand(argv, true, output);
    plugins.run("execute-post", argv);
    return status;
}

int Transaction::callExt(char* argv[], std::string* output) {
    for (int i=0; argv[i] != nullptr; i++) {
        std::string s = std::string(argv[i]);
        std::string from = "{}";
        // replacing all {} by bindDir
        for(size_t pos = 0;
            (pos = s.find(from, pos)) != std::string::npos;
            pos += getRoot().string().length())
            s.replace(pos, from.size(), this->pImpl->bindDir);
        argv[i] = strdup(s.c_str());
    }

    TransactionalUpdate::Plugins plugins{this};
    plugins.run("callExt-pre", argv);
    int status = this->pImpl->runCommand(argv, false, output);
    plugins.run("callExt-post", argv);
    return status;
}

void Transaction::sendSignal(int signal) {
    if (pImpl->pidCmd != 0) {
        if (kill(pImpl->pidCmd, signal) < 0) {
            throw std::runtime_error{"Could not send signal " + std::to_string(signal) + " to process " + std::to_string(pImpl->pidCmd) + ": " + std::string(strerror(errno))};
        }
    }
}

void Transaction::finalize() {
    TransactionalUpdate::Plugins plugins{this};
    plugins.run("finalize-pre", nullptr);

    sync();
    if (pImpl->discardIfNoChange &&
            ((inotifyFd != 0 && pImpl->inotifyRead() == 0) ||
            (inotifyFd == 0 && fs::exists(getRoot() / "discardIfNoChange")))) {
        tulog.info("No changes to the root file system - discarding snapshot.");

        // Even if the snapshot itself does not contain any changes, /etc may do so. If the new snapshot is a
        // direct descendant of the currently running system, then merge the changes back into the currently
        // running system directly and delete the snapshot. Otherwise merge it back into the previous overlay
        // (using rsync instead of a plain copy to preserve xattrs).
        std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
        if (mntEtc->isMount()) {
            std::filesystem::path targetRoot = "/";
            std::string base;

            std::ifstream input(getRoot() / "discardIfNoChange");
            input >> base;
            input.close();

            std::unique_ptr<Mount> previousEtc{new Mount("/etc", 0, true)};
            if (pImpl->snapshotMgr->getCurrent() == base) {
                tulog.info("Merging changes in /etc into the running system.");
            } else {
                tulog.info("Merging changes in /etc into the previous snapshot.");
                targetRoot = pImpl->snapshotMgr->open(base)->getRoot();
            }
            Util::exec("rsync --archive --inplace --xattrs --acls --exclude 'fstab' --delete --quiet '" + this->pImpl->bindDir.native() + "/etc/' " + targetRoot.native() + "/etc");
        }

	TransactionalUpdate::Plugins plugins_without_transaction{nullptr};
	plugins_without_transaction.run("finalize-post", pImpl->snapshot->getUid() + " " + "discarded");
        return;
    }
    if (fs::exists(getRoot() / "discardIfNoChange")) {
        fs::remove(getRoot() / "discardIfNoChange");
    }

    // Update /usr timestamp to support system offline update mechanism
    if (utime((pImpl->snapshot->getRoot() / "usr").c_str(), nullptr) != 0)
        throw std::runtime_error{"Updating /usr timestamp failed: " + std::string(strerror(errno))};

    pImpl->snapshot->close();
    pImpl->supplements.cleanup();
    pImpl->dirsToMount.clear();

    std::unique_ptr<Snapshot> defaultSnap = pImpl->snapshotMgr->open(pImpl->snapshotMgr->getDefault());
    if (defaultSnap->isReadOnly())
        pImpl->snapshot->setReadOnly(true);
    pImpl->snapshot->setDefault();
    tulog.info("New default snapshot is #" + pImpl->snapshot->getUid() + " (" + std::string(pImpl->snapshot->getRoot()) + ").");

    std::string id = pImpl->snapshot->getUid();
    pImpl->snapshot.reset();

    TransactionalUpdate::Plugins plugins_without_transaction{nullptr};
    plugins_without_transaction.run("finalize-post", id);
}

void Transaction::keep() {
    TransactionalUpdate::Plugins plugins{this};
    plugins.run("keep-pre", nullptr);

    sync();
    if (fs::exists(pImpl->snapshot->getRoot() / "discardIfNoChange") && (inotifyFd != 0 && pImpl->inotifyRead() > 0)) {
        tulog.debug("Snapshot was changed, removing discard flagfile.");
        fs::remove(pImpl->snapshot->getRoot() / "discardIfNoChange");
    }
    pImpl->supplements.cleanup();
    std::string id = pImpl->snapshot->getUid();
    pImpl->snapshot.reset();

    TransactionalUpdate::Plugins plugins_without_transaction{nullptr};
    plugins_without_transaction.run("keep-post", id);
}
