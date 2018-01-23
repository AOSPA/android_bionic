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

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/system_properties.h>

#include "TrackLeakDetector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enable this feature with the follow adb commands:
 * 1. 'setprop libc.debug.leakdetect <LEAK_TYPE>' or 'export LD_PRELOAD=libc_leak_detector.so'.
 *     LEAK_TYPE: 0=[Detect all kinds leak]; 1=[file]; 2=[socket]; 3=[mmap].
 * 2. 'setprop libc.debug.leakdetect.program <process name>', for Android process,
 *     set it to 'app_process'. If don't set this prop, all of the processes will be detected.
 *
 * Print the leak information to logcat log command
 * adb shell kill -28 <pid>
 */

// ------------------------------------------------------------------------
// Global Data
// ------------------------------------------------------------------------
#define DEBUG_SIGNAL        SIGWINCH
#define MAP_FAILED          ((void *)-1)

static bool g_init = false;
static bool g_dumping = false;  // Make sure only  one thread to dump at the same time.
static leak_type g_type = LEAK_TYPE_NONE;
static TrackLeakDetector* g_leakDetector = NULL;

// Method handler for wrapped calls
static int (* lib_open) (const char *, int, ...);
static int (* lib_dup) (int);
static int (* lib_dup2) (int, int);
static int (* lib_close) (int);
static int (* lib_socket) (int, int, int);
static int (* lib_accept4) (int, sockaddr*, socklen_t*, int);
static int (* lib_socketpair) (int, int, int, int[2]);
static void* (* lib_mmap) (void*, size_t, int, int, int, off_t);
static int (* lib_munmap) (void*, size_t);

// Filter these processes.
const char* filterProcess[]={"getprop", "setprop", "iptables", "ip6tables", "ip", "ln",
                             "logcat", "sdcard", "dex2oat", "chmod", "insmod", "start",
                             "stop", "chown", "toybox", "sh", "ps"};

static bool isFilterProcess() {
  const char* process_name = getprogname();
  int size = sizeof(filterProcess) / sizeof(filterProcess[0]);
  for (int i = 0; i < size; i++) {
    if (strstr(process_name, filterProcess[i]))
      return true;
  }

  return false;
}

static void* dumpLeakInfo(void *) {
  if(g_init) {
    g_leakDetector->Dump();
  }
  else {
    g_leakDetector->LoadMap();
    g_init = true;
  }

  g_dumping = false;
  return NULL;
}

static void signalHandler(int, siginfo_t*, void*) {
  if(g_dumping)
    return;
  g_dumping = true;
  error_log("receive signal");
  int err;
  pthread_t ntid;
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  err = pthread_create(&ntid, &attributes, dumpLeakInfo, (void*)"dumpLeakInfo Thread");
  if (err != 0) {
    error_log("Can't create dumpLeakInfo thread for %s(%d).\n", getprogname(), getpid());
  }
}

static void setSignalHandler(void) {
  struct sigaction enable_act;
  memset(&enable_act, 0, sizeof(enable_act));
  enable_act.sa_sigaction = signalHandler;
  enable_act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&enable_act.sa_mask);
  if (sigaction(DEBUG_SIGNAL, &enable_act, nullptr) != 0) {
    error_log("Unable to setup Leak Detector signal handler for %s(%d) -- %s\n", getprogname(), getpid(), strerror(errno));
  }
  else {
    error_log("Set Leak Detector signal handler for %s(%d) successfully.\n", getprogname(), getpid());
  }
}

void __attribute__(( constructor ))
libldor_init( void )
{
  if (isFilterProcess()) // Not set signal handle, no way to change the 'g_init' to 'true'.
    return;
  g_leakDetector = new TrackLeakDetector();

  setSignalHandler();

  char value[PROP_VALUE_MAX];
  if (__system_property_get("libc.debug.leakdetect", value) && atoi(value) >= LEAK_TYPE_NONE && atoi(value) < LEAK_TYPE_MAX) {
    g_type = (leak_type)atoi(value); // '0' is for all of the types.
  }
}

