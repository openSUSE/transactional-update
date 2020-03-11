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

#ifndef UTIL_H_
#define UTIL_H_

#include <string>
#include <array>
#include <iostream>
using namespace std;

struct Util {
    static string exec(const string cmd);
    static void ltrim(string &s);
    static void rtrim(string &s);
    static void stub(std::string option);
    static void trim(string &s);
};

struct CString {
    ~CString() { free(ptr); }
    operator char*() { return ptr; }
    char *ptr = nullptr;
};

#endif /* UTIL_H_ */
