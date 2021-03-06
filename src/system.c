/**
 * @file   system.c
 * \code
 *     Copyright (c) 2014 - 2015 Marc Munro
 *     Fileset:	skit - a database schema management toolset
 *     Author:  Marc Munro
 *     License: GPL V3
 *
 * \endcode
 * @brief  
 * Functions for interfacing with the OS which may be OS-specific.
 * Note that there are probably existing functions that should be moved
 * into here.
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include "skit.h"

String *
username(void)
{
    struct passwd *passwd;
    passwd = getpwuid(getuid()); 

    return stringNew(passwd->pw_name);    
}

String *
homedir(void)
{
    struct passwd *passwd;
    passwd = getpwuid(getuid()); 

    return stringNew(passwd->pw_dir);    
}
