/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Handling of /etc overlayfs layers
 */

#include "Overlay.hpp"

#include "Configuration.hpp"
#include "Log.hpp"
#include "Mount.hpp"
#include "Util.hpp"
#include <filesystem>
#include <regex>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <sstream>
#include <unistd.h>

using std::exception;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;
namespace fs = std::filesystem;

namespace TransactionalUpdate {

/*
 * Create a new overlay instance for the given snapshot number.
 * For existing overlays the lowerdirs are read automatically from the given snapshot overlay;
 * for this to work the snapshot still has to exist.
 * For new overlays `create` has to be called afterwards with a base.
 */
Overlay::Overlay(string snapshot):
        workdir(fs::path{config.get("OVERLAY_DIR")} / snapshot / "work-etc")
{
    fs::create_directories(fs::path{workdir});

    // Read lowerdirs
    // Always use the root file system for reading fstab, as the first snapshot overlay doesn't
    // contain an fstab in the /etc overlay yet.
    // Note: Due to this for new overlays (i.e. when "create" will be called later) the lowerdirs
    // will be initialized with outdated data of the base snapshot - it will be initalized
    // correctly during "create".
    unique_ptr<Snapshot> snap = snapMgr->open(snapshot);
    Mount mntEtc{"/etc"};
    mntEtc.setTabSource(snap->getRoot() / "etc" / "fstab");
    // Read data from fstab if this is an existing snapshot, just use the defaults otherwise
    try {
        upperdir = regex_replace(mntEtc.getOption("upperdir"), std::regex("^" + config.get("DRACUT_SYSROOT")), "");
        const string fstabLowerdirs = mntEtc.getOption("lowerdir");
        string lowerdir;
        stringstream ss(fstabLowerdirs);
        while (getline(ss, lowerdir, ':')) {
            lowerdir = regex_replace(lowerdir, std::regex("^" + config.get("DRACUT_SYSROOT")), "");
            lowerdirs.push_back(lowerdir);
        }
    } catch (exception &e) {}
}

string Overlay::getIdOfOverlayDir(const string dir) {
    std::smatch match;
    std::regex exp("^(" + config.get("DRACUT_SYSROOT") + ")?" + config.get("OVERLAY_DIR") + "/(.+)/etc$");
    if (regex_search(dir.begin(), dir.end(), match, exp)) {
        return match[2];
    }
    return "";
}

string Overlay::getPreviousSnapshotOvlId() {
    for (auto it = lowerdirs.begin(); it != lowerdirs.end(); it++) {
        string id = getIdOfOverlayDir(*it);
        if (! id.empty())
            return id;
    }
    return "";
}

bool Overlay::references(string snapshot) {
    for (auto it = lowerdirs.begin(); it != lowerdirs.end(); it++) {
        string id = getIdOfOverlayDir(*it);
        if (id == snapshot)
            return true;
    }
    return false;
}

void Overlay::sync(string base, fs::path snapRoot) {
    Overlay baseOverlay = Overlay{base};
    auto previousSnapId = baseOverlay.getPreviousSnapshotOvlId();
    if (previousSnapId.empty()) {
        tulog.info("No previous snapshot to sync with - skipping");
        return;
    }

    unique_ptr<Snapshot> previousSnapshot;
    try {
        previousSnapshot = snapMgr->open(previousSnapId);
    } catch (std::invalid_argument &e) {
        tulog.info("Parent snapshot ", previousSnapId, " does not exist any more - skipping rsync");
        return;
    }
    unique_ptr<Mount> previousEtc{new Mount("/etc")};
    previousEtc->setTabSource(previousSnapshot->getRoot() / "etc" / "fstab");

    // Mount read-only, so mount everything as lowerdir
    Overlay previousOvl{previousSnapId};
    previousOvl.lowerdirs.insert(previousOvl.lowerdirs.begin(), previousOvl.upperdir);
    previousOvl.setMountOptionsForMount(previousEtc);
    previousEtc->removeOption("upperdir");
    previousEtc->removeOption("workdir");

    string syncSource = string(previousOvl.upperdir.parent_path() / "sync" / "etc") + "/";
    string rsyncExtraArgs;
    previousEtc->mount(previousOvl.upperdir.parent_path() / "sync");
    tulog.info("Syncing /etc of previous snapshot ", previousSnapId, " as base into new snapshot ", snapRoot);

    if (is_selinux_enabled()) {
        tulog.info("SELinux is enabled.");
    }

    try {
        Util::exec("rsync --quiet --archive --inplace --xattrs --exclude='/fstab' --acls --delete " + syncSource + " " + string(snapRoot) + "/etc 2>&1");
    } catch (exception &e) {
        // rsync will fail when synchronizing pre-SELinux snapshots as soon as SELinux enabled,
        // so try again without the SELinux xattrs.
        tulog.info("Retrying rsync without SELinux xattrs...");
        Util::exec("rsync --quiet --archive --inplace --xattrs --filter='-x security.selinux' --exclude='/fstab' --acls --delete " + syncSource + " " + string(snapRoot) + "/etc");
    }
}

void Overlay::setMountOptions(unique_ptr<Mount>& mount) {
    string lower;
    for (auto lowerdir: lowerdirs) {
        if (! lower.empty())
            lower.append(":");
        lower.append(config.get("DRACUT_SYSROOT") / lowerdir.relative_path());
    }
    long pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize > 0 && lower.length() >= reinterpret_cast<unsigned long&>(pagesize)) {
        throw std::runtime_error{"Exceeding maximum length of mount options; please boot into the new snapshot before proceeding."};
    }
    mount->setOption("lowerdir", lower);
    mount->setOption("upperdir", config.get("DRACUT_SYSROOT") / upperdir.relative_path());
    mount->setOption("workdir", config.get("DRACUT_SYSROOT") / workdir.relative_path());
}

