/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  Retrieves configuration values, set via configuration file or using
  default values otherwise.
 */

#include "Configuration.hpp"
#include "Util.hpp"
#include <map>
#include <regex>
#include <stdexcept>
#include <libeconf.h>

namespace TransactionalUpdate {

Configuration::Configuration() {
    econf_file *kf_defaults;
    econf_err error = econf_newIniFile(&kf_defaults);
    if (error)
        throw std::runtime_error{"Could not create default configuration."};
    std::map<const char*, const char*> defaults = {
        {"DRACUT_SYSROOT", "/sysroot"},
        {"LOCKFILE", "/var/run/tukit.lock"},
        {"REBOOT_ALLOW_SOFT_REBOOT", "true"},
        {"REBOOT_ALLOW_KEXEC", "false"},
        {"REBOOT_NEEDED_FILE", "/run/reboot-needed"},
        {"OCI_TARGET", ""},
        {"SNAPSHOT_MANAGER", "snapper"}
    };
    for(auto &[key, value] : defaults) {
        error = econf_setStringValue(kf_defaults, "", key, value);
        if (error) {
            econf_freeFile(kf_defaults);
            throw std::runtime_error{"Could not set default value for '" + std::string(key) + "'."};
        }
    }

    econf_file *kf_conffiles;
    error = econf_readDirs(&kf_conffiles, (std::string(PREFIX) + CONFDIR).c_str(), CONFDIR, "tukit", ".conf", "=", "#");
    if (error && error != ECONF_NOFILE) {
        econf_freeFile(kf_defaults);
        econf_freeFile(kf_conffiles);
        throw std::runtime_error{"Couldn't read configuration file: " + std::string(econf_errString(error))};
    }

    if (error == ECONF_SUCCESS) {
        error = econf_mergeFiles(&key_file, kf_defaults, kf_conffiles);
        econf_freeFile(kf_defaults);
        econf_freeFile(kf_conffiles);
        if (error) {
            throw std::runtime_error{"Couldn't merge configuration: " + std::string(econf_errString(error))};
        }
    } else {
        econf_freeFile(key_file);
        key_file = std::move(kf_defaults);
    }
}

Configuration::~Configuration() {
    econf_freeFile(key_file);
}

std::string Configuration::get(const std::string &key) {
    CString val;
    econf_err error = econf_getStringValue(key_file, "", key.c_str(), &val.ptr);
    if (error)
        throw std::runtime_error{"Could not read configuration setting '" + key + "'."};
    return std::string(val);
}

std::vector<std::string> Configuration::getArray(const std::string &key) {
    std::vector<std::string> ret;
    econf_err error;

    size_t len = 0;
    char** confkeys;
    error = econf_getKeys(key_file, "", &len, &confkeys);
    if (error)
        throw std::runtime_error{"Could not read keys."};

    std::regex exp(key + "\\[.*\\]");
    for (size_t i = 0; i < len; i++) {
        if (std::regex_match(confkeys[i], exp)) {
            CString val;
            error = econf_getStringValue(key_file, "", confkeys[i], &val.ptr);
            if (error) {
                econf_freeArray(confkeys);
                throw std::runtime_error{"Could not read key '" + std::string(confkeys[i]) + "'."};
            }
            if (val != nullptr)
                ret.push_back(std::string(val));
        }
    }
    econf_freeArray(confkeys);

    return ret;
}

} // namespace TransactionalUpdate
