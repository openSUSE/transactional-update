/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Handling of /etc overlayfs layers
 */

#include "Overlay.h"

#include "Configuration.h"
#include "Log.h"
#include "Mount.h"
#include "Snapshot.h"
#include "Util.h"
#include <filesystem>
#include <regex>
#include <selinux/selinux.h>
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
        upperdir(fs::path{config.get("OVERLAY_DIR")} / snapshot / "etc"),
        workdir(fs::path{config.get("OVERLAY_DIR")} / snapshot / "work-etc")
{
    fs::create_directories(fs::path{workdir});

    // Read lowerdirs if this is an existing snapshot
    Mount mntEtc{"/etc"};
    mntEtc.setTabSource(upperdir / "fstab");
    try {
        const string fstabLowerdirs = mntEtc.getOption("lowerdir");
        string lowerdir;
        stringstream ss(fstabLowerdirs);
        while (getline(ss, lowerdir, ':')) {
            lowerdir = regex_replace(lowerdir, std::regex("^" + config.get("DRACUT_SYSROOT")), "");
            lowerdirs.push_back(lowerdir);
        }
    } catch (exception e) {}
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

void Overlay::sync(string base, string snapshot) {
    Overlay baseOverlay = Overlay{base};
    auto previousSnapId = baseOverlay.getPreviousSnapshotOvlId();
    if (previousSnapId.empty())
        return;

    unique_ptr<Snapshot> previousSnapshot = SnapshotFactory::get();
    previousSnapshot->open(previousSnapId);
    unique_ptr<Mount> previousEtc{new Mount("/etc")};
    previousEtc->setTabSource(previousSnapshot->getRoot() / "etc" / "fstab");

    // Mount read-only, so mount everything as lowerdir
    Overlay previousOvl{previousSnapId};
    previousOvl.lowerdirs.insert(previousOvl.lowerdirs.begin(), previousOvl.upperdir);
    previousOvl.setMountOptionsForMount(previousEtc);
    previousEtc->removeOption("upperdir");

    string syncSource = string(previousOvl.upperdir.parent_path() / "sync" / "etc") + "/";
    string rsyncExtraArgs;
    previousEtc->mount(previousOvl.upperdir.parent_path() / "sync");
    tulog.info("Syncing /etc of previous snapshot ", previousSnapId, " as base into new snapshot ", snapshot);
    if (is_selinux_enabled()) {
        // Ignore the SELinux attributes when synchronizing pre-SELinux files,
        // rsync will fail otherwise
        char* context;
        if (getfilecon(syncSource.c_str(), &context) > 0 && strcmp(context, "unlabeled_t") != 0) {
            rsyncExtraArgs = "--filter=-x security.selinux";
        }
    }
    Util::exec("rsync --quiet --archive --inplace --xattrs --exclude='/fstab' " + rsyncExtraArgs + " --acls --delete " + syncSource + " " + snapshot + "/etc");
}

void Overlay::setMountOptions(unique_ptr<Mount>& mount) {
    string lower;
    for (auto lowerdir: lowerdirs) {
        if (! lower.empty())
            lower.append(":");
        lower.append(config.get("DRACUT_SYSROOT") / lowerdir.relative_path());
    }
    if (lower.length() >= sysconf(_SC_PAGE_SIZE)) {
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
        } else
        // Replace /etc in lowerdir with /etc of overlay base
        if (lowerdir == "/etc") {
            std::unique_ptr<Snapshot> snap = SnapshotFactory::get();
            snap->open(getIdOfOverlayDir(upperdir));
            lower.append(snap->getRoot() / "etc");
        } else {
            lower.append(lowerdir);
        }
    }
    mount->setOption("lowerdir", lower);
    mount->setOption("upperdir", upperdir);
    mount->setOption("workdir", workdir);
}

void Overlay::create(string base, string snapshot) {
    Overlay parent = Overlay{base};

    // Remove overlay directory if it already exists (e.g. after the snapshot was deleted)
    fs::remove_all(upperdir);
    fs::create_directories(upperdir);

    // Assemble the new lowerdirs
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
        sync(base, snapshot);
    }
}

} // namespace TransactionalUpdate