/* Mount all layers mentioned as lowerdirs... */
void Overlay::setMountOptionsForMount(unique_ptr<Mount>& mount) {
    string lower;
    Mount mntCurrentEtc{"/etc"};

    string currentUpper = mntCurrentEtc.getOption("upperdir");

    for (auto lowerdir: lowerdirs) {
        if (! lower.empty()) {
            lower.append(":");
        }
        // Check whether the current upper directory is part of the snapshot's lower
        // directory stack; if so use reuse /etc directly instead, as mounting
        // the same upper directory multiple times is not supported by overlayfs
        if (getIdOfOverlayDir(lowerdir) == getIdOfOverlayDir(currentUpper)) {
            lower.append("/etc");
            break;
        }
        // Replace /etc in lowerdir with /etc of overlay base
        if (lowerdir == "/etc") {
            std::unique_ptr<Snapshot> snap = snapMgr->open(getIdOfOverlayDir(upperdir));
            lower.append(snap->getRoot() / "etc");
        } else {
            lower.append(lowerdir);
        }
    }
    mount->setOption("lowerdir", lower);
    mount->setOption("upperdir", upperdir);
    mount->setOption("workdir", workdir);
}

void Overlay::create(string base, string snapshot, fs::path snapRoot) {
    upperdir = fs::path{config.get("OVERLAY_DIR")} / snapshot / "etc";
    Overlay parent = Overlay{base};

    // Remove overlay directory if it already exists (e.g. after the snapshot was deleted)
    fs::remove_all(upperdir);
    fs::create_directories(upperdir);

    char* context = NULL;
    if (getfilecon("/etc", &context) > 0) {
        tulog.debug("selinux context on /etc: " + std::string(context));
        if (setfilecon(upperdir.c_str(), context) != 0) {
            freecon(context);
            throw std::runtime_error{"applying selinux context failed: " + std::string(strerror(errno))};
        }
        freecon(context);
    }

    // Assemble the new lowerdirs
    lowerdirs.clear();
    lowerdirs.push_back(parent.upperdir);

    Mount currentEtc{"/etc"};
    string currentUpper = currentEtc.getOption("upperdir");
    // It is possible that files in /etc will be modified after the creation of the snapshot,
    // but before rebooting the system. When using the --continue option and if the snapshot is
    // based on the currently running system, then the snapshot stack has to be preserved to
    // keep the layer transparency. Otherwise just sync the previous snapshots /etc into the
    // new layer as a base.
    if (parent.references(getIdOfOverlayDir(currentUpper))) {
        for (auto it = parent.lowerdirs.begin(); it != parent.lowerdirs.end(); it++) {
            lowerdirs.push_back(*it);
        }
    } else {
        lowerdirs.push_back(parent.lowerdirs.back());
        sync(base, snapRoot);
    }
}

} // namespace TransactionalUpdate
