/*
  transactional-update - apply updates to the system in an atomic way

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

#ifndef SNAPSHOT_H_
#define SNAPSHOT_H_

#include <filesystem>
#include <memory>
#include <string>

class Snapshot {
public:
    Snapshot() = default;
    virtual ~Snapshot() = default;
    virtual void close() = 0;
    virtual void abort() = 0;
    virtual std::string getRoot() = 0;
};

class SnapshotFactory {
public:
    static std::unique_ptr<Snapshot> create();
};

#endif /* SNAPSHOT_H_ */
