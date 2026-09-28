/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Methods to store the transactional-update state
 */

#ifndef T_U_STATESTORE_H
#define T_U_STATESTORE_H

#include <filesystem>
#include <string>
#include <vector>

typedef struct econf_file econf_file;

namespace TransactionalUpdate {

class StateStore {
public:
    StateStore(std::filesystem::path statefile = config.get("STATE_FILE"));
    virtual ~StateStore();
    StateStore(const StateStore&) = delete;
    void operator=(const StateStore&) = delete;
    std::string get(const std::string &key);
    bool contains(const std::string &key, const std::string &pattern);
    void set(const std::string &key, const std::string &value);
    void add(const std::string &key, std::string addition);
    void remove(const std::string &key, const std::string &pattern);
    void persist(std::filesystem::path snapshot="", std::filesystem::path statefile = config.get("STATE_FILE"));
private:
    void store();
    econf_file *key_file;
};

inline StateStore state{};

} // namespace TransactionalUpdate

#endif // T_U_STATESTORE_H
