# ----------
# GNUmakefile
#
#      Copyright (c) 2009 - 2014 Marc Munro
#      Fileset:	skit - a database schema management toolset
#      Author:  Marc Munro
#      License: GPL V3
#
# ----------
#

MAKESHELL = /bin/bash
SHELL = /bin/bash

top_builddir = $(shell pwd)
garbage := \\\#*  .\\\#*  *~  *.orig  *.rej  core 

DEFAULT: help
.PHONEY: DEFAULT all clean list help extract

# Connection information for postgres connections
#DB_PORT = -p 5435
# Uncomment if not doing local connections
#DB_HOST = -h localhost
#DB_CLUSTER = 
#DB_VERSION = 
#DB_USER = 

DB_CONNECT = $(DB_PORT) $(DB_HOST) $(DB_CLUSTER) $(DB_VERSION) $(DB_USER)

DEBUG = 1

include $(top_builddir)/Makefile.global
SUBDIRS = src test dbscript regress doc test/data
include $(SUBDIRS:%=%/Makefile)

CCNAME := $(shell echo $(CC) | tr '[a-z]' '[A-Z]')


# compile and generate dependency info
%.o: %.c
	@echo "  $(CCNAME)" $*
	@$(CC) -c $(CFLAGS) $(CPPFLAGS) $*.c -o $*.o
	@$(CC) -M -MT $*.o $(CFLAGS) $(CPPFLAGS) $*.c > $*.d

## Build per-source object dependency files for inclusion
#%.d: %.c
#	@echo Recreating $@...
#	@$(CC) -M -MT $*.o $(CPPFLAGS) $< >$@

check: skit
	./skit -t list.xml x.xml

distclean: clean $(SUBDIRS:%=%_distclean)
	@rm -f ./configure config.log Makefile.global

clean: $(SUBDIRS:%=%_clean)
	@rm -f $(garbage) test.log


# Provide a list of the targets buildable by this makefile.
do_help: $(SUBDIRS:%=%_help)
	@echo "help             - list major makefile targets"
	@echo "check            - perform a simple interactive test run of skit"
	@echo "clean            - remove all intermediate, backup and target files"
	@echo "disctclean       - as clean but also remove all auto-generated files"

help:
	@echo "Major targets of this makefile:"
	@$(MAKE) --no-print-directory do_help | sort | sed -e 's/^/ /'
