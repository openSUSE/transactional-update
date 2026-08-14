/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Directory backend for snapshot handling; see Directory.hpp for a description
  of the on-disk layout.
 */

#include "Directory.hpp"
#include "Configuration.hpp"
#include "Log.hpp"
#include "Util.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace TransactionalUpdate {

/* Helper function */

// Read a snapshot's "info" file into a key=value map.
static std::map<std::string, std::string> readInfo(const fs::path& infofile) {
    std::map<std::string, std::string> info;
    std::ifstream in(infofile);
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        info[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return info;
}

static void writeInfo(const fs::path& infofile, const std::map<std::string, std::string>& info) {
    std::ofstream out(infofile, std::ios::trunc);
    for (const auto& [key, value] : info)
        out << key << "=" << value << "\n";
}

static std::string readIdFile(const fs::path& file) {
    if (! fs::exists(file))
        return "";
    std::ifstream in(file);
    std::string id;
    std::getline(in, id);
    Util::rtrim(id);
    return id;
}

/* Helper methods */

std::string Directory::readMeta(const std::string& id, const std::string& key) {
    auto info = readInfo(snapshotsDir() / id / "info");
    auto it = info.find(key);
    return it == info.end() ? "" : it->second;
}

void Directory::writeMeta(const std::string& id, const std::string& key, const std::string& value) {
    fs::path infofile = snapshotsDir() / id / "info";
    auto info = readInfo(infofile);
    info[key] = value;
    writeInfo(infofile, info);
}

void Directory::writeDefault(const std::string& id) {
    std::ofstream out(snapshotsDir() / "default", std::ios::trunc);
    out << id;
}

std::string Directory::nextId() {
    unsigned long max = 0;
    if (fs::exists(snapshotsDir())) {
        for (const auto& entry : fs::directory_iterator(snapshotsDir())) {
            if (! entry.is_directory())
                continue;
            try {
                max = std::max(max, std::stoul(entry.path().filename().string()));
            } catch (const std::exception&) {
                // Ignore non-numeric entries (e.g. the "default" file)
            }
        }
    }
    return std::to_string(max + 1);
}

/* SnapshotManager methods */

std::unique_ptr<Snapshot> Directory::create(std::string base, std::string description) {
    fs::path baseRoot = snapshotsDir() / base / "snapshot";
    if (! fs::exists(baseRoot))
        throw std::invalid_argument{"Base snapshot '" + base + "' does not exist."};

    snapshotId = nextId();
    fs::create_directories(snapshotsDir() / snapshotId);
    std::time_t result = std::time(nullptr);

    writeMeta(snapshotId, "description", description);
    writeMeta(snapshotId, "date", std::ctime(&result));
    writeMeta(snapshotId, "in-progress", "yes");
    writeMeta(snapshotId, "read-only", "no");

    Util::exec("rsync --archive '" + baseRoot.native() + "/' '" + getRoot().native() + "/'");

    return std::make_unique<Directory>(snapshotId);
}

std::unique_ptr<Snapshot> Directory::open(std::string id) {
    snapshotId = id;
    if (! fs::exists(getRoot()) || ! fs::exists(snapshotsDir() / id / "info"))
        throw std::invalid_argument{"Snapshot " + id + " does not exist."};
    return std::make_unique<Directory>(snapshotId);
}

std::deque<std::map<std::string, std::string>> Directory::getList(std::string columns) {
    if (columns.empty())
        columns = "number,date,description";

    std::deque<std::map<std::string, std::string>> snapshotList;
    if (! fs::exists(snapshotsDir()))
        return snapshotList;

    std::string current;
    std::string def;
    try { current = getCurrent(); } catch (const std::exception&) {}
    try { def = getDefault(); } catch (const std::exception&) {}

    std::vector<std::string> ids;
    for (const auto& entry : fs::directory_iterator(snapshotsDir())) {
        if (! entry.is_directory())
            continue;
        const std::string id = entry.path().filename();
        if (id.empty() || ! std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isdigit(c); }))
            continue;
        // Directories without metadata are remains of deleted snapshots
        if (! fs::exists(entry.path() / "info"))
            continue;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end(), [](const std::string& a, const std::string& b) {
        return std::stoul(a) < std::stoul(b);
    });

    std::vector<std::string> cols;
    std::stringstream colStream(columns);
    for (std::string col; std::getline(colStream, col, ','); )
        cols.push_back(col);

    for (const auto& id : ids) {
        auto info = readInfo(snapshotsDir() / id / "info");
        std::map<std::string, std::string> snapshot;
        for (const auto& col : cols) {
            if (col == "number")
                snapshot[col] = id;
            else if (col == "default")
                snapshot[col] = (id == def) ? "yes" : "no";
            else if (col == "active")
                snapshot[col] = (id == current) ? "yes" : "no";
            else
                snapshot[col] = info.count(col) ? info[col] : "";
        }
        snapshotList.push_back(snapshot);
    }
    return snapshotList;
}

std::string Directory::getCurrent() {
    std::string id = readIdFile(snapshotsDir() / "current");
    if (id.empty())
        throw std::runtime_error{"Couldn't determine current snapshot number. Write the booted "
                                 "snapshot ID to '" + (snapshotsDir() / "current").native() + "'."};
    return id;
}

std::string Directory::getDefault() {
    std::string id = readIdFile(snapshotsDir() / "default");
    if (id.empty())
        throw std::runtime_error{"Couldn't determine default snapshot number."};
    return id;
}

void Directory::deleteSnap(std::string id) {
    // Remove a snapshot. The metadata file is deleted first, so the snapshot is
    // invalidated in any case - removing the file tree itself may fail if mount
    // points are still active below the snapshot directory, which can happen when
    // running without the privileges to unmount them again.
    std::error_code ec;
    fs::remove(snapshotsDir() / id / "info", ec);
    fs::remove_all(snapshotsDir() / id, ec);
    if (ec)
        tulog.info("Could not completely remove snapshot directory '" + (snapshotsDir() / id).native() + "': " + ec.message());
}

std::string Directory::rollback(std::string id) {
    if (! isReadOnly()) {
        create(id, "Rollback of " + id);
        id = snapshotId;
    }
    writeDefault(id);
    return id;
}

/* Snapshot methods */

void Directory::close() {
    writeMeta(snapshotId, "in-progress", "no");
}

void Directory::abort() {
    deleteSnap(snapshotId);
}

fs::path Directory::getRoot() {
    return snapshotsDir() / snapshotId / "snapshot";
}

bool Directory::isInProgress() {
    return readMeta(snapshotId, "in-progress") == "yes";
}

bool Directory::isReadOnly() {
    return readMeta(snapshotId, "read-only") == "yes";
}

void Directory::setDefault() {
    writeDefault(snapshotId);
}

void Directory::setReadOnly(bool readonly) {
    writeMeta(snapshotId, "read-only", readonly ? "yes" : "no");
}

} // namespace TransactionalUpdate
