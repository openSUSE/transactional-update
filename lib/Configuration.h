/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Retrieves configuration values, set via configuration file or using
  default values otherwise.
 */

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include <string>

typedef struct econf_file econf_file;

class Configuration {
public:
    Configuration();
    virtual ~Configuration();
    std::string get(const std::string &key);
private:
    econf_file *key_file;
};

inline Configuration config{};

#endif /* CONFIGURATION_H_ */
