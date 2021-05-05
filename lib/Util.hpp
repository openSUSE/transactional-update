/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Helper class
 */

#ifndef T_U_UTIL_H
#define T_U_UTIL_H

#include <string>
#include <array>
#include <iostream>
#include <filesystem>

namespace TransactionalUpdate {

struct Util {
    static std::string exec(const std::string cmd);
    static void ltrim(std::string &s);
    static void rtrim(std::string &s);
    static void stub(std::string option);
    static void trim(std::string &s);
    static void se_copycontext(const std::filesystem::path ref, const std::filesystem::path dst);
    static bool se_is_context_type(const std::filesystem::path ref, const std::string type);
};

struct CString {
    ~CString() { free(ptr); }
    operator char*() { return ptr; }
    char *ptr = nullptr;
};

} // namespace TransactionalUpdate

#endif // T_U_UTIL_H
