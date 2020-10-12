/* SPDX-License-Identifier: GPL-2.0-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  transactional-update - apply updates to the system in an atomic way
 */

#include "transkit.h"
#include <exception>
#include <iostream>
using namespace std;

int main(int argc, const char *argv[]) {
    try {
        Transkit ta{argc, argv};
    } catch (int e) {
	    return e;
    } catch (const exception &e) {
        cerr << "ERROR: " << e.what() << endl;
        return 1;
    }
    return 0;
}
