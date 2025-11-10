/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Provide logging facitlies by including this header file
 */

#ifndef T_U_LOG_H
#define T_U_LOG_H

#include <exception>
#include <iomanip>
#include <iostream>
#include <syslog.h>

enum class TULogLevel {
    None=0, Error, Info, Debug
};
struct TULogOutput {
    bool console = true;
    bool syslog = true;
};

// There's no threading in this application, so no locking is implemented
class TULog {
public:
    TULogLevel level = TULogLevel::Error;
    TULogOutput output{};

    template<typename... T> void error(const T&... args) {
        if (level >=TULogLevel::Error) {
            print_to_output(LOG_ERR, args...);
        }
        print_to_syslog(LOG_ERR, args...);
    }
    template<typename... T> void info(const T&... args) {
        if (level >= TULogLevel::Info) {
            print_to_output(LOG_INFO, args...);
        }
        print_to_syslog(LOG_INFO, args...);
    }
    template<typename... T> void debug(const T&... args) {
        if (level >= TULogLevel::Debug) {
            print_to_output(LOG_DEBUG, args...);
            print_to_syslog(LOG_DEBUG, args...);
        }
    }

    template<typename... T> void log(const T&... args) {
        print_to_output(LOG_INFO, args...);
        print_to_syslog(LOG_INFO, args...);
    }

    void setLogOutput(std::string outputs) {
        output.console = false;
        output.syslog = false;
        std::string field;
        std::stringstream ss(outputs);
        while (getline(ss, field, ',')) {
            if (field == "console") {
                output.console = true;
                continue;
            }
            if (field == "syslog") {
                output.syslog = true;
                continue;
            }
            throw std::invalid_argument{"Invalid log output."};
        }
    }

private:
    template<typename... T> void print_to_output(int loglevel, const T&... args) {
        if (output.console) {
            std::stringstream ss;
            ((ss << args),...);
            std::string s = ss.str();
            if (loglevel <= LOG_ERR) {
                std::cerr << s << std::endl;
            } else {
                std::cout << s << std::endl;
            }
        }
    }
    template<typename... T> void print_to_syslog(int loglevel, const T&... args) {
        if (output.syslog) {
            std::stringstream ss;
            ((ss << args),...);
            std::string s = ss.str();
            syslog(loglevel, "%s", s.c_str());
        }
    }
};

inline TULog tulog{};

#endif // T_U_LOG_H
