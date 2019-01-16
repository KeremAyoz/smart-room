/* Arduino SdFat Library
 * Copyright (C) 2009 by William Greiman
 *
 * This file is part of the Arduino SdFat Library
 *
 * This Library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Arduino SdFat Library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "SdFat.h"
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif
#include <Arduino.h>
//------------------------------------------------------------------------------
// callback function for date/time
void (*SdFile::dateTime_)(uint16_t* date, uint16_t* time) = NULL;

#if ALLOW_DEPRECATED_FUNCTIONS
// suppress cpplint warnings with NOLINT comment
void (*SdFile::oldDateTime_)(uint16_t& date, uint16_t& time) = NULL;  // NOLINT
#endif  // ALLOW_DEPRECATED_FUNCTIONS
//------------------------------------------------------------------------------
// add a cluster to a file
uint8_t SdFile::addCluster() {
  if (!vol_->allocContiguous(1, &curCluster_)) return false;

  // if first cluster of file link to directory entry
  if (firstCluster_ == 0) {
    firstCluster_ = curCluster_;
    flags_ |= F_FILE_DIR_DIRTY;
  }
  return true;
}
//------------------------------------------------------------------------------
// Add a cluster to a directory file and zero the cluster.
// return with first block of cluster in the cache
uint8_t SdFile::addDirCluster(void) {
  if (!addCluster()) return false;

  // zero data in cluster insure first cluster is in cache
  uint32_t block = vol_->clusterStartBlock(curCluster_);
  for (uint8_t i = vol_->blocksPerCluster_; i != 0; i--) {
    if (!SdVolume::cacheZeroBlock(block + i - 1)) return false;
  }
  // Increase directory file size by cluster size
  fileSize_ += 512UL << vol_->clusterSizeShift_;
  return true;
}
//------------------------------------------------------------------------------
// cache a file's directory entry
// return pointer to cached entry or null for failure
dir_t* SdFile::cacheDirEntry(uint8_t action) {
  if (!SdVolume::cacheRawBlock(dirBlock_, action)) return NULL;
  return SdVolume::cacheBuffer_.dir + dirIndex_;
}
//------------------------------------------------------------------------------
/**
 *  Close a file and force cached data and directory information
 *  to be written to the storage device.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include no file is open or an I/O error.
 */
uint8_t SdFile::close(void) {
  if (!sync())return false;
  type_ = FAT_FILE_TYPE_CLOSED;
  return true;
}
//------------------------------------------------------------------------------
/**
 * Check for contiguous file and return its raw block range.
 *
 * \param[out] bgnBlock the first block address for the file.
 * \param[out] endBlock the last  block address for the file.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include file is not contiguous, file has zero length
 * or an I/O error occurred.
 */
uint8_t SdFile::contiguousRange(uint32_t* bgnBlock, uint32_t* endBlock) {
  // error if no blocks
  if (firstCluster_ == 0) return false;

  for (uint32_t c = firstCluster_; ; c++) {
    uint32_t next;
    if (!vol_->fatGet(c, &next)) return false;

    // check for contiguous
    if (next != (c + 1)) {
      // error if not end of chain
      if (!vol_->isEOC(next)) return false;
      *bgnBlock = vol_->clusterStartBlock(firstCluster_);
      *endBlock = vol_->clusterStartBlock(c)
                  + vol_->blocksPerCluster_ - 1;
      return true;
    }
  }
}
//------------------------------------------------------------------------------
/**
 * Create and open a new contiguous file of a specified size.
 *
 * \note This function only supports short DOS 8.3 names.
 * See open() for more information.
 *
 * \param[in] dirFile The directory where the file will be created.
 * \param[in] fileName A valid DOS 8.3 file name.
 * \param[in] size The desired file size.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include \a fileName contains
 * an invalid DOS 8.3 file name, the FAT volume has not been initialized,
 * a file is already open, the file already exists, the root
 * directory is full or an I/O error.
 *
 */
