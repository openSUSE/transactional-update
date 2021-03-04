/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Provide logging facitlies by including this header file
 */

#ifndef T_U_LOG_H
#define T_U_LOG_H

#include <iomanip>
#include <iostream>

enum class TULogLevel {
    None=0, Error, Info, Debug
};

// There's no threading in this application, so no locking is implemented
class TULog {
public:
    TULogLevel level = TULogLevel::Error;

    template<typename... T> void error(const T&... args) {
        if (level >=TULogLevel::Error)
            log(args...);
    }
    template<typename... T> void info(const T&... args) {
        if (level >= TULogLevel::Info)
            log(args...);
    }
    template<typename... T> void debug(const T&... args) {
        if (level >= TULogLevel::Debug)
            log(args...);
    }

    template<typename... T> void log(const T&... args) {
        std::time_t now = std::time(0);
        std::cout << std::put_time(std::localtime(&now), "%F %T ");
        ((std::cout << args),...) << std::endl;
    }
};

inline TULog tulog{};

#endif // T_U_LOG_H
