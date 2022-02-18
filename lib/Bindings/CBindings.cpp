/*
 SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: SUSE LLC */

#include "libtukit.h"
#include "Log.hpp"
#include "Transaction.hpp"
#include <exception>
#include <thread>
#include <string.h>

using namespace TransactionalUpdate;
thread_local std::string errmsg;

const char* tukit_get_errmsg() {
    return errmsg.c_str();
}
void tukit_set_loglevel(loglevel lv) {
    tulog.level = static_cast<TULogLevel>(lv);
}
tukit_tx tukit_new_tx() {
    Transaction* transaction = nullptr;
    try {
        transaction = new Transaction;
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
    }
    return reinterpret_cast<tukit_tx>(transaction);
}
void tukit_free_tx(tukit_tx tx) {
    if (tx != nullptr) {
        delete reinterpret_cast<Transaction*>(tx);
    }
}
int tukit_tx_init(tukit_tx tx, char* base) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        if (std::string(base).empty())
            transaction->init("active");
        else
            transaction->init(base);
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
    return 0;
}
int tukit_tx_discard_if_unchanged(tukit_tx tx, int discard) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        transaction->setDiscardIfUnchanged(discard);
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
    return 0;
}
int tukit_tx_resume(tukit_tx tx, char* id) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        transaction->resume(id);
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
    return 0;
}
int tukit_tx_execute(tukit_tx tx, char* argv[], const char* output[]) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    std::string buffer;
    try {
        int ret = transaction->execute(argv, &buffer);
        if (output) {
            *output = strdup(buffer.c_str());
        }
        return ret;
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
}
int tukit_tx_call_ext(tukit_tx tx, char* argv[], const char* output[]) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    std::string buffer;
    try {
        int ret = transaction->callExt(argv, &buffer);
        if (output) {
            *output = strdup(buffer.c_str());
        }
        return ret;
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
}
int tukit_tx_finalize(tukit_tx tx) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        transaction->finalize();
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
    return 0;
}
int tukit_tx_keep(tukit_tx tx) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        transaction->keep();
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
    return 0;
}
int tukit_tx_send_signal(tukit_tx tx, int signal) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        transaction->sendSignal(signal);
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return -1;
    }
    return 0;
}
int tukit_tx_is_initialized(tukit_tx tx) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    return transaction->isInitialized();
}
/* Free return string with free() */
const char* tukit_tx_get_snapshot(tukit_tx tx) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        return strdup(transaction->getSnapshot().c_str());
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return nullptr;
    }
}
const char* tukit_tx_get_root(tukit_tx tx) {
    Transaction* transaction = reinterpret_cast<Transaction*>(tx);
    try {
        return transaction->getRoot().c_str();
    } catch (const std::exception &e) {
        fprintf(stderr, "ERROR: %s\n", e.what());
        errmsg = e.what();
        return nullptr;
    }
}
