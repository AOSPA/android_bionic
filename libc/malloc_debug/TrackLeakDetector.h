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

#ifndef LEAK_DETECTOR_UTILS_H
#define LEAK_DETECTOR_UTILS_H

#include <stdint.h>
#include <pthread.h>
#include <async_safe/log.h>

#define debug_log(format, ...)  \
		async_safe_format_log(ANDROID_LOG_DEBUG, "fd_leak_debug", (format), ##__VA_ARGS__ )
#define error_log(format, ...)  \
		async_safe_format_log(ANDROID_LOG_ERROR, "fd_leak_debug", (format), ##__VA_ARGS__ )
#define error_log_string(str)  \
		async_safe_write_log(ANDROID_LOG_ERROR, "fd_leak_debug", (str))
#define info_log(format, ...)  \
		async_safe_format_log(ANDROID_LOG_INFO, "fd_leak_debug", (format), ##__VA_ARGS__ )

#define DEFAULT_BACKTRACE_FRAMES  (32)
#define MAX_LEAK_RECORDS          (2048)
#define FILE_PATH_MAX             (512)

enum leak_type {
    LEAK_TYPE_NONE = 0,
    LEAK_TYPE_FILE,
    LEAK_TYPE_SOCKET,
    LEAK_TYPE_MMAP,
    LEAK_TYPE_MAX
};

// 4 + (4*8 + 32*8) = 37 * 8 = 292 bytes
// 2[headersLock & headersCopy] * 2048[MAX_LEAK_RECORDS] * 292 = 1168 K.
// So we need 1168 K memory to manager this struct.
// The 'java_stack' string is used to record the Java Stack,
// it cost 885 bytes memory for every Java Stack record averagely.
struct BacktraceHeader {
  leak_type type;
  unsigned long key;
  size_t num_same;
  char* java_stack;
  size_t num_frames;
  uintptr_t frames[DEFAULT_BACKTRACE_FRAMES];
} __attribute__((packed));

class TrackLeakDetector {
 public:
  TrackLeakDetector();
  virtual ~TrackLeakDetector() = default;

  void Add(unsigned long key, leak_type type);
  void Remove(unsigned long key);
  void Dump();
  void LoadART();
  void LoadMap();
  void PrintStack();

 private:
  int find(unsigned long key);
  int findBlank();

  pthread_mutex_t mutex_ = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
  BacktraceHeader headersLock[MAX_LEAK_RECORDS];
  BacktraceHeader headersCopy[MAX_LEAK_RECORDS];
};
#endif // LEAK_DETECTOR_UTILS_H
