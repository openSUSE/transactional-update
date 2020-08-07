/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#ifndef TRANSACTIONALUPDATE_H_
#define TRANSACTIONALUPDATE_H_

#include <string>

class Transkit {
public:
    Transkit(int argc, const char *argv[]);
    virtual ~Transkit() = default;

    void getHelp();
    int parseOptions(int argc, const char *argv[]);
private:
    std::string baseSnapshot = "active";
};

#endif /* TRANSACTIONALUPDATE_H_ */