uint8_t SdFile::createContiguous(SdFile* dirFile,
        const char* fileName, uint32_t size) {
  // don't allow zero length file
  if (size == 0) return false;
  if (!open(dirFile, fileName, O_CREAT | O_EXCL | O_RDWR)) return false;

  // calculate number of clusters needed
  uint32_t count = ((size - 1) >> (vol_->clusterSizeShift_ + 9)) + 1;

  // allocate clusters
  if (!vol_->allocContiguous(count, &firstCluster_)) {
    remove();
    return false;
  }
  fileSize_ = size;

  // insure sync() will update dir entry
  flags_ |= F_FILE_DIR_DIRTY;
  return sync();
}
//------------------------------------------------------------------------------
/**
 * Return a files directory entry
 *
 * \param[out] dir Location for return of the files directory entry.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 */
uint8_t SdFile::dirEntry(dir_t* dir) {
  // make sure fields on SD are correct
  if (!sync()) return false;

  // read entry
  dir_t* p = cacheDirEntry(SdVolume::CACHE_FOR_READ);
  if (!p) return false;

  // copy to caller's struct
  memcpy(dir, p, sizeof(dir_t));
  return true;
}
//------------------------------------------------------------------------------
/**
 * Format the name field of \a dir into the 13 byte array
 * \a name in standard 8.3 short name format.
 *
 * \param[in] dir The directory structure containing the name.
 * \param[out] name A 13 byte char array for the formatted name.
 */
void SdFile::dirName(const dir_t& dir, char* name) {
  uint8_t j = 0;
  for (uint8_t i = 0; i < 11; i++) {
    if (dir.name[i] == ' ')continue;
    if (i == 8) name[j++] = '.';
    name[j++] = dir.name[i];
  }
  name[j] = 0;
}
//------------------------------------------------------------------------------
/** List directory contents to Serial.
 *
 * \param[in] flags The inclusive OR of
 *
 * LS_DATE - %Print file modification date
 *
 * LS_SIZE - %Print file size.
 *
 * LS_R - Recursive list of subdirectories.
 *
 * \param[in] indent Amount of space before file name. Used for recursive
 * list to indicate subdirectory level.
 */
