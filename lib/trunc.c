#include <config.h>

#ifndef HAVE_TRUNC

#include <math.h>

double
trunc (double x)
{
		return round(x);	/* FIXME !! */
}

#endif /* !HAVE_TRUNC */
