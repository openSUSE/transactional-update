/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Wrapper for libmount
 */

#ifndef MOUNT_H
#define MOUNT_H

#include <filesystem>
#include <libmount/libmount.h>
#include <string>

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

#endif // MOUNT_H
