# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright SUSE LLC

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh
LOG_DRIVER_FLAGS = -- rootlesskit bats --tap --output

TESTS = etc_changes.bats

EXTRA_DIST = $(TESTS)
