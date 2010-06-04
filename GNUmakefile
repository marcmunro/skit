# ----------
# GNUmakefile
#
#      Copyright (c) 2009 Marc Munro
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
#DB_HOST = 
#DB_CLUSTER = test81
#DB_VERSION = 8.1
#DB_USER = 

DB_CONNECT = $(DB_PORT) $(DB_HOST) $(DB_CLUSTER) $(DB_VERSION) $(DB_USER)

#CODE_SOURCES = $(wildcard src/*.c)
#XML_SOURCES = $(wildcard templates/*/*/*.xml) $(wildcard templates/*/*/*/*.xml) 
#XSL_SOURCES = $(wildcard templates/*/*/*.xsl) $(wildcard templates/*/*/*/*.xsl) 
#SQL_SOURCES = $(wildcard templates/*/*/*.sql) $(wildcard templates/*/version.sql) 

#EXTRACT_SOURCES = $(wildcard templates/*/*/extract.xml) \
#		  $(wildcard templates/*/*/function_params.xsl) \
#		  $(wildcard templates/*/*/post_extract.xsl) \
#		  $(SQL_SOURCES) $(CODE_SOURCES)

#GENERATE_SOURCES = $(wildcard templates/*/*/generate.xml) \
#	 	   $(wildcard templates/*/*/add_deps.xsl) \
#		   $(wildcard templates/*/*/ddl.xsl)


#SOURCES = $(CODE_SOURCES) $(XML_SOURCES) $(XSL_SOURCES) $(SQL_SOURCES)
include $(top_builddir)/Makefile.global
SUBDIRS = src test dbscript regress
include $(SUBDIRS:%=%/Makefile)

scatter:	skit
	rm -rf regress/scratch/dbdump/
	./skit -t extract.xml --dbtype=postgres --connect "dbname = 'regressdb' port = 5432 host = /var/run/postgresql" --scatter --path regress/scratch/dbdump


gend:	skit
	./skit --generate --drop  regress/scratch/regressdb_dump1a.xml

genb:	skit
	./skit --generate --build  regress/scratch/regressdb_dump1a.xml

tryl:	skit
	./skit --generate --build y --list -g

# Build per-source object dependency files for inclusion
%.d: %.c
	@echo Recreating $@...
	@$(CC) -M -MT $*.o $(CPPFLAGS) $< >$@

check: skit
	./skit -t list.xml x.xml

clean: $(SUBDIRS:%=%_clean)
	@rm -f $(garbage) test.log


# Provide a list of the targets buildable by this makefile.
do_help: $(SUBDIRS:%=%_help)
	@echo "help         - list major makefile targets"
	@echo "check        - perform a simple interactive test run of skit"
	@echo "clean        - remove all intermediate, backup and target files"

help:
	@echo "Major targets of this makefile:"
	@$(MAKE) --no-print-directory do_help | sort | sed -e 's/^/ /'