void SdFile::ls(uint8_t flags, uint8_t indent) {
  dir_t* p;

  rewind();
  while ((p = readDirCache())) {
    // done if past last used entry
    if (p->name[0] == DIR_NAME_FREE) break;

    // skip deleted entry and entries for . and  ..
    if (p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') continue;

    // only list subdirectories and files
    if (!DIR_IS_FILE_OR_SUBDIR(p)) continue;

    // print any indent spaces
    for (int8_t i = 0; i < indent; i++) Serial.print(' ');

    // print file name with possible blank fill
    printDirName(*p, flags & (LS_DATE | LS_SIZE) ? 14 : 0);

    // print modify date/time if requested
    if (flags & LS_DATE) {
       printFatDate(p->lastWriteDate);
       Serial.print(' ');
       printFatTime(p->lastWriteTime);
    }
    // print size if requested
    if (!DIR_IS_SUBDIR(p) && (flags & LS_SIZE)) {
      Serial.print(' ');
      Serial.print(p->fileSize);
    }
    Serial.println();

    // list subdirectory content if requested
    if ((flags & LS_R) && DIR_IS_SUBDIR(p)) {
      uint16_t index = curPosition()/32 - 1;
      SdFile s;
      if (s.open(this, index, O_READ)) s.ls(flags, indent + 2);
      seekSet(32 * (index + 1));
    }
  }
}
//------------------------------------------------------------------------------
// format directory name field from a 8.3 name string
uint8_t SdFile::make83Name(const char* str, uint8_t* name) {
  uint8_t c;
  uint8_t n = 7;  // max index for part before dot
  uint8_t i = 0;
  // blank fill name and extension
  while (i < 11) name[i++] = ' ';
  i = 0;
  while ((c = *str++) != '\0') {
    if (c == '.') {
      if (n == 10) return false;  // only one dot allowed
      n = 10;  // max index for full 8.3 name
      i = 8;   // place for extension
    } else {
      // illegal FAT characters
      uint8_t b;
#if defined(__AVR__)
      PGM_P p = PSTR("|<>^+=?/[];,*\"\\");
      while ((b = pgm_read_byte(p++))) if (b == c) return false;
#elif defined(__arm__)
      const uint8_t valid[] = "|<>^+=?/[];,*\"\\";
      const uint8_t *p = valid;
      while ((b = *p++)) if (b == c) return false;
#endif
      // check size and only allow ASCII printable characters
      if (i > n || c < 0X21 || c > 0X7E)return false;
      // only upper case allowed in 8.3 names - convert lower to upper
      name[i++] = c < 'a' || c > 'z' ?  c : c + ('A' - 'a');
    }
  }
  // must have a file name, extension is optional
  return name[0] != ' ';
}
//------------------------------------------------------------------------------
/** Make a new directory.
 *
 * \param[in] dir An open SdFat instance for the directory that will containing
 * the new directory.
 *
 * \param[in] dirName A valid 8.3 DOS name for the new directory.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include this SdFile is already open, \a dir is not a
 * directory, \a dirName is invalid or already exists in \a dir.
 */
uint8_t SdFile::makeDir(SdFile* dir, const char* dirName) {
  dir_t d;

  // create a normal file
  if (!open(dir, dirName, O_CREAT | O_EXCL | O_RDWR)) return false;

  // convert SdFile to directory
  flags_ = O_READ;
  type_ = FAT_FILE_TYPE_SUBDIR;

  // allocate and zero first cluster
  if (!addDirCluster())return false;

  // force entry to SD
  if (!sync()) return false;

  // cache entry - should already be in cache due to sync() call
  dir_t* p = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!p) return false;

  // change directory entry  attribute
  p->attributes = DIR_ATT_DIRECTORY;

  // make entry for '.'
  memcpy(&d, p, sizeof(d));
  for (uint8_t i = 1; i < 11; i++) d.name[i] = ' ';
  d.name[0] = '.';

  // cache block for '.'  and '..'
  uint32_t block = vol_->clusterStartBlock(firstCluster_);
  if (!SdVolume::cacheRawBlock(block, SdVolume::CACHE_FOR_WRITE)) return false;

  // copy '.' to block
  memcpy(&SdVolume::cacheBuffer_.dir[0], &d, sizeof(d));

  // make entry for '..'
  d.name[1] = '.';
  if (dir->isRoot()) {
    d.firstClusterLow = 0;
    d.firstClusterHigh = 0;
  } else {
    d.firstClusterLow = dir->firstCluster_ & 0XFFFF;
    d.firstClusterHigh = dir->firstCluster_ >> 16;
  }
  // copy '..' to block
  memcpy(&SdVolume::cacheBuffer_.dir[1], &d, sizeof(d));

  // set position after '..'
  curPosition_ = 2 * sizeof(d);

  // write first block
  return SdVolume::cacheFlush();
}
//------------------------------------------------------------------------------
/**
 * Open a file or directory by name.
 *
 * \param[in] dirFile An open SdFat instance for the directory containing the
 * file to be opened.
 *
 * \param[in] fileName A valid 8.3 DOS name for a file to be opened.
 *
 * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
 * OR of flags from the following list
 *
 * O_READ - Open for reading.
 *
 * O_RDONLY - Same as O_READ.
 *
 * O_WRITE - Open for writing.
 *
 * O_WRONLY - Same as O_WRITE.
 *
 * O_RDWR - Open for reading and writing.
 *
 * O_APPEND - If set, the file offset shall be set to the end of the
 * file prior to each write.
 *
 * O_CREAT - If the file exists, this flag has no effect except as noted
 * under O_EXCL below. Otherwise, the file shall be created
 *
 * O_EXCL - If O_CREAT and O_EXCL are set, open() shall fail if the file exists.
 *
 * O_SYNC - Call sync() after each write.  This flag should not be used with
 * write(uint8_t), write_P(PGM_P), writeln_P(PGM_P), or the Arduino Print class.
 * These functions do character at a time writes so sync() will be called
 * after each byte.
 *
 * O_TRUNC - If the file exists and is a regular file, and the file is
 * successfully opened and is not read only, its length shall be truncated to 0.
 *
 * \note Directory files must be opened read only.  Write and truncation is
 * not allowed for directory files.
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include this SdFile is already open, \a difFile is not
 * a directory, \a fileName is invalid, the file does not exist
 * or can't be opened in the access mode specified by oflag.
 */
