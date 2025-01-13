/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/* Plugin mechanism for tukit */

#ifndef T_U_PLUGINS_H
#define T_U_PLUGINS_H

#include "Transaction.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace TransactionalUpdate {

class Plugins {
public:
    Plugins(TransactionalUpdate::Transaction* transaction);
    virtual ~Plugins();
    void run(std::string stage, std::string args);
    void run(std::string stage, char* argv[]);
protected:
    TransactionalUpdate::Transaction* transaction;
    std::vector<std::filesystem::path> plugins;
};

} // namespace TransactionalUpdate

#endif // T_U_PLUGINS_H
