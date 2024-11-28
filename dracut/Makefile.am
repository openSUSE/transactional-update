#
# Copyright (c) 2018 Ignaz Forster <iforster@suse.de>
#

modulesdir = @DRACUTDIR@/50transactional-update

modules_SCRIPTS = transactional-update-etc-cleaner.sh module-setup.sh
modules_DATA = transactional-update-etc-cleaner.service

libexec_PROGRAMS = transactional-update-sync-etc-state
transactional_update_sync_etc_state_SOURCES = sync-etc-state.cpp

EXTRA_DIST = $(SCRIPTS) $(DATA)
