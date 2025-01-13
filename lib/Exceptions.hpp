/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Custom exception classes
 */

#ifndef T_U_EXCEPTIONS_H
#define T_U_EXCEPTIONS_H

#include <exception>
#include <string>

class ExecutionException : public std::exception
{
public:
    ExecutionException(const std::string& reason, const int returncode, const std::string& output)
        : reason{reason}, returncode{returncode}, output{output} {
    }
    const char* what() const noexcept override {
        return reason.c_str();
    }
    const std::string reason;
    const int returncode;
    const std::string output;
};

class VersionException : public std::exception
{
public:
    VersionException(const std::string& reason) : reason{reason} {
    }
    const char* what() const noexcept override {
        return reason.c_str();
    }
private:
    const std::string reason;
};

#endif // T_U_EXCEPTIONS_H
