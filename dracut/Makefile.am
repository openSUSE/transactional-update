# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

modulesdir = @DRACUTDIR@/50transactional-update

modules_SCRIPTS = module-setup.sh transactional-update-sync-etc-state.sh
modules_DATA = transactional-update-sync-etc-state.service

libexec_PROGRAMS = transactional-update-sync-etc-state
transactional_update_sync_etc_state_SOURCES = sync-etc-state.cpp

EXTRA_DIST = $(SCRIPTS) $(DATA)
