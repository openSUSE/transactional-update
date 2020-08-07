/*
  Retrieves configuration values, set via configuration file or using
  default values otherwise.

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

#include "Configuration.h"
#include "Util.h"
#include <map>
#include <stdexcept>
extern "C" {
#include <libeconf.h>
}

Configuration::Configuration() {
    econf_err error = econf_newIniFile(&key_file);
    if (error)
        throw std::runtime_error{"Could not create default configuration."};
    std::map<const char*, const char*> defaults = {
        {"DRACUT_SYSROOT", "/sysroot"},
        {"LOCKFILE", "/var/run/transactional-update.pid"},
        {"OVERLAY_DIR", "/var/lib/overlay"}
    };
    for(auto &e : defaults) {
        error = econf_setStringValue(key_file, "", e.first, e.second);
        if (error)
            throw std::runtime_error{"Could not set default value for '" + std::string(e.first) + "'."};
    }
}

Configuration::~Configuration() {
    econf_freeFile(key_file);
}

std::string Configuration::get(const std::string &key) {
    CString val;
    econf_err error = econf_getStringValue(key_file, "", key.c_str(), &val.ptr);
    if (error)
        throw std::runtime_error{"Could not read configuration setting '" + key + "'."};
    return std::string(val);
}
