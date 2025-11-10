/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#include "Snapper.hpp"
#include "Exceptions.hpp"
#include "Util.hpp"
#include <regex>

namespace TransactionalUpdate {

/* SnapshotManager methods */

std::unique_ptr<Snapshot> Snapper::create(std::string base, std::string description) {
    if (! std::filesystem::exists("/.snapshots/" + base + "/snapshot"))
        throw std::invalid_argument{"Base snapshot '" + base + "' does not exist."};
    snapshotId = callSnapper("create --from " + base + " --read-write --cleanup-algorithm number --print-number --description '" + description + "' --userdata 'transactional-update-in-progress=yes'");
    Util::rtrim(snapshotId);
    return std::make_unique<Snapper>(snapshotId);
}

std::unique_ptr<Snapshot> Snapper::open(std::string id) {
    snapshotId = id;
    if (! std::filesystem::exists(getRoot()))
        throw std::invalid_argument{"Snapshot " + id + " does not exist."};
    return std::make_unique<Snapper>(snapshotId);
}

std::deque<std::map<std::string, std::string>> Snapper::getList(std::string columns) {
    std::deque<std::map<std::string, std::string>> snapshotList;

    // Sanitize user input
    if (! std::all_of(columns.begin(), columns.end(), [](char c) {
           return (std::isalpha(c) || c == ',');
        })) {
        throw std::invalid_argument{"Column list contains invalid characters."};
    }

    if (columns.empty())
        columns="number,date,description";
    std::string snapshots = callSnapper("--utc --iso --csvout list --columns " + columns);
    std::stringstream snapshotsStream(snapshots);

    // Headers
    std::vector<std::string> headers;
    std::string line;
    std::getline(snapshotsStream, line);
    std::stringstream fieldsStream(line);
    for (std::string field; std::getline(fieldsStream, field, ','); ) {
        headers.push_back(field);
    }

    // Lines
    for (std::string line; std::getline(snapshotsStream, line); ) {
        std::map<std::string, std::string> snapshot;
        auto header = headers.begin();
        std::stringstream fieldsStream(line, std::stringstream::out | std::stringstream::in | std::stringstream::app);
        std::string field;
        // This is a simple CSV parser
        while (std::getline(fieldsStream, field, ',')) {
            if (field[0] == '"') {
                while (field[field.length() - 1] != '"') {
                    std::string continuation;
                    if (fieldsStream.eof()) { // newline character
                        fieldsStream.clear();
                        std::getline(snapshotsStream, line);
                        fieldsStream << line;
                        std::getline(fieldsStream, continuation, ',');
                        field += "\n" + continuation;
                    } else {
                        std::getline(fieldsStream, continuation, ',');
                        field += "," + continuation;
                    }
                }
                field = field.substr(1, field.length() - 2);
                field = std::regex_replace(field, std::regex("\"\""), "\"");
            }
            snapshot.emplace(*header, field);
            header++;
        }
        if (line.back() == ',') {
            snapshot.emplace(*header, "");
        }
        snapshotList.push_back(snapshot);
    }
    return snapshotList;
}

/* Snapshot methods */

void Snapper::close() {
    callSnapper("modify --userdata 'transactional-update-in-progress=' " + snapshotId);
}

void Snapper::abort() {
    callSnapper("delete " + snapshotId);
}

std::filesystem::path Snapper::getRoot() {
    return std::filesystem::path("/.snapshots/" + snapshotId + "/snapshot");
}

std::string Snapper::getCurrent() {
    std::smatch match;

    // snapper doesn't support the `apply` command for now, so use findmnt directly.
    std::string id = Util::exec("findmnt --target /usr --raw --noheadings --output FSROOT --first-only --direction backward --types btrfs");
    bool found = std::regex_search(id, match, std::regex(".*.snapshots/(.*)/snapshot.*"));
    if (!found) {
        id = Util::exec("findmnt --target / --raw --noheadings --output FSROOT --first-only --direction backward --types btrfs");
        found = std::regex_search(id, match, std::regex(".*.snapshots/(.*)/snapshot.*"));
        if (!found)
            throw std::runtime_error{"Couldn't determine current snapshot number"};
    }
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

void Snapper::deleteSnap(std::string id) {
    callSnapper("delete " + id);
}

void Snapper::rollbackTo(std::string id) {
    callSnapper("rollback " + id);
}

bool Snapper::isInProgress() {
    std::string desc = callSnapper("--csvout list --columns number,userdata");
    std::smatch match;
    return std::regex_search(desc, match, std::regex("(^|\n)" + snapshotId + ",.*transactional-update-in-progress=yes"));
}

bool Snapper::isReadOnly() {
    std::string ro = callSnapper("--csvout list --columns number,read-only");
    std::smatch match;
    bool found = std::regex_search(ro, match, std::regex(snapshotId + ",(.*)"));
    if (!found)
        throw std::runtime_error{"Couldn't determine read-only state"};
    if (match[1].str() == "yes")
        return true;
    else
        return false;
}

void Snapper::setDefault() {
    try {
        callSnapper("modify --default " + snapshotId + " 2>&1");
    } catch (const VersionException &e) {
        Util::exec("btrfs subvolume set-default " + std::string(getRoot()));
    }
}

void Snapper::setReadOnly(bool readonly) {
    try {
        if (readonly == true)
            callSnapper("modify --read-only " + snapshotId + " 2>&1");
        else
            callSnapper("modify --read-write " + snapshotId + " 2>&1");
    } catch (const VersionException &e) {
        Util::exec("btrfs property set " + std::string(getRoot()) + " ro " + (readonly ? "true" : "false"));
    }
}

/* Helper methods */

std::string Snapper::callSnapper(std::string opts) {
    std::string output;
    try {
        if (std::filesystem::exists("/run/dbus/system_bus_socket")) {
            output = Util::exec("snapper " + opts);
        } else {
            output = Util::exec("snapper --no-dbus " + opts);
        }
    } catch (const ExecutionException &e) {
        if (e.output.rfind("Unknown option", 0) == 0) {
            std::string message;
            std::getline(std::istringstream(e.output), message);
            throw VersionException{"snapper: " + message};
        } else {
            throw;
        }
    }
    return output;
}

} // namespace TransactionalUpdate
