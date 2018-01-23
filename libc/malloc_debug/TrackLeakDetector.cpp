/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of The Linux Foundation nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unwind.h>
#include <string>

#include "backtrace.h"
#include "MapData.h"
#include "TrackLeakDetector.h"

unsigned char *g_type[] = {
  (unsigned char *)"none",
  (unsigned char *)"file",
  (unsigned char *)"socket",
  (unsigned char *)"mmap",
};

// Variables for statistics.
static size_t g_discard = 0;
static size_t g_added = 0;
static size_t g_addedType[LEAK_TYPE_MAX];
static size_t g_sameType[LEAK_TYPE_MAX];

static MapData g_map_data;
static const MapEntry* g_libDetectorCodeMap = nullptr;

// ------------ Customized unwind function begin ------------//
#if defined(__aarch64__)
#define MAX_FP_DISTANCE (2048)

uintptr_t __attribute__((noinline)) getFP(void)
{
  uintptr_t fp = 0;

  __asm__ __volatile__(
    "mov %[fp], x29\n\t"
    : [fp] "=r" (fp)
    :
    :"memory");

  return fp;
}

size_t backtrace_get2(uintptr_t* frames, size_t frame_count)
{
  size_t cur_frame = 0;
  uintptr_t * fp = (uintptr_t *)getFP();
  uintptr_t lr = 0;
  uintptr_t fp_distance = 0;

  if ((uintptr_t)(*fp) <= (uintptr_t)fp) {
    fp_distance = 0;
  }
  else {
    fp_distance = (uintptr_t)(*fp) - (uintptr_t)fp;
  }

  while(*fp && (cur_frame < frame_count) && (fp_distance > 0) && (fp_distance < MAX_FP_DISTANCE))
  {
    lr = (*(fp + 1)) - 4;

    frames[cur_frame++] = lr;

    fp = (uintptr_t *)(*fp);
    if ((uintptr_t)(*fp) <= (uintptr_t)fp) {
      fp_distance = 0;
    }
    else {
      fp_distance = (uintptr_t)(*fp) - (uintptr_t)fp;
    }

  }

  return cur_frame;
}
#else
size_t backtrace_get2(uintptr_t* frames, size_t frame_count)
{
  return backtrace_get(frames, frame_count);
}
#endif
// ------------ Customized unwind function ended ------------//

TrackLeakDetector::TrackLeakDetector() {
}

int TrackLeakDetector::find(unsigned long key) {
  for (int i = 0; i < MAX_LEAK_RECORDS; i++) {
    if (headersLock[i].key == key && headersLock[i].type != LEAK_TYPE_NONE)
      return i;
  }

  return -1;
}

int TrackLeakDetector::findBlank() {
  for (int i = 0; i < MAX_LEAK_RECORDS; i++) {
    if (headersLock[i].type == LEAK_TYPE_NONE)
      return i;
  }

  return -1;
}

bool addressInLibDetector(uintptr_t* frames, size_t frame_count) {
  // Start from index '2' since '0' and '1' are in 'libc_leak_detector.so'.
  for (size_t i = 2; i < frame_count; i++) {
    if (frames[i] >= g_libDetectorCodeMap->start && frames[i] < g_libDetectorCodeMap->end) {
      return true;
    }
  }
  return false;
}

void TrackLeakDetector::LoadMap() {
  if (g_libDetectorCodeMap != NULL)
    return;

  g_libDetectorCodeMap = g_map_data.find((uintptr_t)addressInLibDetector);
}

