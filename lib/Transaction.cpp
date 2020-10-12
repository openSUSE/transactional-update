/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  The Transaction class is the central API class for the lifecycle of a
  transaction: It will open and close transactions and execute commands in the
  correct context.
 */

#include "Transaction.h"
#include "Configuration.h"
#include "Log.h"
#include "Overlay.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>
using namespace TransactionalUpdate;
namespace fs = std::filesystem;

class Transaction::impl {
public:
    void addSupplements();
    void mount(std::string base = "");
    std::unique_ptr<Snapshot> snapshot;
    std::string bindDir;
    std::vector<std::unique_ptr<Mount>> dirsToMount;
    Supplements supplements;
};

Transaction::Transaction() : pImpl{std::make_unique<impl>()} {
    tulog.debug("Constructor Transaction");
    pImpl->snapshot = SnapshotFactory::get();
}

Transaction::~Transaction() {
    tulog.debug("Destructor Transaction");

    pImpl->supplements.cleanup();

    pImpl->dirsToMount.clear();
    try {
        fs::remove_all(fs::path{pImpl->bindDir});
        if (isInitialized())
            pImpl->snapshot->abort();
    }  catch (const std::exception &e) {
        tulog.error("ERROR: ", e.what());
    }
}

bool Transaction::isInitialized() {
    return pImpl->snapshot ? true : false;
}

std::string Transaction::getSnapshot()
{
    return pImpl->snapshot->getUid();
}

void Transaction::impl::mount(std::string base) {
    dirsToMount.push_back(std::make_unique<PropagatedBindMount>("/dev"));
    dirsToMount.push_back(std::make_unique<BindMount>("/var/log"));

    Mount mntVar{"/var"};
    if (mntVar.isMount()) {
        dirsToMount.push_back(std::make_unique<BindMount>("/var/cache"));
        dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/alternatives"));
        dirsToMount.push_back(std::make_unique<BindMount>("/var/lib/ca-certificates", MS_RDONLY));
    }
    std::unique_ptr<Mount> mntEtc{new Mount{"/etc"}};
    if (mntEtc->isMount() && mntEtc->getFS() == "overlay") {
        Overlay overlay = Overlay{snapshot->getUid()};
        overlay.create(base);

        overlay.setMountOptions(mntEtc);
        mntEtc->persist(snapshot->getRoot() / "etc" / "fstab");
        overlay.setMountOptionsForMount(mntEtc);

        overlay.sync(snapshot->getRoot());

        dirsToMount.push_back(std::move(mntEtc));

        // Make sure both the snapshot and the overlay contain all relevant fstab data, i.e.
        // user modifications from the overlay are present in the root fs and the /etc
        // overlay is visible in the overlay
        fs::copy(fs::path{snapshot->getRoot() / "etc" / "fstab"}, overlay.upperdir, fs::copy_options::overwrite_existing);
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
        supplements.addFile(fs::path{"/var/lib/zypp/RequestedLocales"}); // locale specific packages with zypper
        supplements.addLink(fs::path{"/run"}, fs::path{"/var/run"});
    }
    supplements.addLink(fs::path{"/usr/lib/sysimage/rpm"}, fs::path{"/var/lib/rpm"});
    supplements.addFile(fs::path{"/run/netconfig"});
    supplements.addDir(fs::path{"/var/cache/zypp"});
    supplements.addDir(fs::path{"/var/spool"});
}

void Transaction::init(std::string base) {
    if (base == "active")
        base = pImpl->snapshot->getCurrent();
    else if (base == "default")
        base = pImpl->snapshot->getDefault();
    pImpl->snapshot->create(base);

    pImpl->mount(base);
    pImpl->addSupplements();
}

void Transaction::resume(std::string uuid) {
    pImpl->snapshot->open(uuid);
    pImpl->mount();
}

int Transaction::execute(const char* argv[]) {
    if (! pImpl->snapshot->isInProgress())
        throw std::invalid_argument{"Snapshot " + pImpl->snapshot->getUid() + " is not an open transaction."};

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
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error{"fork() failed: " + std::string(strerror(errno))};
    } else if (pid == 0) {
        if (chroot(pImpl->bindDir.c_str()) < 0) {
            throw std::runtime_error{"Chrooting to " + pImpl->bindDir + " failed: " + std::string(strerror(errno))};
        }
        // Set indicator for RPM pre/post sections to detect whether we run in a
        // transactional update
        setenv("TRANSACTIONAL_UPDATE", "true", 1);
        std::cout << "◸" << std::flush;
        if (execvp(argv[0], (char* const*)argv) < 0) {
            throw std::runtime_error{"Calling " + std::string(argv[0]) + " failed: " + std::string(strerror(errno))};
        }
    } else {
        int ret;
        ret = waitpid(pid, &status, 0);
        std::cout << "◿" << std::endl;
        if (ret < 0) {
            throw std::runtime_error{"waitpid() failed: " + std::string(strerror(errno))};
        } else {
            tulog.info("Application returned with exit status ", WEXITSTATUS(status), ".");
        }
    }
    return WEXITSTATUS(status);
}

void Transaction::finalize() {
    pImpl->snapshot->close();

    std::unique_ptr<Snapshot> defaultSnap = SnapshotFactory::get();
    defaultSnap->open(pImpl->snapshot->getDefault());
    if (defaultSnap->isReadOnly())
        pImpl->snapshot->setReadOnly(true);

    pImpl->snapshot.reset();
}

void Transaction::keep() {
    pImpl->snapshot.reset();
}
