/* transactional-update-helper - native helper scripts for transactional-update

   Copyright (C) 2018 SUSE Linux GmbH

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <zypp/ZYppFactory.h>
#include <zypp/RepoManager.h>
using namespace std;

void usage(string command) {
  cout << "Usage: " << command << " [disable-optical]" << endl;
}

int disable_optical() {
  const zypp::Pathname &sysroot = "/";
  zypp::RepoManager repoManager(sysroot);
  zypp::RepoInfoList repos = repoManager.knownRepositories();

  if (repos.empty()) {
    cerr << "No repositories found in " << sysroot << endl;
    return 2;
  }

  for_(it, repos.begin(), repos.end()) {
    zypp::RepoInfo &repo(*it);
    if (repo.url().schemeIsVolatile() && repo.enabled()) {
      cout << "Disabling optical media repository " << repo.name() << "..." << endl;
      repo.setEnabled(false);
      try {
        repoManager.modifyRepository(repo.alias(), repo);
      } catch (const zypp::Exception &e) {
        cerr << "Error while disabling repository '" << repo.name() << "': " << e << endl;
      }
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc == 1) {
    usage(argv[0]);
    return 1;
  }

  string command = argv[1];
  if (command == "disable-optical") {
    disable_optical();
  } else {
    usage(argv[0]);
  }
  return 0;
}

