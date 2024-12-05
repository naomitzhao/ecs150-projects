#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include <string.h>

#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;


LocalFileSystem::LocalFileSystem(Disk *disk) {
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super) {
  char buffer[4096];
  this->disk->readBlock(0, buffer);
  memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {

  int bitmapAddr = super->inode_bitmap_addr;
  int bitmapBlocks = super->inode_bitmap_len;
  int unreadBits = super->num_inodes;

  for (int i = 0; i < bitmapBlocks; i ++) {
    char buffer[4096];
    this->disk->readBlock(bitmapAddr, buffer);
    int bitsToRead = 4096;
    if (unreadBits < 4096) bitsToRead = unreadBits;
    memcpy(inodeBitmap + i * 4096, buffer, super->num_inodes);
    unreadBits -= bitsToRead;
  }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {

}

void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  int bitmapAddr = super->data_bitmap_addr;
  int bitmapBlocks = super->data_bitmap_len;
  int unreadBits = super->num_inodes;
  
  for (int i = 0; i < bitmapBlocks; i ++) {
    char buffer[4096];
    this->disk->readBlock(bitmapAddr + i, buffer);
    int bitsToRead = 4096;
    if (unreadBits < 4096) bitsToRead = unreadBits;
    memcpy(dataBitmap + i * 4096, buffer, bitsToRead);
    unreadBits -= bitsToRead;
  }
}

void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {

}

void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  int numInodes = super->num_inodes;
  int blockInodes = 4096 / sizeof(inode_t);
  int inodeRegionAddr = super->inode_region_addr;
  int inodeIdx = 0;
  int lastBlock = -1;
  while (inodeIdx < numInodes) {
    char buffer[4096];
    int blockNum = inodeIdx / blockInodes;
    int offset = inodeIdx % blockInodes;
    if (blockNum != lastBlock) {
      this->disk->readBlock(inodeRegionAddr + blockNum, buffer);
    }
    memcpy(&inodes[inodeIdx], buffer + sizeof(inode_t) * offset, sizeof(inode_t));
    inodeIdx += 1;
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {

}

int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  inode_t parentInode;
  stat(parentInodeNumber, &parentInode);
  if (parentInode.type != UFS_DIRECTORY) {
    return -1;
  }

  for (int i = 0; i < DIRECT_PTRS; i ++) {
    // cout << "reading ptr " << i << " to block " << parentInode.direct[i] << endl;
    if (parentInode.direct[i] < 0 || parentInode.direct[i] > MAX_FILE_SIZE) {
      // cout << "block num less than 1" << endl;
      break;
    }
    char buffer[4096];
    this->disk->readBlock(parentInode.direct[i], buffer);
    dir_ent_t entry; 

    for (int j = 0; j < 4096; j += sizeof(dir_ent_t)) {
      memcpy(&entry, buffer + j, sizeof(dir_ent_t));
      // cout << entry.name << ", ";
      
      if (strcmp(entry.name, name.c_str()) == 0) {
        // cout << "found " << entry.name << ", inode number = " << entry.inum << endl; 
        return entry.inum;
      }
    }
  }
  
  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super;
  readSuperBlock(&super);
  int numInodes = super.num_inodes;
  inode_t inodes[numInodes];
  readInodeRegion(&super, inodes);
  memcpy(inode, &inodes[inodeNumber], sizeof(inode_t));
  return 0;
}

bool bitIsSet(int index, unsigned char *bitmap) {
  int byte = index / 8; 
  int offset = index % 8;
  int chosenByte = bitmap[byte];
  int bit = chosenByte >> (8 - offset - 1);
  if (bit == 1) return true;
  return false;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  // todo: fix this super janky implementation it's not even correct

  super_t super;
  readSuperBlock(&super);
  int numInodes = super.num_inodes;

  inode_t inodes[numInodes];
  readInodeRegion(&super, inodes);
  inode_t inode = inodes[inodeNumber];

  char* charBuffer = static_cast<char*>(buffer);
  // cout << "size of charBuffer is " << sizeof(charBuffer) << endl;
  // cout << charBuffer << endl;

  int bytesToRead = inode.size;
  int blocksToRead = bytesToRead / 4096;
  if (bytesToRead % 4096) blocksToRead ++;

  // cout << "reading " << bytesToRead << " bytes from " << blocksToRead << " blocks" << endl;

  for (int i = 0; i < blocksToRead; i ++) {
    char blockBuffer[4096];
    this->disk->readBlock(inode.direct[i], blockBuffer);
    int readingBytes = 4096;
    if (bytesToRead < 4096) readingBytes = bytesToRead;
    // cout << readingBytes << " from block " << inode.direct[i] << endl;
    memcpy(charBuffer + i * 4096, blockBuffer, readingBytes);
    // cout << charBuffer << endl;
    bytesToRead -= readingBytes;
  }

  // cout << charBuffer << endl;
  // cout << "done with read" << endl;

  return size;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}

