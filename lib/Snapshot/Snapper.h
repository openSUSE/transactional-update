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

#ifndef FILESYSTEMS_SNAPPER_H_
#define FILESYSTEMS_SNAPPER_H_

#include "Snapshot.h"
#include <filesystem>
#include <string>

class Snapper: public Snapshot {
public:
    Snapper() = default;
    ~Snapper() = default;
    void create(std::string base);
    void open(std::string id);
    void close();
    void abort();
    std::filesystem::path getRoot();
    std::string getUid();
    std::string getCurrent();
    std::string getDefault();

private:
    std::string callSnapper(std::string);
};

#endif /* FILESYSTEMS_SNAPPER_H_ */
