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

/* SnapshotManager methods */

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
        // This is a simple CSV parser
        for (std::string field; std::getline(fieldsStream, field, ','); ) {
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
        snapshotList.push_back(snapshot);
    }
    return snapshotList;
}

/* Snapshot methods */

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

void Snapper::deleteSnap(std::string id) {
    callSnapper("delete " + id);
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

/* Helper methods */

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
