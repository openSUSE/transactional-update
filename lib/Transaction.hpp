/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  A Transaction is unique instance, shared between all classes derived from
  the "TransactionalCommand" base class; that way it is made sure that all
  commands operate on the same snapshot. In case the destructor should be
  called before the transaction instance is closed, an eventual snapshot will
  be deleted again.
 */

#ifndef T_U_TRANSACTION_H
#define T_U_TRANSACTION_H

#include <filesystem>
#include <optional>

namespace TransactionalUpdate {

class Transaction {
public:
    /**
     * @brief Constructor for a new Transaction object
     *
     * The Transaction constructor will determine which snapshotting mechanism to use, but will
     * not change the system itself. It is required to either init() or resume() a session.
     * @param manager (optional) explicitly selects one of the implementations
     */
    Transaction(std::string manager = "auto");

    /**
     * @brief Destructor
     *
     * If the destructor is triggered and the snapshot hasn't been persisted with finalize()
     * or keep(), then the snapshot will be purged again.
     */
    virtual ~Transaction();

    /**
     * @brief Open a new transaction
     * @param base Snapshot ID, "active" or "default"
     * @param description (optional) allows to customize the description of the snapshot
     *
     * Create a new snapshot, based on the given @base.
     * @base can be "active" to base the snapshot on the currently running system, "default" to
     * use the current default snapshot as a base (which may or may not be identical to "active")
     * or any specific existing snapshot id.
     *
     * If @base is not set "active" will be used as the default.
     */
    void init(std::string base, std::optional<std::string> description = std::nullopt);

    /**
     * @brief Set flag to keep snapshots if plugins or command return error code
     * @param keep true or false
     *
     */
    void setKeepIfError(bool keep);

    /**
     * @brief Set flag to discard snapshots if no changes are detected
     * @param discard true or false
     *
     * If discard is true, then an inotify watcher will be registered to listen for changes
     * in the root file system during execute() and callExt() calls. In case no change is
     * detected the snapshot will be discarded when calling finalize().
     * If the snapshot will be discarded and if /etc is an overlay file system, then potentially
     * changed files in /etc will be synchronized into the running system.
     *
     * This method has to be called before init(), otherwise setting the mode has no effect. The
     * mode is stored until the snapshot is finalized, i.e. resuming a snapshot will remember
     * whether the snapshot may be a candidate for discarding.
     *
     * Be aware that inotify registration may fail, e.g. if a system has a lot of open inotify
     * listeners already. Changes may not be detected correctly in this case.
     */
    void setDiscardIfUnchanged(bool discard);

    /**
     * @brief Resume an existing transaction
     * @param id Snapshot ID
     *
     * Resume a transaction closed with keep().
     */
    void resume(std::string id);

    /**
     * @brief Execute the given application in the new snapshot
     * @param argv
     * @param (optional) output Variable to store the command's output to
     * @return application's return code
     *
     * Execute any given command within the new snapshot. The application's output will be
     * printed to the corresponding streams or, if set, stored in the output variable.
     *
     * Note that @param is following the default C style syntax:
     * @example: char *args[] = {(char*)"ls", (char*)"-l", NULL};
                 int status = transaction.execute(args);
     */
    int execute(char* argv[], std::string *output=nullptr);

    /**
     * @brief Replace '{}' in argv with mount directory and execute command
     * @param argv
     * @param (optional) output Variable to store the command's output to
     * @return application's return code
     *
     * Replace any standalone occurrence of '{}' in argv with the snapshot's mount directory
     * and execute the given command *outside* of the snapshot in the running system. This may
     * be useful if the command needs access to the current environment, e.g. to copy a file
     * from a directory not accessible from within the chroot environment.
     * The application's output will be printed to the corresponding streams or, if set,
     * stored in the output variable.
     *
     * Note that @param is following the default C style syntax:
     * @example: char *args[] = {(char*)"zypper", (char*)"-R", (char*)"{}", (char*)"up", NULL};
                 int status = transaction.callExt(args);
     */
    int callExt(char* argv[], std::string *output=nullptr);

    /**
     * @brief Close a transaction and set it as the new default snapshot
     *
     * Note that it is necessary to call this method if the snapshot is supposed to be kept.
     * Failing to do so will remove the snapshot as soon as the Transaction's destructor is
     * called.
     */
    void finalize();

    /**
     * @brief Don't discard transaction on destructor call
     *
     * It is possible to keep a transaction open even though the Transaction object has been
     * destructed. Such a transaction can be resumed later using resume(), but it won't be set
     * as the new default snapshot as long as it is still open.
     *
     * Note that such pending transactions will still be marked for cleanup by
     * transactional-update to avoid collecting unfinished / never closed snapshots
     */
    void keep();

    /**
     * @brief Sends a signal to the executed process
     * @param int Signal number
     *
     * If a transaction is currently running, then the given signal will be sent to the
     * transaction's process (i.e. the command called by execute()) if started already.
     */
    void sendSignal(int signal);

    /**
     * @brief Check whether a snapshot has been initialized already
     * @return
     *
     * True if init() or resume() have been called successfully.
     */
    bool isInitialized();

    /**
     * @brief Return the snapshot ID
     * @return snapshot ID
     *
     * May be used for resume() or general information.
     */
    std::string getSnapshot();

    /**
     * @brief Return the root path of the snapshot
     * @return root path
     *
     * To actually operate on the path the Transaction should still be open: On a read-only
     * system the new snapshot's path will be set read-only as soon as finalize() is called.
     * Also auxiliary mounts such as /etc will only be bind mounted into the new snapshot's
     * root path as long as Transaction's destructor hasn't been called.
     */
    std::filesystem::path getRoot();

    friend class Plugins;
protected:
    std::filesystem::path getBindDir();
private:
    class impl;
    std::unique_ptr<impl> pImpl;
};

} // namespace TransactionalUpdate

#endif // T_U_TRANSACTION_H
