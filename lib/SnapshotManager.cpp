/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Factory / interface class for snapshot management;
  implementations can be found in the "Snapshot" directory
 */

#include "Configuration.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Snapshot/Snapper.hpp"
#include "Snapshot/Podman.hpp"
#include "Snapshot/Directory.hpp"
#include "StateStore.hpp"
#include "Util.hpp"

using namespace std;

namespace TransactionalUpdate {

unique_ptr<SnapshotManager> SnapshotFactory::get() {
    auto sm = config.get("SNAPSHOT_MANAGER");
    if (sm == "auto") {
        if (filesystem::exists("/usr/bin/snapper"))
            sm = "snapper";
        else if (filesystem::exists("/usr/bin/podman"))
            sm = "podman";
        else
            throw runtime_error{"No snapshot manager found using 'auto'."};
    }

    tulog.info("Using '" + sm + "' as snapshot manager.");
    if (sm == "snapper") {
        return make_unique<Snapper>();
    } else if (sm == "podman") {
        return make_unique<Podman>();
    } else if (sm == "directory") {
        return make_unique<Directory>();
    } else {
        throw runtime_error{"Unsupported snapshot manager '" + sm + "'."};
    }
}

void SnapshotManager::cleanupSnap(std::string snapid, bool important, std::string statevar) {
    StateStore tmpState = StateStore(config.get("TMP_STATE_FILE"));
    if (snapid != getCurrent() && snapid != tmpState.get("LAST_BOOTED") && snapid != getDefault()) {
        try {
            tulog.info("Cleaning up snapshot " + snapid + "...");
            auto snap = open(snapid);
            snap->cleanup(important);
        } catch (const invalid_argument) {
            // Ignore - it's deleted already
        } catch (const ExecutionException &e) {
            tulog.error("ERROR: Cleaning up snapshot " + snapid + " failed: " + e.what());
            if (! statevar.empty()) {
                // Keep snapshot in the list so that the admin has a chance to see the error
                state.add(statevar, snapid);
            }
        }
    } else if (! statevar.empty()) {
        state.add(statevar, snapid);
    }
}

void SnapshotManager::cleanup() {
    string snapshot;

    stringstream wsnapshots(state.get("LAST_WORKING_SNAPSHOTS"));
    state.set("LAST_WORKING_SNAPSHOTS", "");
    while (wsnapshots >> snapshot) {
        cleanupSnap(snapshot, true, "LAST_WORKING_SNAPSHOTS");
    }

    stringstream usnapshots(state.get("UNUSED_SNAPSHOTS"));
    state.set("UNUSED_SNAPSHOTS", "");
    while (usnapshots >> snapshot) {
        cleanupSnap(snapshot, false, "UNUSED_SNAPSHOTS");
    }

    if ((typeid(*this) == typeid(Snapper)) || (typeid(*this) == typeid(Podman))) {
        stringstream psnapshots(Util::exec("snapper --csvout list --columns number,userdata | grep 'transactional-update-in-progress=yes' | cut -d , -f 1"));
        while (psnapshots >> snapshot) {
            cleanupSnap(snapshot, false, "");
        }
    }

    state.persist();
}

string SnapshotManager::rollbackTo(std::string id) {
    std::string newDefaultId = rollback(id);
    if (! state.contains("LAST_WORKING_SNAPSHOTS", newDefaultId)) {
        auto snapshot = open(id);
        state.add("UNUSED_SNAPSHOTS", newDefaultId);
        state.persist(snapshot->getRoot());
    }
    return newDefaultId;
}

} // namespace TransactionalUpdate
