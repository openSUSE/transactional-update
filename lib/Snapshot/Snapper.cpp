/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#include "Snapper.h"
#include "Exceptions.h"
#include "Log.h"
#include "Util.h"

namespace TransactionalUpdate {

void Snapper::create(std::string base) {
    snapshotId = callSnapper("create --from " + base + " --read-write --print-number --description 'Snapshot Update of #" + base + "' --userdata 'transactional-update-in-progress=yes'");
    Util::rtrim(snapshotId);
}

void Snapper::open(std::string id) {
    snapshotId = id;
    if (! std::filesystem::exists(getRoot()))
        throw std::invalid_argument{"Snapshot " + id + " does not exist."};
}

void Snapper::close() {
    callSnapper("modify -u 'transactional-update-in-progress=' " + snapshotId);
}

void Snapper::abort() {
    callSnapper("delete " + snapshotId);
}

std::filesystem::path Snapper::getRoot() {
    return std::filesystem::path("/.snapshots/" + snapshotId + "/snapshot");
}

std::string Snapper::getUid() {
    return snapshotId;
}

std::string Snapper::getCurrent() {
    std::string id = callSnapper("--csvout list --columns active,number | grep yes | cut -f 2 -d ,");
    Util::rtrim(id);
    return id;
}

std::string Snapper::getDefault() {
    std::string id = callSnapper("--csvout list --columns default,number | grep yes | cut -f 2 -d ,");
    Util::rtrim(id);
    return id;
}

bool Snapper::isInProgress() {
    std::string desc = callSnapper("--csvout list --columns number,userdata | grep '^" + snapshotId + ",'");
    if (desc.find("transactional-update-in-progress=yes") == std::string::npos)
        return false;
    else
        return true;
}

bool Snapper::isReadOnly() {
    std::string ro = Util::exec("btrfs property get " + std::string(getRoot()) + " ro");
    Util::rtrim(ro);
    if (ro == "ro=true")
        return true;
    else
        return false;
}

void Snapper::setDefault() {
    Util::exec("btrfs subvolume set-default " + std::string(getRoot()));
}

void Snapper::setReadOnly(bool readonly) {
    std::string boolstr = "true";
    if (readonly == false)
        boolstr = "false";
    Util::exec("btrfs property set " + std::string(getRoot()) + " ro " + boolstr);
}

std::string Snapper::callSnapper(std::string opts) {
    try {
        return Util::exec("snapper " + opts);
    } catch (const ExecutionException &e) {
        return Util::exec("snapper --no-dbus " + opts);
    }
}

} // namespace TransactionalUpdate
