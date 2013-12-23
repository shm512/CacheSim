/* -*- mode:c; coding: utf-8 -*- */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
strcmp_wrapper(const void *p1, const void *p2)
{
    char * const *s1 = p1;
    char * const *s2 = p2;
    return strcmp(*s1, *s2);
}

char *
make_param_name(
	char *buf,
	int size,
	const char *prefix,
	const char *name)
{
    if (!prefix) prefix = "";
    snprintf(buf, size, "%s%s", prefix, name);
    return buf;
}

void
error_undefined(const char *func, const char *param)
{
    fprintf(stderr, "Configuration parameter %s is undefined\n", param);
}

void
error_invalid(const char *func, const char *param)
{
    fprintf(stderr, "Configuration parameter %s value is invalid\n", param);
}

/*
 * Local variables:
 *  c-basic-offset: 4
 * End:
 */
