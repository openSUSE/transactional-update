/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Retrieves configuration values, set via configuration file or using
  default values otherwise.
 */

#ifndef T_U_CONFIGURATION_H
#define T_U_CONFIGURATION_H

#include <string>
#include <vector>

typedef struct econf_file econf_file;

namespace TransactionalUpdate {

class Configuration {
public:
    Configuration();
    virtual ~Configuration();
    Configuration(const Configuration&) = delete;
    void operator=(const Configuration&) = delete;
    std::string get(const std::string &key);
    std::vector<std::string> getArray(const std::string &key);
private:
    econf_file *key_file;
};

inline Configuration config{};

} // namespace TransactionalUpdate

#endif // T_U_CONFIGURATION_H
