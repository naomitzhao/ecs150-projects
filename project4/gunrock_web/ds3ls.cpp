#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include "StringUtils.h"
#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

vector<string> splitStringByDelimiter(string s, string delimiter) {
  vector<string> substrings;
  int idx = 0;
  while (idx < (int) s.size() && idx != (int) string::npos) {
    idx = s.find(delimiter);
    string substring = s.substr(0, idx);
    if (substring.size() > 0) substrings.push_back(substring);
    s.erase(0, idx + delimiter.size());
  }
  if (s.length() != 0) substrings.push_back(s);
  return substrings;
}

/*
  Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t& a, const dir_ent_t& b) {
    return std::strcmp(a.name, b.name) < 0;
}
*/

int main(int argc, char *argv[]) {
  if (argc != 3) {
    cerr << argv[0] << ": diskImageFile directory" << endl;
    cerr << "For example:" << endl;
    cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
    return 1;
  }

  // parse command line arguments
  
  Disk *disk = new Disk(argv[1], UFS_BLOCK_SIZE);
  LocalFileSystem *fileSystem = new LocalFileSystem(disk);
  string directory = string(argv[2]);
  
  cout << directory << endl;
  vector<string> pathDirectories = splitStringByDelimiter(directory, "/");
  for (int i = 0; i < (int) pathDirectories.size(); i ++) {
    cout << pathDirectories[i] << " ";
  }
  cout << endl;
  int currInode = 0;
  for (int i = 0; i < (int) pathDirectories.size(); i ++) {
    currInode = fileSystem->lookup(currInode, pathDirectories[i]);
    if (currInode == ENOTFOUND) return -ENOTFOUND;
  }

  cout << "got to directory" << endl;
  
  return 0;
}