// Opens a file.
int open(const char* pathname, int flags, ...) {
  if (lib_open == NULL)
    lib_open = (int (*) (const char *, int, ...))dlsym(RTLD_NEXT, "open");

  mode_t mode = 0;

  if ((flags & O_CREAT) != 0) {
    va_list args;
    va_start(args, flags);
    mode = static_cast<mode_t>(va_arg(args, int));
    va_end(args);
  }

  int fd = (lib_open(pathname, flags,  mode));
  if (g_init && fd != -1 && (g_type == 0 || g_type == LEAK_TYPE_FILE))
    g_leakDetector->Add((unsigned long)fd, LEAK_TYPE_FILE);

  return fd;
}

// Dup a file.
int dup(int oldfd) {
  if (lib_dup == NULL)
    lib_dup = (int (*) (int))dlsym(RTLD_NEXT, "dup");

  int fd = (lib_dup(oldfd));
  if (g_init && fd != -1 && (g_type == 0 || g_type == LEAK_TYPE_FILE))
    g_leakDetector->Add((unsigned long)fd, LEAK_TYPE_FILE);

  return fd;
}

// Dup2 a file.
int dup2(int oldfd, int newfd) {
  if (lib_dup2 == NULL)
    lib_dup2 = (int (*) (int, int))dlsym(RTLD_NEXT, "dup2");

  int fd = (lib_dup2(oldfd, newfd));
  if (g_init && fd != -1 && (g_type == 0 || g_type == LEAK_TYPE_FILE))
    g_leakDetector->Add((unsigned long)fd, LEAK_TYPE_FILE);

  return fd;
}

// Closes a file or socket.
int close(int fd) {
  if (lib_close == NULL)
    lib_close = (int (*) (int))dlsym(RTLD_NEXT, "close");

  if (g_init)
    g_leakDetector->Remove((unsigned long)fd);

  return (lib_close(fd));
}

// Creates a new socket.
int socket(int domain, int type, int protocol) {
  if (lib_socket == NULL)
    lib_socket = (int (*) (int, int, int))dlsym(RTLD_NEXT, "socket");

  int fd = (lib_socket(domain, type, protocol));
  if (g_init && fd != -1 && (g_type == 0 || g_type == LEAK_TYPE_SOCKET))
    g_leakDetector->Add((unsigned long)fd, LEAK_TYPE_SOCKET);

  return fd;
}

// Accept a connection on a socket.
int __accept4(int sockfd, sockaddr* addr, socklen_t* addrlen, int flags) {
  if (lib_accept4 == NULL)
    lib_accept4 = (int (*) (int, sockaddr*, socklen_t*, int))dlsym(RTLD_NEXT, "__accept4");

  int fd = (lib_accept4(sockfd, addr, addrlen, flags));
  if (g_init && fd != -1 && (g_type == 0 || g_type == LEAK_TYPE_SOCKET))
    g_leakDetector->Add((unsigned long)fd, LEAK_TYPE_SOCKET);

  return fd;
}

// Creates an unnamed pair of connected sockets in the specified domain.
int socketpair(int domain, int type, int protocol, int sv[2]) {
  if (lib_socketpair == NULL)
    lib_socketpair = (int (*) (int, int, int, int[2]))dlsym(RTLD_NEXT, "socketpair");

  int ret = (lib_socketpair(domain, type, protocol, sv));
  if (g_init && ret != -1 && (g_type == 0 || g_type == LEAK_TYPE_SOCKET)) {
    g_leakDetector->Add((unsigned long)sv[0], LEAK_TYPE_SOCKET);
    g_leakDetector->Add((unsigned long)sv[1], LEAK_TYPE_SOCKET);
  }

  return ret;
}

// Creates a new mapping in the virtual address space of the calling process.
void* mmap(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
  if (lib_mmap == NULL)
    lib_mmap = (void* (*) (void*, size_t, int, int, int, off_t))dlsym(RTLD_NEXT, "mmap");

  void* address = (lib_mmap(addr, size, prot, flags, fd, offset));
  if (g_init && address != MAP_FAILED && (g_type == 0 || g_type == LEAK_TYPE_MMAP))
    g_leakDetector->Add((unsigned long)address, LEAK_TYPE_MMAP);

  return address;
}

// Deletes the mappings for the specified address range.
int munmap(void* addr, size_t size) {
  if (lib_munmap == NULL)
    lib_munmap = (int (*) (void*, size_t))dlsym(RTLD_NEXT, "munmap");

  int ret = (lib_munmap(addr, size));
  if (g_init && ret != -1)
    g_leakDetector->Remove((unsigned long)addr);

  return ret;
}

#ifdef __cplusplus
}
#endif
