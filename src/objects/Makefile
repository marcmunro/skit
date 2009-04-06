#      Makefile for skit src/objects files
#
#      Copyright (c) 2008 Marc Munro
#      Fileset:	skit - a database schema management toolset
#      Author:  Marc Munro
#      License: GPL V3
#
# $Id$
#
# Do not attempt to use this makefile directly: its targets are available
# and should be built from the main GNUmakefile in the parent directory.
# The GNUmakefile in this directory will build using the parent GNUmakefile
# so using make <target> in this directory will work as long as you don't
# try to specify this makefile.  It even works with emacs compile and 
# next-error functions though the number of makefiles involved seems a 
# little alarming at first.
# The whole strangeness of this makefile hierarchy derives from a, 
# possibly misguided, attempt to avoid recursive make (see the article
# "Recursive make considered harmful" for a rationale).

.PHONY: objects_clean objects_help

OBJECTS_DIR = src/objects
OBJECTS_SOURCES = $(wildcard $(OBJECTS_DIR)/*.c)
OBJECTS_HEADERS = $(wildcard $(OBJECTS_DIR)/*.h)

OBJECTS_OBJECTS = $(OBJECTS_SOURCES:%.c=%.o)
OBJECTS_DEPS = $(OBJECTS_SOURCES:.c=.d)
OBJECTS_GARBAGE = $(garbage:%=$(OBJECTS_DIR)/%)


# Include dependency files for each source file
include $(OBJECTS_DEPS) 


# Remove the shared library and all generated/intermediate files
src/objects_clean:
	@echo Cleaning src/objects...
	@rm -f $(OBJECTS_OBJECTS) $(OBJECTS_DEPS) $(OBJECTS_GARBAGE)

# Describe what this makefile can build
src/objects_help:
