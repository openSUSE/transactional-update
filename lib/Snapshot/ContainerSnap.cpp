/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

#include "ContainerSnap.hpp"
#include "Util.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace TransactionalUpdate {

static std::string callContainerSnap(std::string opts) {
  std::string output;

  output = Util::exec("container-snap " + opts);
  Util::trim(output);
  return output;
}

std::string ContainerSnap::getCurrent() {
  return callContainerSnap("get-current");
}

std::string ContainerSnap::getDefault() {
  return callContainerSnap("get-default");
}

void ContainerSnap::deleteSnap(std::string id) {
  callContainerSnap("delete --id=" + id);
}

void ContainerSnap::rollbackTo(std::string id) {
  callContainerSnap("switch " + id);
}

std::deque<std::map<std::string, std::string>>
ContainerSnap::getList(std::string /*columns*/) {
  std::deque<std::map<std::string, std::string>> snapshotList;

  const auto output = callContainerSnap("list-snapshots");
  // output looks like this:
  // # snasphot-id,ro
  // 43de4dcfccec5cd0b92c04afe1bbde645ff24bff5ff8845b73e82ae8bfd58e74,false
  std::stringstream outputstream(output);
  std::string line;

  // consume header line
  std::getline(outputstream, line);

  while (std::getline(outputstream, line)) {
    // Skip empty lines if any
    if (line.empty()) {
      continue;
    }

    std::stringstream linestream(line);
    std::string id_value, ro_value;

    // Extract snapshot-id
    if (!std::getline(linestream, id_value, ',')) {
      // line doesn't contain a comma, or is malformed => skip
      continue;
    }

    // Extract ro state (rest of the line after the first comma)
    std::getline(linestream, ro_value);

    std::map<std::string, std::string> entry;
    entry["id"] = id_value;
    entry["ro"] = ro_value;

    snapshotList.push_back(entry);
  }

  return snapshotList;
}

std::unique_ptr<Snapshot> ContainerSnap::open(std::string id) {
  return std::make_unique<ContainerSnapshot>(ContainerSnapshot(id));
};

std::unique_ptr<Snapshot> ContainerSnap::create(std::string base,
                                                std::string /*description*/) {
  std::string new_snapshot_id = callContainerSnap("create-snapshot " + base);
  return std::make_unique<ContainerSnapshot>(
      ContainerSnapshot(new_snapshot_id));
}

std::filesystem::path ContainerSnapshot::getRoot() {
  return std::filesystem::path(
      callContainerSnap("get-root " + this->snapshotId));
}

void ContainerSnapshot::abort() {
  ContainerSnap().deleteSnap(this->snapshotId);
}

void ContainerSnapshot::close() { this->inProgress = false; }

bool ContainerSnapshot::isInProgress() { return this->inProgress; }

bool ContainerSnapshot::isReadOnly() {
  return callContainerSnap("get-readonly-state " + this->snapshotId) == "true";
}

void ContainerSnapshot::setDefault() {
  callContainerSnap("switch " + this->snapshotId);
}

void ContainerSnapshot::setReadOnly(bool readonly) {
  callContainerSnap("set-readonly-state " +
                    std::string(readonly ? "--readonly" : "--no-readonly") +
                    " " + this->snapshotId);
}

} // namespace TransactionalUpdate
