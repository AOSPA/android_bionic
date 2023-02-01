/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <math.h>

#include "fpmath.h"

#include <fenv.h>

double fabs(double x) { return __builtin_fabs(x); }
float fabsf(float x) { return __builtin_fabsf(x); }
long double fabsl(long double x) { return __builtin_fabsl(x); }

#if defined (__AARCH32__) || defined (__aarch64__)
float ceilf(float x) { return __builtin_ceilf(x); }
double ceil(double x) { return __builtin_ceil(x); }
#endif

#if defined (__aarch64__)
double copysign(double x, double y) { return __builtin_copysign(x, y); }
float copysignf(float x, float y) { return __builtin_copysignf(x, y); }
#endif

#if defined (__AARCH32__) || defined (__aarch64__)
float floorf(float x) { return __builtin_floorf(x); }
double floor(double x) { return __builtin_floor(x); }
#endif

#if defined (__ARM_NEON__) || defined (__aarch64__)
float fmaf(float x, float y, float z) { return __builtin_fmaf(x, y, z); }
double fma(double x, double y, double z) { return __builtin_fma(x, y, z); }
#endif

#if defined (__AARCH32__) || defined (__aarch64__)
float fmaxf(float x, float y) { return __builtin_fmaxf(x, y); }
double fmax(double x, double y) { return __builtin_fmax(x, y); }

float fminf(float x, float y) { return __builtin_fminf(x, y); }
double fmin(double x, double y) { return __builtin_fmin(x, y); }

#endif

#if defined (__aarch64__)
long lrint(double x) { return __builtin_lrint(x); }
long lrintf(float x) { return __builtin_lrintf(x); }
long long llrint(double x) { return __builtin_llrint(x); }
long long llrintf(float x) { return __builtin_llrintf(x); }

long lround(double x) { return __builtin_lround(x); }
long lroundf(float x) { return __builtin_lroundf(x); }
long long llround(double x) { return __builtin_llround(x); }
long long llroundf(float x) { return __builtin_llroundf(x); }
#endif

#if defined (__AARCH32__) || defined (__aarch64__)
float nearbyintf(float x) { return __builtin_nearbyintf(x); }
double nearbyint(double x) { return __builtin_nearbyint(x); }

float rintf(float x) { return __builtin_rintf(x); }
double rint(double x) { return __builtin_rint(x); }

float roundf(float x) { return __builtin_roundf(x); }
double round(double x) { return __builtin_round(x); }
#endif

#if defined(__aarch64__)
float sqrtf(float x) { return __builtin_sqrtf(x); }
double sqrt(double x) { return __builtin_sqrt(x); }
#endif

#if defined(__aarch64__)
float truncf(float x) { return __builtin_truncf(x); }
double trunc(double x) { return __builtin_trunc(x); }
#endif

#if defined (__aarch64__)
// msun s_nearbyint.c defines all floating-point version, so we need to
// redefine the long double one here. For aarch64, clang/compiler-rt
// soft-float routines does not use single/double floating-point operation,
// so it should be safe to call rintl directly.
long double nearbyintl(long double x) {
    volatile long double ret;
    fenv_t env;
    fegetenv(&env);
    ret = rintl(x);
    fesetenv(&env);
    return (ret);
}
#endif
