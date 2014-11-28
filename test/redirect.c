/**
 * @file   redirect.c
 * \code
 *     Copyright (c) 2009 - 2014 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 * $Id$
 * \endcode
 * @brief  
 * Toold to redirect stdout and stderr.
 */


#include <check.h>
#include <stdio.h>
#include "../src/skit_lib.h"
#include "../src/exceptions.h"
#include "suites.h"


static char *stdout_path = NULL;
static char *stderr_path = NULL;
static FILE *stdout_file = NULL;
static FILE *stderr_file = NULL;

void
redirect_stdout(char *filename)
{
    stdout_path = newstr("test/log/%s.out", filename);
    stdout_file = freopen(stdout_path, "w", stdout);
}

void
redirect_stderr(char *filename)
{
    stderr_path = newstr("test/log/%s.err", filename);
    stderr_file = freopen(stderr_path, "w", stderr);
}

static void
undirect_stdout()
{
    if (stdout_file) {
	fclose(stdout_file);
	stdout_file = NULL;
    }
}

static void
undirect_stderr()
{
    if (stderr_file) {
	fclose(stderr_file);
	stderr_file = NULL;
    }
}


static char *
readfile(FILE *fp)
{
    int size = 10000;
    char *buf = malloc(size);
    char c = 0;
    int pos = 0;
    
    while (1) {
	for (; pos < size; pos++) {
	    c = fgetc(fp);
	    if (c == EOF) {
		buf[pos] = '\0';
		return memchunks_incr(buf);
	    }
	    buf[pos] = c;
	}
	size += 10000;
	buf = realloc(buf, size);
    }
}

char *
readfrom_stdout()
{
    FILE *fp;
    char *result;
    undirect_stdout();
    fp = fopen(stdout_path, "r");
    if (!fp) {
	RAISE(GENERAL_ERROR, 
	      newstr("UNABLE TO OPEN STDOUT CAPTURE FILE %s", stdout_path));
    }
    result = readfile(fp);
    fclose(fp);
    return result;
}

char *
readfrom_stderr()
{
    FILE *fp;
    char *result;
    undirect_stderr();
    fp = fopen(stderr_path, "r");
    result = readfile(fp);
    fclose(fp);
    return result;
}

void
end_redirects()
{
    undirect_stdout();
    undirect_stderr();
    if (stdout_path) {
	skfree(stdout_path);
	stdout_path = NULL;
    }
    if (stderr_path) {
	skfree(stderr_path);
	stderr_path = NULL;
    }
}
