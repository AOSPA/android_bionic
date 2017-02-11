
/* @(#)e_hypot.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* __ieee754_hypot(x,y)
 *
 * Method :                  
 *	If (assume round-to-nearest) z=x*x+y*y 
 *	has error less than sqrt(2)/2 ulp, than 
 *	sqrt(z) has error less than 1 ulp (exercise).
 *
 *	So, compute sqrt(x*x+y*y) with some care as 
 *	follows to get the error below 1 ulp:
 *
 *	Assume x>y>0;
 *	(if possible, set rounding to round-to-nearest)
 *	1. if x > 2y  use
 *		x1*x1+(y*y+(x2*(x+x1))) for x*x+y*y
 *	where x1 = x with lower 32 bits cleared, x2 = x-x1; else
 *	2. if x <= 2y use
 *		t1*y1+((x-y)*(x-y)+(t1*y2+t2*y))
 *	where t1 = 2x with lower 32 bits cleared, t2 = 2x-t1, 
 *	y1= y with lower 32 bits chopped, y2 = y-y1.
 *		
 *	NOTE: scaling may be necessary if some argument is too 
 *	      large or too tiny
 *
 * Special cases:
 *	hypot(x,y) is INF if x or y is +INF or -INF; else
 *	hypot(x,y) is NAN if x or y is NAN.
 *
 * Accuracy:
 * 	hypot(x,y) returns sqrt(x^2+y^2) with error less 
 * 	than 1 ulps (units in the last place) 
 */

#include <float.h>

#include "math.h"
#include "math_private.h"

typedef union{ uint64_t u; double d; } du;

double
__ieee754_hypot(double x, double y)
{
	static const double inf = __builtin_inf();
	du u[3];
	du *large = u;
	du *small = &u[1];

	u[0].d = fabs(x);
	u[1].d = fabs(y);

	// handle inf / NaN
	if( 0x7ff0000000000000ULL == ( u[0].u & 0x7ff0000000000000ULL)  ||
		0x7ff0000000000000ULL == ( u[1].u & 0x7ff0000000000000ULL)	)
	{
		if( 0x7ff0000000000000ULL == u[0].u || 0x7ff0000000000000ULL == u[1].u )
			return inf;

		return x + y;		// NaN
	}

	if( x == 0.0 || y == 0.0 )
		return fabs( x + y );

	//fix pointers to large and small if necessary
	if( u[0].d < u[1].d )
	{
		large = &u[1];
		small = &u[0];
	}

	//break values up into exponent and mantissa
	int64_t largeExp = large->u >> 52;
	int64_t smallExp = small->u >> 52;
	int64_t diff = largeExp - smallExp;
	if( diff >= 55L )
		return large->d + small->d;

	large->u &= 0x000fffffffffffffULL;
	small->u &= 0x000fffffffffffffULL;
	large->u |= 0x3ff0000000000000ULL;
	small->u |= 0x3ff0000000000000ULL;

	//fix up denormals
	if( 0 == smallExp )
	{
		if( 0 == largeExp )
		{
			large->d -= 1.0;
			largeExp = (large->u >> 52) - (1022);
			large->u &= 0x000fffffffffffffULL;
			large->u |= 0x3ff0000000000000ULL;
		}
		small->d -= 1.0;
		smallExp = (small->u >> 52) - (1022);
		small->u &= 0x000fffffffffffffULL;
		small->u |= 0x3ff0000000000000ULL;
	}

	u[2].u = (1023ULL - largeExp + smallExp) << 52;
	small->d *= u[2].d;

	double r = sqrt( large->d * large->d + small->d * small->d );

	if( largeExp < 0 )
	{
		largeExp += 1022;
		r *= 0x1.0p-1022;
	}

	u[2].u = largeExp << 52;
	r *= u[2].d;

	return r;
}

#if LDBL_MANT_DIG == 53
__weak_reference(hypot, hypotl);
#endif
