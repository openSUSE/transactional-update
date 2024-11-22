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
ContainerSnap::getList(std::string columns) {
  std::deque<std::map<std::string, std::string>> snapshotList;

  const auto output = callContainerSnap("list-snapshots");
  std::stringstream outputstream(output);
  std::string line;

  while (std::getline(outputstream, line)) {
    std::stringstream linestream(line);

    std::string id, ro;
    std::getline(linestream, id, ',');

    if (id[0] == '#') {
      continue;
    }

    std::getline(linestream, ro);

    std::map<std::string, std::string> entry = {{"id", id}, {"ro", ro}};
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
