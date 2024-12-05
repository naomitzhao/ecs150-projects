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


// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t& a, const dir_ent_t& b) {
    return std::strcmp(a.name, b.name) < 0;
}


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
  
  vector<string> pathDirectories = splitStringByDelimiter(directory, "/");

  int currInodeNum = 0;
  for (int i = 0; i < (int) pathDirectories.size(); i ++) {
    currInodeNum = fileSystem->lookup(currInodeNum, pathDirectories[i]);
    if (currInodeNum == ENOTFOUND) return -ENOTFOUND;
  }

  super_t super;
  fileSystem->readSuperBlock(&super);
  int numInodes = super.num_inodes;

  inode_t inodes[numInodes];
  fileSystem->readInodeRegion(&super, inodes);

  inode_t currInode = inodes[currInodeNum];

  if (currInode.type == UFS_REGULAR_FILE) {
    cout << currInodeNum << "\t" << pathDirectories.back() << endl;
    return 0;
  }

  // cout << "got to directory" << endl;

  vector<dir_ent_t> files;
  int inodeSize = currInode.size;
  char buffer[inodeSize];
  fileSystem->read(currInodeNum, buffer, inodeSize);

  int bytesToRead = currInode.size;
  // cout << "this directory has " << bytesToRead << " bytes to read" << endl;
  // cout << bytesToRead / sizeof(dir_ent_t) << " entries." << endl;
  int blocksToRead = bytesToRead / 4096;
  if (currInode.size % 4096) blocksToRead ++;

  for (int i = 0; i < blocksToRead; i ++) {
    if (currInode.direct[i] < 0 || currInode.direct[i] > MAX_FILE_SIZE) {
      break;
    }

    char blockBuffer[4096];
    disk->readBlock(currInode.direct[i], blockBuffer);

    for (int j = 0; j < 4096; j += sizeof(dir_ent_t)) {
      dir_ent_t entry; 
      memcpy(&entry, buffer + j, sizeof(dir_ent_t));
      files.push_back(entry);
      bytesToRead -= sizeof(dir_ent_t);
      if (bytesToRead <= 0) break;
    }
  }

  sort(files.begin(), files.end(), compareByName);

  for (int i = 0; i < (int) files.size(); i ++) {
    cout << files[i].inum << "\t" << files[i].name << endl;
  }
  
  return 0;
}
