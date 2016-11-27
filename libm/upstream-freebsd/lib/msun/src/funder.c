/*
 *  funder.c - manually provoke SP exceptions for 
 *
 *  Copyright (C) 2009-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of the Optimized Routines project
 */

#include "math_private.h"
#include <fenv.h>

__inline float _flt_infnan2(float x, float y)
{
  return x+y;
}

__inline float _flt_infnan(float x)
{
  return x+x;
}

float _flt_underflow(void)
{
#ifdef CLANG_EXCEPTIONS
  feraiseexcept(FE_UNDERFLOW);
#endif
  return 0x1p-95F * 0x1p-95F;
}

float _flt_overflow(void)
{
#ifdef CLANG_EXCEPTIONS
  feraiseexcept(FE_OVERFLOW);
#endif
  return 0x1p+97F * 0x1p+97F;
}

float _flt_invalid(void)
{
#ifdef CLANG_EXCEPTIONS
  feraiseexcept(FE_INVALID);
#endif
  return 0.0f / 0.0f;
}

float _flt_divzero(void)
{
#ifdef CLANG_EXCEPTIONS
  feraiseexcept(FE_DIVBYZERO);
#endif
  return 1.0f / 0.0f;
}
