/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#ifndef TRANSACTIONALUPDATE_H
#define TRANSACTIONALUPDATE_H

#include <string>

class Transkit {
public:
    Transkit(int argc, char *argv[]);
    virtual ~Transkit() = default;

    void displayHelp();
    int parseOptions(int argc, char *argv[]);
    int processCommand(char *argv[]);
private:
    std::string baseSnapshot = "active";
};

#endif /* TRANSACTIONALUPDATE_H */
