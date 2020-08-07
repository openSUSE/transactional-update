/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Custom exception classes
 */

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <exception>
#include <string>

class ExecutionException : public std::exception
{
public:
    ExecutionException(std::string why, int status) : reason{why}, returncode{status} {

    }
    const char* what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW override {
        return reason.c_str();
    }
    int getReturnCode() {
        return returncode;
    }
private:
    std::string reason;
    int returncode;
};

#endif // EXCEPTIONS_H