uint8_t SdFile::open(SdFile* dirFile, const char* fileName, uint8_t oflag) {
  uint8_t dname[11];
  dir_t* p;

  // error if already open
  if (isOpen())return false;

  if (!make83Name(fileName, dname)) return false;
  vol_ = dirFile->vol_;
  dirFile->rewind();

  // bool for empty entry found
  uint8_t emptyFound = false;

  // search for file
  while (dirFile->curPosition_ < dirFile->fileSize_) {
    uint8_t index = 0XF & (dirFile->curPosition_ >> 5);
    p = dirFile->readDirCache();
    if (p == NULL) return false;

    if (p->name[0] == DIR_NAME_FREE || p->name[0] == DIR_NAME_DELETED) {
      // remember first empty slot
      if (!emptyFound) {
        emptyFound = true;
        dirIndex_ = index;
        dirBlock_ = SdVolume::cacheBlockNumber_;
      }
      // done if no entries follow
      if (p->name[0] == DIR_NAME_FREE) break;
    } else if (!memcmp(dname, p->name, 11)) {
      // don't open existing file if O_CREAT and O_EXCL
      if ((oflag & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) return false;

      // open found file
      return openCachedEntry(0XF & index, oflag);
    }
  }
  // only create file if O_CREAT and O_WRITE
  if ((oflag & (O_CREAT | O_WRITE)) != (O_CREAT | O_WRITE)) return false;

  // cache found slot or add cluster if end of file
  if (emptyFound) {
    p = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
    if (!p) return false;
  } else {
    if (dirFile->type_ == FAT_FILE_TYPE_ROOT16) return false;

    // add and zero cluster for dirFile - first cluster is in cache for write
    if (!dirFile->addDirCluster()) return false;

    // use first entry in cluster
    dirIndex_ = 0;
    p = SdVolume::cacheBuffer_.dir;
  }
  // initialize as empty file
  memset(p, 0, sizeof(dir_t));
  memcpy(p->name, dname, 11);

  // set timestamps
  if (dateTime_) {
    // call user function
    dateTime_(&p->creationDate, &p->creationTime);
  } else {
    // use default date/time
    p->creationDate = FAT_DEFAULT_DATE;
    p->creationTime = FAT_DEFAULT_TIME;
  }
  p->lastAccessDate = p->creationDate;
  p->lastWriteDate = p->creationDate;
  p->lastWriteTime = p->creationTime;

  // force write of entry to SD
  if (!SdVolume::cacheFlush()) return false;

  // open entry in cache
  return openCachedEntry(dirIndex_, oflag);
}
//------------------------------------------------------------------------------
/**
 * Open a file by index.
 *
 * \param[in] dirFile An open SdFat instance for the directory.
 *
 * \param[in] index The \a index of the directory entry for the file to be
 * opened.  The value for \a index is (directory file position)/32.
 *
 * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
 * OR of flags O_READ, O_WRITE, O_TRUNC, anяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяя---------
/** %Print a directory date field to Serial.
 *
 *  Format is yyyy-mm-dd.
 *
 * \param[in] fatDate The date field from a directory entry.
 */
void SdFile::printFatDate(uint16_t fatDate) {
  Serial.print(FAT_YEAR(fatDate));
  Serial.print('-');
  printTwoDigits(FAT_MONTH(fatDate));
  Serial.print('-');
  printTwoDigits(FAT_DAY(fatDate));
}
//------------------------------------------------------------------------------
/** %Print a directory time field to Serial.
 *
 * Format is hh:mm:ss.
 *
 * \param[in] fatTime The time field from a directory entry.
 */
void SdFile::printFatTime(uint16_t fatTime) {
  printTwoDigits(FAT_HOUR(fatTime));
  Serial.print(':');
  printTwoDigits(FAT_MINUTE(fatTime));
  Serial.print(':');
  printTwoDigits(FAT_SECOND(fatTime));
}
//------------------------------------------------------------------------------
/** %Print a value as two digits to Serial.
 *
 * \param[in] v Value to be printed, 0 <= \a v <= 99
 */
void SdFile::printTwoDigits(uint8_t v) {
  char str[3];
  str[0] = '0' + v/10;
  str[1] = '0' + v % 10;
  str[2] = 0;
  Serial.print(str);
}
//------------------------------------------------------------------------------
/**
 * Read data from a file starting at the current position.
 *
 * \param[out] buf Pointer to the location that will receive the data.
 *
 * \param[in] nbyte Maximum number of bytes to read.
 *
 * \return For success read() returns the number of bytes read.
 * A value less than \a nbyte, including zero, will be returned
 * if end of file is reached.
 * If an error occurs, read() returns -1.  Possible errors include
 * read() called before a file has been opened, corrupt file system
 * or an I/O error occurred.
 */
int16_t SdFile::read(void* buf, uint16_t nbyte) {
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);

  // error if not open or write only
  if (!isOpen() || !(flags_ & O_READ)) return -1;

  // max bytes left in file
  if (nbyte > (fileSize_ - curPosition_)) nbyte = fileSize_ - curPosition_;

  // amount left to read
  uint16_t toRead = nbyte;
  while (toRead > 0) {
    uint32_t block;  // raw device block number
    uint16_t offset = curPosition_ & 0X1FF;  // offset in block
    if (type_ == FAT_FILE_TYPE_ROOT16) {
      block = vol_->rootDirStart() + (curPosition_ >> 9);
    } else {
      uint8_t blockOfCluster = vol_->blockOfCluster(curPosition_);
      if (offset == 0 && blockOfCluster == 0) {
        // start of new cluster
        if (curPosition_ == 0) {
          // use first cluster in file
          curCluster_ = firstCluster_;
        } else {
          // get next cluster from FAT
          if (!vol_->fatGet(curCluster_, &curCluster_)) return -1;
        }
      }
      block = vol_->clusterStartBlock(curCluster_) + blockOfCluster;
    }
    uint16_t n = toRead;

    // amount to be read from current block
    if (n > (512 - offset)) n = 512 - offset;

    // no buffering needed if n == 512 or user requests no buffering
    if ((unbufferedRead() || n == 512) &&
      block != SdVolume::cacheBlockNumber_) {
      if (!vol_->readData(block, offset, n, dst)) return -1;
      dst += n;
    } else {
      // read block to cache and copy data to caller
      if (!SdVolume::cacheRawBlock(block, SdVolume::CACHE_FOR_READ)) return -1;
      uint8_t* src = SdVolume::cacheBuffer_.data + offset;
      uint8_t* end = src + n;
      while (src != end) *dst++ = *src++;
    }
    curPosition_ += n;
    toRead -= n;
  }
  return nbyte;
}
//------------------------------------------------------------------------------
/**
 * Read the next directory entry from a directory file.
 *
 * \param[out] dir The dir_t struct that will receive the data.
 *
 * \return For success readDir() returns the number of bytes read.
 * A value of zero will be returned if end of file is reached.
 * If an error occurs, readDir() returns -1.  Possible errors include
 * readDir() called before a directory has been opened, this is not
 * a directory file or an I/O error occurred.
 */
