/**
 * @file   skit_utils.c
 * \code
 *      Copyright (c) 2008 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Provides basic utility functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "skit_lib.h"


void
skitFail(char *str)
{
    fprintf(stderr, "%s\n", str);
    raise(SIGTERM);
    printf("CONTINUING.....\n");
}

