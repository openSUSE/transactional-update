/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

#include "Configuration.hpp"
#include "Mount.hpp"
#include "StateStore.hpp"
#include "Util.hpp"
#include <map>
#include <regex>
#include <stdexcept>
#include <libeconf.h>

namespace TransactionalUpdate {

StateStore::StateStore() {
    econf_err error = econf_readFile(&key_file, config.get("STATE_FILE").c_str(), "=", "#");
    if (error != ECONF_SUCCESS && error != ECONF_NOFILE) {
        throw std::runtime_error{"Couldn't read configuration file: " + std::string(econf_errString(error))};
    }
}

StateStore::~StateStore() {
    econf_freeFile(key_file);
}

std::string StateStore::get(const std::string &key) {
    CString val;
    econf_err error = econf_getStringValueDef(key_file, "", key.c_str(), &val.ptr, "");
    if (error != ECONF_SUCCESS && error != ECONF_NOKEY)
        throw std::runtime_error{"Could not read configuration setting '" + key + "': " + std::string(econf_errString(error))};
    return std::string(val);
}

bool StateStore::contains(const std::string &key, const std::string &pattern) {
    std::string value = get(key);
    std::regex word_regex("(\\b)" + pattern + "(\\b)");
    return std::regex_search(value, word_regex);
}

void StateStore::set(const std::string &key, const std::string &value) {
    econf_setStringValue(key_file, "", key.c_str(), value.c_str());
}

void StateStore::add(const std::string &key, std::string addition) {
    if (contains(key, addition))
        return;
    std::string value = get(key);
    if (value != "")
        value += " ";
    econf_setStringValue(key_file, "", key.c_str(), (value + addition).c_str());
}

void StateStore::remove(const std::string &key, const std::string &pattern) {
    std::string value = get(key);
    std::regex word_regex("(\\b)(\\s)*" + pattern + "(\\b)");
    set(key, std::regex_replace(value, word_regex, ""));
}

void StateStore::persist(std::filesystem::path snapPath) {
    std::filesystem::path statefile = config.get("STATE_FILE");
    econf_err error = econf_writeFile(key_file, statefile.parent_path().c_str(), statefile.filename().c_str());
    if (error != ECONF_SUCCESS)
        throw std::runtime_error{"Could not write state file: " + std::string(econf_errString(error))};

    // Backwards compatibility for really old pre-2018 rw filesystem layouts where /var
    // was not a general subvolume yet; it seems read-write distributions didn't have
    // /var/lib/misc as a dedicated subvolume there.
    Mount mntVar{"/var"};
    Mount mntVarLibMisc{"/var/lib/misc"};
    if (! mntVar.isMount() && ! mntVarLibMisc.isMount()) {
        std::filesystem::copy(statefile, snapPath / statefile.relative_path());
    }
}

} // namespace TransactionalUpdate
