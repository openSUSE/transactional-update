/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Helper class
 */

#include "Log.h"
#include "Util.h"
#include "Exceptions.h"
#include <algorithm>

namespace TransactionalUpdate {

using namespace std;

string Util::exec(const string cmd) {
    array<char, 128> buffer;
    string result;

    tulog.info("Executing `", cmd, "`:");

    auto pipe = popen(cmd.c_str(), "r");

    if (!pipe)
        throw runtime_error{"popen() failed!"};

    while (!feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }

    int rc = pclose(pipe);

    if (rc == EXIT_SUCCESS) {
        tulog.info("◸", result, "◿");
    } else {
        throw ExecutionException{"`" + cmd + "` returned with error code " + to_string(rc%255) + ".", rc};
    }

    return result;
}

// trim from start (in place)
void Util::ltrim(string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
void Util::rtrim(string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

void Util::stub(string option) {
    cerr << "STUB: '" << option << "' not implemented yet." << endl;
}

// trim from both ends (in place)
void Util::trim(string &s) {
    ltrim(s);
    rtrim(s);
}

} // namespace TransactionalUpdate
