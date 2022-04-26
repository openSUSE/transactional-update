/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Helper class
 */

#include "Log.hpp"
#include "Util.hpp"
#include "Exceptions.hpp"
#include <algorithm>
#include <sys/wait.h>

namespace TransactionalUpdate {

using namespace std;

string Util::exec(const string cmd) {
    array<char, 128> buffer;
    string result;

    tulog.debug("Executing `", cmd, "`:");

    // Ensure there is a sane path set
    const string cmd_pathenv = "PATH='/usr/bin:/usr/sbin:/bin:/sbin' " + cmd;

    auto pipe = popen(cmd_pathenv.c_str(), "r");

    if (!pipe)
        throw runtime_error{"popen() failed!"};

    while (!feof(pipe)) {
        if (fgets(buffer.data(), 128, pipe) != nullptr)
            result += buffer.data();
    }

    int rc = pclose(pipe);

    tulog.debug("◸", result, "◿");
    if (WIFEXITED(rc) && WEXITSTATUS(rc) != EXIT_SUCCESS) {
        throw ExecutionException{"`" + cmd + "` returned with error code " + std::to_string(WEXITSTATUS(rc)) + ".", WEXITSTATUS(rc)};
    } else if (WIFSIGNALED(rc)) {
        throw ExecutionException{"`" + cmd + "` was killed by signal " + std::to_string(WTERMSIG(rc)) + ".", WTERMSIG(rc)};
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
