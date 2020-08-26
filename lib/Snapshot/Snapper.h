/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#ifndef T_U_SNAPPER_H
#define T_U_SNAPPER_H

#include "Snapshot.h"
#include <filesystem>
#include <string>

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
    void setReadOnly(bool readonly);

private:
    std::string callSnapper(std::string);
};

#endif // T_U_SNAPPER_H
