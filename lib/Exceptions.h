/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

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
    ExecutionException(const std::string& reason, const int returncode) : reason{reason}, returncode{returncode} {
    }
    const char* what() const noexcept override {
        return reason.c_str();
    }
    int getReturnCode() {
        return returncode;
    }
private:
    const std::string reason;
    const int returncode;
};

#endif // T_U_EXCEPTIONS_H
