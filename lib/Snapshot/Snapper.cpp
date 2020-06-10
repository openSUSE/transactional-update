/*
  Snapper backend for snapshot handling

  Copyright (c) 2016 - 2020 SUSE LLC

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Snapper.h"
#include "Exceptions.h"
#include "Log.h"
#include "Util.h"

void Snapper::create(std::string base) {
    snapshotId = callSnapper("create --from " + base + " --read-write --print-number --description 'Snapshot Update of #" + base + "' --userdata 'transactional-update-in-progress=yes'");
    Util::rtrim(snapshotId);
}

void Snapper::open(std::string id) {
    snapshotId = id;
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

bool Snapper::isReadOnly() {
    std::string ro = Util::exec("btrfs property get " + std::string(getRoot()) + " ro");
    Util::rtrim(ro);
    if (ro == "ro=true")
        return true;
    else
        return false;
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
