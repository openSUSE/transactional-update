/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Snapper backend for snapshot handling
 */

#ifndef FILESYSTEMS_SNAPPER_H_
#define FILESYSTEMS_SNAPPER_H_

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

#endif /* FILESYSTEMS_SNAPPER_H_ */
