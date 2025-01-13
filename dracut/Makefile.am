#
# Copyright (c) 2018 Ignaz Forster <iforster@suse.de>
#

modulesdir = @DRACUTDIR@/50transactional-update

modules_SCRIPTS = module-setup.sh transactional-update-sync-etc-state.sh
modules_DATA = transactional-update-sync-etc-state.service

libexec_PROGRAMS = transactional-update-sync-etc-state
transactional_update_sync_etc_state_SOURCES = sync-etc-state.cpp

EXTRA_DIST = $(SCRIPTS) $(DATA)
