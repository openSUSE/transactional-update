/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#ifndef T_U_SNAPPER_H
#define T_U_SNAPPER_H

#include "Snapshot.h"
#include <filesystem>
#include <string>

namespace TransactionalUpdate {

class Snapper: public Snapshot {
public:
    Snapper() = default;
    ~Snapper() = default;
    void create(std::string base);
    void open(std::string id);
    void close();
    void abort();
    std::filesystem::path getRoot();
    std::string getUid();
    std::string getCurrent();
    std::string getDefault();
    bool isInProgress();
    bool isReadOnly();
    void setDefault();
    void setReadOnly(bool readonly);

private:
    std::string callSnapper(std::string);
};

} // namespace TransactionalUpdate

#endif // T_U_SNAPPER_H
