/*
  Handling of /etc overlayfs layers

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

#ifndef OVERLAY_H
#define OVERLAY_H

#include "Mount.h"
#include <memory>
#include <string>
#include <vector>

class Overlay {
public:
    Overlay(std::string snapshot);
    virtual ~Overlay() = default;
    void create(std::string base);
    std::string getOldestSnapshot();
    void sync(std::string snapshot);
    void setMountOptions(std::unique_ptr<Mount>& mount);
    void setMountOptionsForMount(std::unique_ptr<Mount>& mount);

    std::vector<std::filesystem::path> lowerdirs;
    std::filesystem::path upperdir;
    std::filesystem::path workdir;
private:
    static std::string getIdOfOverlayDir(const std::string dir);
};

#endif // OVERLAY_H
