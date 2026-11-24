/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#include "Log.hpp"
#include "tukit.hpp"
#include <exception>
#include <iostream>
using namespace std;

int main(int argc, char *argv[]) {
    try {
        TUKit ta{argc, argv};
    } catch (int e) {
        return e;
    } catch (const exception &e) {
        tulog.error("ERROR: ", e.what());
        return 1;
    }
    return 0;
}
