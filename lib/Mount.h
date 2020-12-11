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
    Mount(std::string mountpoint, unsigned long flags = 0);
    Mount(Mount&& other) noexcept;
    virtual ~Mount();
    std::string getFilesystem();
    std::string getOption(std::string option);
    bool isMount();
    virtual void mount(std::string prefix = "/");
    void persist(std::filesystem::path file);
    void removeOption(std::string option);
    void setOption(std::string option, std::string value);
    void setSource(std::string source);
    void setTabSource(std::string source);
    void setType(std::string type);
protected:
    struct libmnt_context* mnt_cxt = nullptr;
    struct libmnt_table* mnt_table = nullptr;
    struct libmnt_fs* mnt_fs = nullptr;
    std::string tabsource;
    std::string mountpoint;
    unsigned long flags;
    std::string directoryCreated;
    struct libmnt_fs* findFS();
    struct libmnt_fs* getTabEntry();
    struct libmnt_fs* newFS();
    void umountRecursive(libmnt_table* umount_table, libmnt_fs* umount_fs);
};

class BindMount : public Mount
{
public:
    BindMount(std::string mountpoint, unsigned long flags = 0);
    void mount(std::string prefix = "/") override;
};

class PropagatedBindMount : public BindMount
{
public:
    PropagatedBindMount(std::string mountpoint, unsigned long flags = 0);
};

} // namespace TransactionalUpdate

#endif // T_U_MOUNT_H