int8_t SdFile::readDir(dir_t* dir) {
  int8_t n;
  // if not a directory file or miss-positioned return an error
  if (!isDir() || (0X1F & curPosition_)) return -1;

  while ((n = read(dir, sizeof(dir_t))) == sizeof(dir_t)) {
    // last entry if DIR_NAME_FREE
    if (dir->name[0] == DIR_NAME_FREE) break;
    // skip empty entries and entry for .  and ..
    if (dir->name[0] == DIR_NAME_DELETED || dir->name[0] == '.') continue;
    // return if normal file or subdirectory
    if (DIR_IS_FILE_OR_SUBDIR(dir)) return n;
  }
  // error, end of file, or past last entry
  return n < 0 ? -1 : 0;
}
//------------------------------------------------------------------------------
// Read next directory entry into the cache
// Assumes file is correctly positioned
dir_t* SdFile::readDirCache(void) {
  // error if not directory
  if (!isDir()) return NULL;

  // index of entry in cache
  uint8_t i = (curPosition_ >> 5) & 0XF;

  // use read to locate and cache block
  if (read() < 0) return NULL;

  // advance to next entry
  curPosition_ += 31;

  // return pointer to entry
  return (SdVolume::cacheBuffer_.dir + i);
}
//------------------------------------------------------------------------------
/**
 * Remove a file.
 *
 * The directory entry and all data for the file are deleted.
 *
 * \note This function should not be used to delete the 8.3 version of a
 * file that has a long name. For example if a file has the long name
 * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the file read-only, is a directory,
 * or an I/O error occurred.
 */
