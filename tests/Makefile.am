# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

BATS_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh
AM_BATS_LOG_DRIVER_FLAGS = -- bats --tap --filter-tags '!needroot' --output
ROOT_LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh
AM_ROOT_LOG_DRIVER_FLAGS = -- fakeroot bats --tap --filter-tags 'needroot' --output

TEST_EXTENSIONS = .bats .root
TESTS = etc_changes.bats etc_changes_as.root

EXTRA_DIST = $(TESTS)