void TrackLeakDetector::Add(unsigned long key, leak_type type) {
  BacktraceHeader header;
  memset(&header, 0, sizeof(header));
  header.num_frames = backtrace_get2(header.frames, DEFAULT_BACKTRACE_FRAMES);
  char* callstack = NULL;

  pthread_mutex_lock(&mutex_);

  int index = find(key);
  if (index != -1) {  // Find the item has the same key, just update the number.
    g_sameType[type] ++;
    headersLock[index].num_same ++;
  }
  else {
    g_added ++;
    g_addedType[type] ++;
    int index = findBlank();
    if (index != -1) {
      header.key = key;
      header.num_same = 1;
      header.type = type;
      header.java_stack = NULL;
      memcpy(&headersLock[index], &header, sizeof(BacktraceHeader));
      headersLock[index].java_stack = callstack;
      callstack = NULL;
    }
    else {
      // Don't have enough space, we need to enlarge 'MAX_LEAK_RECORDS'.
      g_discard ++;
    }
  }

  pthread_mutex_unlock(&mutex_);

  if (callstack) {
    free(callstack);
    callstack = NULL;
  }
}

void TrackLeakDetector::Remove(unsigned long key) {
  BacktraceHeader header;
  header.num_frames = backtrace_get2(header.frames, DEFAULT_BACKTRACE_FRAMES);

  if (g_libDetectorCodeMap && addressInLibDetector(header.frames, header.num_frames)) {
    return;
  }

  pthread_mutex_lock(&mutex_);

  int index = find(key);
  if (index != -1) {
    leak_type type = headersLock[index].type;
    if (g_sameType[type] >= (headersLock[index].num_same - 1))
      g_sameType[type] -= headersLock[index].num_same - 1;
    else
      g_sameType[type] = 0;
    g_addedType[type] --;
    g_added --;
    headersLock[index].type = LEAK_TYPE_NONE; // Make this item 'blank'.
  }

  pthread_mutex_unlock(&mutex_);
}

void TrackLeakDetector::Dump() {
  pthread_mutex_lock(&mutex_);
  // Copy data to headersCopy so that we can print after unlock the mutex_.
  memcpy(headersCopy, headersLock, sizeof(headersLock));
  pthread_mutex_unlock(&mutex_);

  error_log("\n==============================================================\n");
  error_log("+++ %s(%d) dumping leak information started. +++\n\n", getprogname(), getpid());

  if (g_discard > 0)
    error_log("{ Added: %zu, Discarded: %zu }\n", g_added, g_discard);

  error_log("{ [  file socket   mmap] }\n");
  error_log("{ [%6zu %6zu %6zu] }\n\n", g_addedType[LEAK_TYPE_FILE],
                g_addedType[LEAK_TYPE_SOCKET], g_addedType[LEAK_TYPE_MMAP]);

  int size = 0;
  char buf[30] = {'\0'};
  char file_path[FILE_PATH_MAX] = {'\0'};

  for (int i = 0; i < MAX_LEAK_RECORDS; i++) {
    if (headersCopy[i].type == LEAK_TYPE_NONE)
      continue;

    if (headersCopy[i].type == LEAK_TYPE_FILE) {
      int fd = (int)headersCopy[i].key;
      snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
      size = (int) readlink(buf, file_path, sizeof(file_path) - 1);
      if (size == -1)
        snprintf(file_path, sizeof(file_path), "[Unknown]");
      else
        file_path[size] = '\0';
      error_log("Leak Type: %s, count = %zu, fd = %d, path = %s\n",
                      g_type[headersCopy[i].type], headersCopy[i].num_same, fd, file_path);
    }
    else if (headersCopy[i].type == LEAK_TYPE_MMAP) {
      unsigned long addr = headersCopy[i].key;
      error_log("Leak Type: %s, count = %zu, addr = 0x%lx\n", g_type[headersCopy[i].type],
                                                    headersCopy[i].num_same, addr);
    }
    else { // LEAK_TYPE_SOCKET.
      int fd = (int)headersCopy[i].key;
      error_log("Leak Type: %s, count = %zu, fd = %d\n", g_type[headersCopy[i].type],
                   headersCopy[i].num_same, fd);
    }

    error_log_string(backtrace_string(headersCopy[i].frames, headersCopy[i].num_frames).c_str());
    error_log("\n");
  }
  error_log("+++ %s(%d) dumping leak information ended. +++\n", getprogname(), getpid());
  error_log("============================================================\n\n");
}
