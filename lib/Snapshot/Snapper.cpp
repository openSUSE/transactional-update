/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#include "Snapper.hpp"
#include "Exceptions.hpp"
#include "Log.hpp"
#include "Util.hpp"
#include <regex>

namespace TransactionalUpdate {

std::unique_ptr<Snapshot> Snapper::create(std::string base) {
    if (! std::filesystem::exists("/.snapshots/" + base + "/snapshot"))
        throw std::invalid_argument{"Base snapshot '" + base + "' does not exist."};
    snapshotId = callSnapper("create --from " + base + " --read-write --print-number --description 'Snapshot Update of #" + base + "' --userdata 'transactional-update-in-progress=yes'");
    Util::rtrim(snapshotId);
    return std::make_unique<Snapper>(snapshotId);
}

std::unique_ptr<Snapshot> Snapper::open(std::string id) {
    snapshotId = id;
    if (! std::filesystem::exists(getRoot()))
        throw std::invalid_argument{"Snapshot " + id + " does not exist."};
    return std::make_unique<Snapper>(snapshotId);
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

std::string Snapper::getCurrent() {
    std::string id = callSnapper("--csvout list --columns active,number");
    std::smatch match;
    bool found = std::regex_search(id, match, std::regex("yes,([0-9]+)"));
    if (!found)
        throw std::runtime_error{"Couldn't determine current snapshot number"};
    return match[1].str();
}

std::string Snapper::getDefault() {
    std::string id = callSnapper("--csvout list --columns default,number");
    std::smatch match;
    bool found = std::regex_search(id, match, std::regex("yes,([0-9]+)"));
    if (!found)
        throw std::runtime_error{"Couldn't determine default snapshot number"};
    return match[1].str();
}

bool Snapper::isInProgress() {
    std::string desc = callSnapper("--csvout list --columns number,userdata");
    std::smatch match;
    return std::regex_search(desc, match, std::regex("(^|\n)" + snapshotId + ",.*transactional-update-in-progress=yes"));
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
    if (snapperNoDbus == false) {
        try {
            return Util::exec("snapper " + opts);
        } catch (const ExecutionException &e) {
            snapperNoDbus = true;
        }
    }
    if (snapperNoDbus == true) {
        return Util::exec("snapper --no-dbus " + opts);
    }
    return "false ";
}

} // namespace TransactionalUpdate
