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

#include "Log.h"
#include "Snapper.h"
#include "Util.h"

Snapper::Snapper() {
    snapshotId = Util::exec("snapper create --read-write --print-number --userdata 'transactional-update-in-progress=yes'");
    Util::rtrim(snapshotId);
}
Snapper::Snapper(std::string id)
    : snapshotId(id) {
}

Snapper::~Snapper() {
}

void Snapper::close() {
    Util::exec("snapper modify -u 'transactional-update-in-progress=' " + snapshotId);
}

void Snapper::abort() {
    Util::exec("snapper delete " + snapshotId);
}

std::filesystem::path Snapper::getRoot() {
    return std::filesystem::path("/.snapshots/" + snapshotId + "/snapshot");
}

std::string Snapper::getUid() {
    return snapshotId;
}

std::string Snapper::getCurrent() {
    std::string id = Util::exec("snapper --csvout list --columns active,number | grep yes | cut -f 2 -d ,");
    Util::rtrim(id);
    return id;
}
