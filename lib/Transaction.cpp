/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  The Transaction class is the central API class for the lifecycle of a
  transaction: It will open and close transactions and execute commands in the
  correct context.
 */

#include "Transaction.hpp"
#include "Configuration.hpp"
#include "Log.hpp"
#include "Mount.hpp"
#include "Overlay.hpp"
#include "Snapshot.hpp"
#include "Supplement.hpp"
#include "Util.hpp"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ftw.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
using namespace TransactionalUpdate;
namespace fs = std::filesystem;

static int inotifyFd;

class Transaction::impl {
public:
    void addSupplements();
    void mount();
    int runCommand(char* argv[], bool inChroot);
    static int inotifyAdd(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb);
    int inotifyRead();
    std::unique_ptr<Snapshot> snapshot;
    std::string bindDir;
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
    pImpl->snapshot = SnapshotFactory::get();
}

Transaction::~Transaction() {
    tulog.debug("Destructor Transaction");

    if (inotifyFd != 0)
        close(inotifyFd);

    pImpl->dirsToMount.clear();
    if (!pImpl->bindDir.empty()) {
        try {
            fs::remove(fs::path{pImpl->bindDir});
        }  catch (const std::exception &e) {
            tulog.error("ERROR: ", e.what());
        }
    }
    try {
        if (isInitialized() && !getSnapshot().empty() && fs::exists(getRoot())) {
            tulog.info("Discarding snapshot ", pImpl->snapshot->getUid(), ".");
            pImpl->snapshot->abort();
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

void Transaction::impl::mount() {
    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/dev"));
    dirsToMount.push_back(std::make_unique<BindMount>("/var/log"));
    dirsToMount.push_back(std::make_unique<BindMount>("/opt"));

    Mount mntVar{"/var"};
    if (mntVar.isMount()) {
        dirsToMount.push_back(std::make_unique<BindMount>("/var/cache"));
        if (fs::is_directory("/var/lib/zypp"))
            dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/zypp"));
        dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/ca-certificates"));
        if (fs::is_directory("/var/lib/alternatives"))
            dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/alternatives"));
        if (fs::is_directory("/var/lib/selinux"))
            dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/selinux"));
    }
    std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
    if (mntEtc->isMount() && mntEtc->getFilesystem() == "overlay") {
        Overlay overlay = Overlay{snapshot->getUid()};
        overlay.setMountOptionsForMount(mntEtc);
        dirsToMount.push_back(std::move(mntEtc));
    }

    // Mount platform specific GRUB directories for GRUB updates
    for (auto& path: fs::directory_iterator("/boot/grub2")) {
        if (fs::is_directory(path)) {
            if (BindMount{path.path()}.isMount())
                dirsToMount.push_back(std::make_unique<BindMount>(path.path()));
        }
    }
    if (BindMount{"/boot/efi"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/boot/efi"));

    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/proc"));
    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/sys"));

    if (BindMount{"/root"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/root"));

    if (BindMount{"/boot/writable"}.isMount())
        dirsToMount.push_back(std::make_unique<BindMount>("/boot/writable"));

    dirsToMount.push_back(std::make_unique<BindMount>("/.snapshots"));

    for (auto it = dirsToMount.begin(); it != dirsToMount.end(); ++it) {
        it->get()->mount(snapshot->getRoot());
    }

    // When all mounts are set up, then bind mount everything into a temporary
    // directory - GRUB needs to have an actual mount point for the root
    // partition
    char bindTemplate[] = "/tmp/transactional-update-XXXXXX";
    bindDir = mkdtemp(bindTemplate);
    std::unique_ptr<BindMount> mntBind{new BindMount{bindDir, MS_REC}};
    mntBind->setSource(snapshot->getRoot());
    mntBind->mount();
    dirsToMount.push_back(std::move(mntBind));
}

void Transaction::impl::addSupplements() {
    supplements = Supplements(snapshot->getRoot());

    Mount mntVar{"/var"};
    if (mntVar.isMount()) {
        supplements.addDir(fs::path{"/var/tmp"});
        supplements.addLink(fs::path{"/run"}, fs::path{"/var/run"});
    }
    supplements.addLink(fs::path{"../../usr/lib/sysimage/rpm"}, fs::path{"/var/lib/rpm"});
    supplements.addFile(fs::path{"/run/netconfig"});
    supplements.addFile(fs::path{"/run/systemd/resolve"});
    if (fs::is_directory("/var/cache/dnf"))
        supplements.addDir(fs::path{"/var/cache/dnf"});
    if (fs::is_directory("/var/cache/yum"))
        supplements.addDir(fs::path{"/var/cache/yum"});
    if (fs::is_directory("/var/cache/PackageKit"))
        supplements.addDir(fs::path{"/var/cache/PackageKit"});
    if (fs::is_directory("/var/cache/zypp"))
        supplements.addDir(fs::path{"/var/cache/zypp"});
    supplements.addDir(fs::path{"/var/spool"});
}

// Callback function for nftw to register all directories for inotify
int Transaction::impl::inotifyAdd(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
    if (!(type == FTW_D))
        return 0;
    if (inotify_add_watch(inotifyFd, pathname, IN_MODIFY | IN_MOVE | IN_CREATE | IN_DELETE | IN_ATTRIB | IN_ONESHOT | IN_ONLYDIR | IN_DONT_FOLLOW) == -1)
        tulog.info("WARNING: Cannot register inotify watch for ", pathname);
    else
        tulog.debug("Watching ", pathname);
    return 0;
}

void Transaction::init(std::string base = "active") {
    if (base == "active")
        base = pImpl->snapshot->getCurrent();
    else if (base == "default")
        base = pImpl->snapshot->getDefault();
    pImpl->snapshot->create(base);

    tulog.info("Using snapshot " + base + " as base for new snapshot " + pImpl->snapshot->getUid() + ".");

    // Create /etc overlay
    std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
    if (mntEtc->isMount() && mntEtc->getFilesystem() == "overlay") {
        Overlay overlay = Overlay{pImpl->snapshot->getUid()};
        overlay.create(base, pImpl->snapshot->getUid(), pImpl->snapshot->getRoot());
        overlay.setMountOptions(mntEtc);
        // Copy current fstab into root in case the user modified it
        if (fs::exists(fs::path{overlay.lowerdirs[0] / "fstab"})) {
            fs::copy(fs::path{overlay.lowerdirs[0] / "fstab"}, fs::path{pImpl->snapshot->getRoot() / "etc"}, fs::copy_options::overwrite_existing);
        }

        mntEtc->persist(pImpl->snapshot->getRoot() / "etc" / "fstab");

        // Make sure both the snapshot and the overlay contain all relevant fstab data, i.e.
        // user modifications from the overlay are present in the root fs and the /etc
        // overlay is visible in the overlay
        fs::copy(fs::path{pImpl->snapshot->getRoot() / "etc" / "fstab"}, overlay.upperdir, fs::copy_options::overwrite_existing);
    }

    pImpl->mount();
    pImpl->addSupplements();
    if (pImpl->discardIfNoChange) {
        // Flag file to indicate this snapshot was initialized with discard flag
        std::ofstream output(pImpl->snapshot->getRoot() / "discardIfNoChange");
    }
}

void Transaction::resume(std::string id) {
    pImpl->snapshot->open(id);
    if (! pImpl->snapshot->isInProgress()) {
        pImpl->snapshot.reset();
        throw std::invalid_argument{"Snapshot " + id + " is not an open transaction."};
    }
    pImpl->mount();
    pImpl->addSupplements();
    if (fs::exists(pImpl->snapshot->getRoot() / "discardIfNoChange")) {
        pImpl->discardIfNoChange = true;
    }
}

void Transaction::setDiscardIfUnchanged(bool discard) {
    pImpl->discardIfNoChange = discard;
}
void Transaction::setDiscard(bool discard) {
    setDiscardIfUnchanged(discard);
}

int Transaction::impl::inotifyRead() {
    size_t bufLen = sizeof(struct inotify_event) + NAME_MAX + 1;
    char buf[bufLen] __attribute__((aligned(8)));
    ssize_t numRead;
    int ret;

    struct pollfd pfd = {inotifyFd, POLLIN, 0};
    ret = (poll(&pfd, 1, 500));
    if (ret == -1) {
        throw std::runtime_error{"Polling inotify file descriptior failed: " + std::string(strerror(errno))};
    } else if (ret > 0) {
        numRead = read(inotifyFd, buf, bufLen);
        if (numRead == 0)
            throw std::runtime_error{"Read() from inotify fd returned 0!"};
        if (numRead == -1)
            throw std::runtime_error{"Reading from inotify fd failed: " + std::string(strerror(errno))};
        tulog.debug("inotify: Exiting after event on ", ((struct inotify_event *)buf)->name);
    }
    return ret;
}

int Transaction::impl::runCommand(char* argv[], bool inChroot) {
    if (discardIfNoChange) {
        inotifyFd = inotify_init();
        if (inotifyFd == -1)
            throw std::runtime_error{"Couldn't initialize inotify."};

        // Recursively register all directories of the root file system
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
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error{"fork() failed: " + std::string(strerror(errno))};
    } else if (pid == 0) {
        if (inChroot) {
            if (chdir(bindDir.c_str()) < 0) {
                tulog.info("Warning: Couldn't set working directory: ", std::string(strerror(errno)));
            }
            if (chroot(bindDir.c_str()) < 0) {
                throw std::runtime_error{"Chrooting to " + bindDir + " failed: " + std::string(strerror(errno))};
            }
        }
        // Set indicator for RPM pre/post sections to detect whether we run in a
        // transactional update
        setenv("TRANSACTIONAL_UPDATE", "true", 1);
        if (execvp(argv[0], (char* const*)argv) < 0) {
            throw std::runtime_error{"Calling " + std::string(argv[0]) + " failed: " + std::string(strerror(errno))};
        }
        ret = -1;
    } else {
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

int Transaction::execute(char* argv[]) {
    return this->pImpl->runCommand(argv, true);
}

int Transaction::callExt(char* argv[]) {
    for (int i=0; argv[i] != nullptr; i++) {
        if (strcmp(argv[i], "{}") == 0) {
            char* bindDir = strdup(pImpl->bindDir.c_str());
            argv[i] = bindDir;
        }
    }
    return this->pImpl->runCommand(argv, false);
}

void Transaction::sendSignal(int signal) {
    if (pImpl->pidCmd != 0) {
        if (kill(pImpl->pidCmd, signal) < 0) {
            throw std::runtime_error{"Could not send signal " + std::to_string(signal) + " to process " + std::to_string(pImpl->pidCmd) + ": " + std::string(strerror(errno))};
        }
    }
}

void Transaction::finalize() {
    sync();
    if (pImpl->discardIfNoChange &&
            ((inotifyFd != 0 && pImpl->inotifyRead() == 0) ||
            (inotifyFd == 0 && fs::exists(pImpl->snapshot->getRoot() / "discardIfNoChange")))) {
        tulog.info("No changes to the root file system - discarding snapshot.");

        // Even if the snapshot itself did not contain any changes, /etc may do so. Changes
        // in /etc may be applied immediately, so merge them back into the running system.
        std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
        if (mntEtc->isMount() && mntEtc->getFilesystem() == "overlay") {
            Util::exec("rsync --archive --inplace --xattrs --acls --exclude 'fstab' --delete --quiet '" + std::string(pImpl->snapshot->getRoot()) + "/etc/' /etc");
        }
        return;
    }
    if (fs::exists(pImpl->snapshot->getRoot() / "discardIfNoChange")) {
        fs::remove(pImpl->snapshot->getRoot() / "discardIfNoChange");
    }

    // Update /usr timestamp to support system offline update mechanism
    if (utime((pImpl->snapshot->getRoot() / "usr").c_str(), nullptr) != 0)
        throw std::runtime_error{"Updating /usr timestamp failed: " + std::string(strerror(errno))};

    pImpl->snapshot->close();
    pImpl->supplements.cleanup();
    pImpl->dirsToMount.clear();

    std::unique_ptr<Snapshot> defaultSnap = SnapshotFactory::get();
    defaultSnap->open(pImpl->snapshot->getDefault());
    if (defaultSnap->isReadOnly())
        pImpl->snapshot->setReadOnly(true);
    pImpl->snapshot->setDefault();
    tulog.info("New default snapshot is #" + pImpl->snapshot->getUid() + " (" + std::string(pImpl->snapshot->getRoot()) + ").");

    pImpl->snapshot.reset();
}

void Transaction::keep() {
    sync();
    if (fs::exists(pImpl->snapshot->getRoot() / "discardIfNoChange") && (inotifyFd != 0 && pImpl->inotifyRead() > 0)) {
        tulog.debug("Snapshot was changed, removing discard flagfile.");
        fs::remove(pImpl->snapshot->getRoot() / "discardIfNoChange");
    }
    pImpl->supplements.cleanup();
    pImpl->snapshot.reset();
}
