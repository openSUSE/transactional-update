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
#include "SnapshotManager.hpp"
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
#include <sched.h>
#include <signal.h>
#include <sys/inotify.h>
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
    void mount();
    int runCommand(char* argv[], bool inChroot, std::string* buffer);
    static int inotifyAdd(const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb);
    int inotifyRead();
    std::unique_ptr<SnapshotManager> snapshotMgr;
    std::unique_ptr<Snapshot> snapshot;
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
    if (unshare(CLONE_NEWNS) < 0) {
        throw std::runtime_error{"Creating new mount namespace failed: " + std::string(strerror(errno))};
    }

    // GRUB needs to have an actual mount point for the root partition, so
    // mount the snapshot directory on a temporary mount point
    std::unique_ptr<BindMount> mntBind{new BindMount{snapshot->getRoot(), MS_UNBINDABLE}};
    mntBind->setSource(snapshot->getRoot());
    mntBind->mount();

    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/dev"));
    dirsToMount.push_back(std::make_unique<BindMount>("/var/log"));

    std::vector<std::string> customDirs = config.getArray("BINDDIRS");
    for (auto it = customDirs.begin(); it != customDirs.end(); ++it) {
        dirsToMount.push_back(std::make_unique<BindMount>(*it));
    }

    Mount mntVar{"/var"};
    if (mntVar.isMount()) {
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
    for (auto& path: fs::directory_iterator("/boot/grub2")) {
        if (fs::is_directory(path)) {
            if (BindMount{path.path()}.isMount())
                dirsToMount.push_back(std::make_unique<BindMount>(path.path()));
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

    dirsToMount.push_back(std::make_unique<BindMount>("/.snapshots"));

    for (auto it = dirsToMount.begin(); it != dirsToMount.end(); ++it) {
        it->get()->mount(snapshot->getRoot());
    }

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

void Transaction::init(std::string base = "active") {
    if (base == "active")
        base = pImpl->snapshotMgr->getCurrent();
    else if (base == "default")
        base = pImpl->snapshotMgr->getDefault();
    pImpl->snapshot = pImpl->snapshotMgr->create(base);

    tulog.info("Using snapshot " + base + " as base for new snapshot " + pImpl->snapshot->getUid() + ".");

    // Create /etc overlay
    std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
    if (mntEtc->isMount() && mntEtc->getFilesystem() == "overlay") {
        Overlay overlay = Overlay{pImpl->snapshot->getUid()};
        overlay.create(base, pImpl->snapshot->getUid(), getRoot());
        overlay.setMountOptions(mntEtc);
        // Copy current fstab into root in case the user modified it
        if (fs::exists(fs::path{overlay.lowerdirs[0] / "fstab"})) {
            fs::copy(fs::path{overlay.lowerdirs[0] / "fstab"}, fs::path{getRoot() / "etc"}, fs::copy_options::overwrite_existing);
        }

        mntEtc->persist(getRoot() / "etc" / "fstab");

        // Make sure both the snapshot and the overlay contain all relevant fstab data, i.e.
        // user modifications from the overlay are present in the root fs and the /etc
        // overlay is visible in the overlay
        fs::copy(fs::path{getRoot() / "etc" / "fstab"}, overlay.upperdir, fs::copy_options::overwrite_existing);
    }

    pImpl->mount();
    pImpl->addSupplements();
    if (pImpl->discardIfNoChange) {
        // Flag file to indicate this snapshot was initialized with discard flag
        std::ofstream output(getRoot() / "discardIfNoChange");
    }
}

void Transaction::resume(std::string id) {
    pImpl->snapshot = pImpl->snapshotMgr->open(id);
    if (! pImpl->snapshot->isInProgress()) {
        pImpl->snapshot.reset();
        throw std::invalid_argument{"Snapshot " + id + " is not an open transaction."};
    }
    pImpl->mount();
    pImpl->addSupplements();
    if (fs::exists(getRoot() / "discardIfNoChange")) {
        pImpl->discardIfNoChange = true;
    }
}

void Transaction::setDiscardIfUnchanged(bool discard) {
    pImpl->discardIfNoChange = discard;
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
                throw std::runtime_error{"Redirecting stdout failed: " + std::string(strerror(errno))};
            }
            ret = dup2(pipefd[1], STDERR_FILENO);
            if (ret < 0) {
                throw std::runtime_error{"Redirecting stderr failed: " + std::string(strerror(errno))};
            }
            ret = close(pipefd[0]);
            if (ret < 0) {
                throw std::runtime_error{"Closing pipefd failed: " + std::string(strerror(errno))};
            }
        }

        if (inChroot) {
            if (chdir(snapshot->getRoot().c_str()) < 0) {
                tulog.info("Warning: Couldn't set working directory: ", std::string(strerror(errno)));
            }
            if (chroot(snapshot->getRoot().c_str()) < 0) {
                throw std::runtime_error{"Chrooting to " + std::string(snapshot->getRoot()) + " failed: " + std::string(strerror(errno))};
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
    return this->pImpl->runCommand(argv, true, output);
}

int Transaction::callExt(char* argv[], std::string* output) {
    for (int i=0; argv[i] != nullptr; i++) {
        std::string s = std::string(argv[i]);
        std::string from = "{}";
        // replacing all {} by bindDir
        for(size_t pos = 0;
           (pos = s.find(from, pos)) != std::string::npos;
           pos += getRoot().string().length())
            s.replace(pos, from.size(), getRoot());
        argv[i] = strdup(s.c_str());
    }
    return this->pImpl->runCommand(argv, false, output);
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
            (inotifyFd == 0 && fs::exists(getRoot() / "discardIfNoChange")))) {
        tulog.info("No changes to the root file system - discarding snapshot.");

        // Even if the snapshot itself did not contain any changes, /etc may do so. Changes
        // in /etc may be applied immediately, so merge them back into the running system.
        std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
        if (mntEtc->isMount() && mntEtc->getFilesystem() == "overlay") {
            Util::exec("rsync --archive --inplace --xattrs --acls --exclude 'fstab' --delete --quiet '" + std::string(getRoot()) + "/etc/' /etc");
        }
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
