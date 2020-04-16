/*
  Provide logging facitlies by including this header file

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

#ifndef LOG_H
#define LOG_H

#include <iomanip>
#include <iostream>

enum class TULogLevel {
    NONE=0, ERROR, INFO, DEBUG
};

// There's no threading in this application, so no locking is implemented
class TULog {
public:
    TULogLevel level = TULogLevel::ERROR;

    template<typename... T> void error(const T&... args) {
        if (int(level) >= int(TULogLevel::ERROR))
            log(args...);
    }
    template<typename... T> void info(const T&... args) {
        if (int(level) >= int(TULogLevel::INFO))
            log(args...);
    }
    template<typename... T> void debug(const T&... args) {
        if (int(level) >= int(TULogLevel::DEBUG))
            log(args...);
    }

    template<typename... T> void log(const T&... args) {
        time_t now = time(0);
        std::cout << std::put_time(std::localtime(&now), "%F %T ");
        ((std::cout << args),...) << std::endl;
    }
};

inline TULog tulog{};

#endif /* LOG_H */
