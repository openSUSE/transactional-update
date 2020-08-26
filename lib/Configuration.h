/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Retrieves configuration values, set via configuration file or using
  default values otherwise.
 */

#ifndef T_U_CONFIGURATION_H
#define T_U_CONFIGURATION_H

#include <string>

typedef struct econf_file econf_file;

class Configuration {
public:
    Configuration();
    virtual ~Configuration();
    Configuration(const Configuration&) = delete;
    void operator=(const Configuration&) = delete;
    std::string get(const std::string &key);
private:
    econf_file *key_file;
};

inline Configuration config{};

#endif // T_U_CONFIGURATION_H
