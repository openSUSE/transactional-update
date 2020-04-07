/*
  Zypper implementation of the PackageManager class

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

#ifndef ZYPPER_H
#define ZYPPER_H

#include "PackageManager.h"

class Zypper : public PackageManager
{
public:
    Zypper() = default;
    virtual ~Zypper() = default;
    virtual void doDistUpgrade(string chrootDir) override;
    virtual void doUpdate(string chrootDir) override;
};

#endif // ZYPPER_H
