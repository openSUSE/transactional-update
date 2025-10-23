/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#ifndef TRANSACTIONALUPDATE_H
#define TRANSACTIONALUPDATE_H

#include <optional>
#include <string>

class TUKit {
public:
    TUKit(int argc, char *argv[]);
    virtual ~TUKit() = default;

    void displayHelp();
    int parseOptions(int argc, char *argv[]);
    int processCommand(char *argv[]);
private:
    std::string baseSnapshot = "active";
    bool keepSnapshot = false;
    bool discardSnapshot = false;
    std::string fields;
    std::optional<std::string> description = std::nullopt;
    std::string manager;
};

#endif /* TRANSACTIONALUPDATE_H */
