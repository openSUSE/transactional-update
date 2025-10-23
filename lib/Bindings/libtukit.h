/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  This is the C API for libtukit.
  For documentation please see the corresponding classes in the C++ header
  files.
 */

#ifndef T_U_TUKIT_H
#define T_U_TUKIT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"

typedef enum {
    None=0, Error, Info, Debug
} tukit_loglevel;

const char* tukit_get_errmsg();
void tukit_set_loglevel(tukit_loglevel lv);
int tukit_set_logoutput(char *fields);
typedef void* tukit_tx;
tukit_tx tukit_new_tx();
tukit_tx tukit_new_tx_set_manager(char* manager);
void tukit_free_tx(tukit_tx tx);
int tukit_tx_init(tukit_tx tx, char* base);
int tukit_tx_init_with_desc(tukit_tx tx, char* base, char* description);
int tukit_tx_discard_if_unchanged(tukit_tx tx, int discard);
int tukit_tx_resume(tukit_tx tx, char* id);
int tukit_tx_execute(tukit_tx tx, char* argv[], const char* output[]);
int tukit_tx_call_ext(tukit_tx tx, char* argv[], const char* output[]);
int tukit_tx_finalize(tukit_tx tx);
int tukit_tx_keep(tukit_tx tx);
int tukit_tx_send_signal(tukit_tx tx, int signal);
int tukit_tx_is_initialized(tukit_tx tx);
const char* tukit_tx_get_snapshot(tukit_tx tx);
const char* tukit_tx_get_root(tukit_tx tx);
typedef void* tukit_sm_list;
tukit_sm_list tukit_sm_get_list(size_t* len, const char* columns);
const char* tukit_sm_get_list_value(tukit_sm_list list, size_t row, char* columns);
void tukit_free_sm_list(tukit_sm_list list);
int tukit_sm_deletesnap(const char* id);
int tukit_sm_rollbackto(const char* id);
int tukit_reboot(const char* method);

#ifdef __cplusplus
}
#endif
#endif // T_U_TUKIT_H
