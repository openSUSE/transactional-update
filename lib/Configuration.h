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

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <string>

extern "C" {
#include <libeconf.h>
}

class Configuration {
public:
    Configuration();
    virtual ~Configuration();
    std::string get(const std::string &key);
private:
    econf_file *key_file;
};

inline Configuration config{};

#endif /* CONFIGURATION_H_ */
