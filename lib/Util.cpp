/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: 2020 SUSE LLC */

/*
  Helper class
 */

#include "Log.hpp"
#include "Util.hpp"
#include "Exceptions.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>

#include <selinux/selinux.h>
#include <selinux/context.h>

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

    if (rc == EXIT_SUCCESS) {
        tulog.debug("◸", result, "◿");
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

void Util::se_copycontext(const std::filesystem::path ref, const std::filesystem::path dst)
{
    char* context = NULL;
    if (getfilecon(ref.c_str(), &context) <= 0)
        return;
    tulog.debug("selinux context on " + ref.string() + ": " + std::string(context));
    if (setfilecon(dst.c_str(), context) != 0) {
        freecon(context);
        throw std::runtime_error{"applying selinux context for " + dst.string() + " failed: " + std::string(strerror(errno))};
    }
    freecon(context);
}

bool Util::se_is_context_type(const std::filesystem::path ref, const std::string type)
{
    bool ret = false;
    char* context_str;
    if (getfilecon(ref.c_str(), &context_str) <= 0)
        return ret;
    context_t context = context_new(context_str);
    if (strcmp(context_type_get(context), type.c_str()) == 0)
        ret = true;
    context_free(context);
    freecon(context_str);
    return ret;
}

} // namespace TransactionalUpdate