uint8_t SdFile::remove(void) {
  // free any clusters - will fail if read-only or directory
  if (!truncate(0)) return false;

  // cache directory entry
  dir_t* d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) return false;

  // mark entry deleted
  d->name[0] = DIR_NAME_DELETED;

  // set this SdFile closed
  type_ = FAT_FILE_TYPE_CLOSED;

  // write entry to SD
  return SdVolume::cacheFlush();
}
//------------------------------------------------------------------------------
/**
 * Remove a file.
 *
 * The directory entry and all data for the file are deleted.
 *
 * \param[in] dirFile The directory that contains the file.
 * \param[in] fileName The name of the file to be removed.
 *
 * \note This function should not be used to delete the 8.3 version of a
 * file that has a long name. For example if a file has the long name
 * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the file is a directory, is read only,
 * \a dirFile is not a directory, \a fileName is not found
 * or an I/O error occurred.
 */
uint8_t SdFile::remove(SdFile* dirFile, const char* fileName) {
  SdFile file;
  if (!file.open(dirFile, fileName, O_WRITE)) return false;
  return file.remove();
}
//------------------------------------------------------------------------------
/** Remove a directory file.
 *
 * The directory file will be removed only if it is empty and is not the
 * root directory.  rmDir() follows DOS and Windows and ignores the
 * read-only attribute for the directory.
 *
 * \note This function should not be used to delete the 8.3 version of a
 * directory that has a long name. For example if a directory has the
 * long name "New folder" you should not delete the 8.3 name "NEWFOL~1".
 *
 * \return The value one, true, is returned for success and
 * the value zero, false, is returned for failure.
 * Reasons for failure include the file is not a directory, is the root
 * directory, is not empty, or an I/O error occurred.
 */
