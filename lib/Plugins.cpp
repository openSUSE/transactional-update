/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* SPDX-FileCopyrightText: Copyright SUSE LLC */

/* Plugin mechanism for tukit */

#include "Exceptions.hpp"
#include "Log.hpp"
#include "Plugins.hpp"
#include "Util.hpp"
#include <regex>
#include <set>
#include <unistd.h>

namespace TransactionalUpdate {

using namespace std;

Plugins::Plugins(TransactionalUpdate::Transaction* transaction, bool ignore_error)
    : transaction{transaction}, ignore_error{ignore_error} {
    set<string> plugins_set{};

    const filesystem::path plugins_dir{filesystem::path(CONFDIR)/"tukit"/"plugins"};
    const filesystem::path system_plugins_dir{filesystem::path(PREFIX)/"lib"/"tukit"/"plugins"};

    for (auto d: {plugins_dir, system_plugins_dir}) {
        if (!filesystem::exists(d))
            continue;

        for (auto const& dir_entry: filesystem::directory_iterator{d}) {
            auto path = dir_entry.path();
            auto filename = dir_entry.path().filename();

            // Plugins can be shadowed, so a plugin in /etc can
            // replace one from /usr/lib
            if (plugins_set.count(filename) != 0)
                continue;

            // If is a symlink to /dev/null, ignore and shadow it
            if (filesystem::is_symlink(path) && filesystem::read_symlink(path) == "/dev/null") {
                plugins_set.insert(filename);
                continue;
            }

            // If the plugin is not executable, ignore it
            if (!(filesystem::is_regular_file(path) && (access(path.c_str(), X_OK) == 0)))
                continue;

            tulog.info("Found plugin ", path);
            plugins.push_back(path);
            plugins_set.insert(filename);
        }
    }
}

Plugins::~Plugins() {
    plugins.clear();
}

void Plugins::run(string stage, string args) {
    std::string output;

    for (auto& p: plugins) {
        std::string cmd = p.string() + " " + stage;
        if (!args.empty())
            cmd.append(" " + args);

        try {
            output = Util::exec(cmd);
            if (!output.empty())
                tulog.info("Output of plugin ", p, ":\n", output, "---");
        } catch (const ExecutionException &e) {
            tulog.error("ERROR: ", e.what());
            if (!e.output.empty())
                tulog.error("Output of plugin ", p, ":\n", e.output, "---");
            if (!ignore_error)
                if (WIFEXITED(e.returncode))
                    throw(WEXITSTATUS(e.returncode));
                else
                    throw(-1);
        }
    }
}

void Plugins::run(string stage, char* argv[]) {
    std::string args;

    if (transaction != nullptr)
        args.append(transaction->getBindDir().string() + " " + transaction->getSnapshot());

    int i = 0;
    while (argv != nullptr && argv[i]) {
        std::string param = argv[i++];
        param = std::regex_replace(param, std::regex("'"), "'\"'\"'");
        args.append(" '");
        args.append(param);
        args.append("'");
    }

    run(stage, args);
}

} // namespace TransactionalUpdate
