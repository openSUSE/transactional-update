/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Wrapper for libmount
 */

#ifndef T_U_MOUNT_H
#define T_U_MOUNT_H

#include <filesystem>
#include <libmount/libmount.h>
#include <string>

namespace TransactionalUpdate {

class Mount
{
public:
    Mount(std::string target, unsigned long flags = 0);
    Mount(Mount&& other) noexcept;
    virtual ~Mount();
    std::string getFS();
    std::string getOption(std::string option);
    std::string getTarget();
    bool isMount();
    virtual void mount(std::string prefix = "/");
    void persist(std::filesystem::path file);
    void removeOption(std::string option);
    void setOption(std::string option, std::string value);
    void setSource(std::string source);
    void setTabSource(std::string source);
    void setType(std::string type);
protected:
    struct libmnt_context* mnt_cxt;
    struct libmnt_table* mnt_table;
    struct libmnt_fs* mnt_fs;
    std::string tabsource;
    std::string target;
    unsigned long flags;
    void find();
    void getTabEntry();
    void getMntFs();
    void umountRecursive(libmnt_table* umount_table, libmnt_fs* umount_fs);
};

class BindMount : public Mount
{
public:
    BindMount(std::string target, unsigned long flags = 0);
    void mount(std::string prefix = "/") override;
};

class PropagatedBindMount : public BindMount
{
public:
    PropagatedBindMount(std::string target, unsigned long flags = 0);
};

} // namespace TransactionalUpdate

#endif // T_U_MOUNT_H
