/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Helper class
 */

#ifndef T_U_UTIL_H
#define T_U_UTIL_H

#include <string>
#include <array>
#include <iostream>

namespace TransactionalUpdate {

struct Util {
    static std::string exec(const std::string cmd);
    static void ltrim(std::string &s);
    static void rtrim(std::string &s);
    static void stub(std::string option);
    static void trim(std::string &s);
    static void sanitize_quotes(std::string &s);
};

struct CString {
    ~CString() { free(ptr); }
    operator char*() { return ptr; }
    char *ptr = nullptr;
};

} // namespace TransactionalUpdate

#endif // T_U_UTIL_H
