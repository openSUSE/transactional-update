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

#ifndef LIBMOUNTWRAPPER_H
#define LIBMOUNTWRAPPER_H

#include <libmount/libmount.h>
#include <string>

class Mount
{
public:
    Mount(std::string target, unsigned long flags = 0);
    Mount(Mount&& other) noexcept;
    virtual ~Mount();
    std::string getFS();
    std::string getTarget();
    bool isMounted();
    virtual void mount(std::string prefix = "/");
    void setSource(std::string source);
    void setType(std::string type);
protected:
    struct libmnt_context* mnt_cxt;
    struct libmnt_table* mnt_table;
    struct libmnt_fs* mnt_fs;
    std::string target;
    unsigned long flags;
    void find();
    void getFstabEntry();
    void getMntFs();
};

class BindMount : public Mount
{
public:
    BindMount(std::string target, unsigned long flags = 0);
    void mount(std::string prefix = "/") override;
};

#endif // LIBMOUNTWRAPPER_H