uint8_t SdFile::rmDir(void) {
  // must be open subdirectory
  if (!isSubDir()) return false;

  rewind();

@      Ђ                              Ђ                                     Ђ      `                           Ђ                                                                     Ђ             Ђ             Ђ                        Ђ                 ђ                                 Ђ              Ђ      Ђ      Ђ  @                            Ђ                                                                 Ђ    @   Ђ Ђ Ђ                                  Ђ                                          Ђ                   Ђ                           Ђ   Ђ              Ђ                          Ђ           Ђ         Ђ                                          Ђ                                                                                                                  Ђ              Ђ   Ѓ                         Ђ                       Ђ                   Ђ                    Ђ   Ђ                                                                                       Ђ            @                                                Ђ Ђ Ђ @                             Ђ               @                                               ЂЂ   ‚                                              Ђ                                                            Ђ  Ђ                                          Ђ     Ђ      Ђ                  …                                                                             Ђ                                                                       Ђ                    Ђ                                                       Ђ        Ђ          Ђ        Ђ   
                                 Ђ        ‚                                              Ђ                                                 Ђ             Ђ Ђ       Ђ        Ђ         Ђ        Ђ Ђ                 Ђ              Ђ  Ђ  Ђ  ЂЂ               ђ           Ђ    Ђ          Ђ                   Ђ                                                                          Ђ               @       Ђ   Ђ                                   ЂЂ                                                                      Ђ                  Ђ                           Ђ                                        „ Ђ               Ђ       Ђ   	       Ђ                 ЂЂ                                                            Њ                  Ђ                            Ђ                                Ђ Ђ                                   Ђ                        Ђ                      Ђ        Ђ      ЂЂ             Ђ     ЂЂ                  Ђ                 Ђ@ Ђ Ђ       Ђ                  Ђ              Ѓ                  Ђ                                                                                             €        Ђ                                       Ђ          Ђ Ђ Ђ           Ђ                                      Ђ                                               Ђ                                    ђ                „                                                    Ђ                 @            Ђ               €    Ђ  „                         Ђ          @      Ђ                 ”Ђ€                    Ђ                     Ђ                           Њ         ЂЂ                          @    Ђ                                                                         Ђ       „                                     Ђ                                       ‚                      Ђ     Ђ Ђ       Ђ             €                 ”                             Ђ             Ђ  €                                                                                                €                           Ђ    Ђ  Ђ                                 Ђ            Ђ     Ђ               ‚     Ђ   Ђ        Ђ    Ђ                                                                                                                                                                                                            @             @                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      @                                              @                                                                                       @                                                                                                                                                                                                                         @                                                                                                                                                                                                                                                                                                                                                                                  @                                                                                          @                                                                                                                                                                                                                                                                                    @                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           @                                                                                                                                                                                                                                                                                                                                                                                                                                                          @                                                                                                                      @                                                                          @                                                                             @                                              @    @                                    @             @                        A             @                @                                                   @                                                                                                     @                                                     @                                       @     @                                               @                                  @                @                                                                          0                                             @                             @                    (                                            @                                                    @ @                                                 @                                                                                               яяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяяя-------------------------------------------------------------------
/**
 * Write a byte to a file. Required by the Arduino Print class.
 *
 * Use SdFile::writeError to check for errors.
 */
size_t SdFile::write(uint8_t b) {
  return write(&b, 1);
}
//------------------------------------------------------------------------------
/**
 * Write a string to a file. Used by the Arduino Print class.
 *
 * Use SdFile::writeError to check for errors.
 */
size_t SdFile::write(const char* str) {
  return write(str, strlen(str));
}
#ifdef __AVR__
//------------------------------------------------------------------------------
/**
 * Write a PROGMEM string to a file.
 *
 * Use SdFile::writeError to check for errors.
 */
void SdFile::write_P(PGM_P str) {
  for (uint8_t c; (c = pgm_read_byte(str)); str++) write(c);
}
//------------------------------------------------------------------------------
/**
 * Write a PROGMEM string followed by CR/LF to a file.
 *
 * Use SdFile::writeError to check for errors.
 */
void SdFile::writeln_P(PGM_P str) {
  write_P(str);
  println();
}
#endif
