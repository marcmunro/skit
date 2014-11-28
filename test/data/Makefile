#      Makefile for skit test data files
#
#      Copyright (c) 2014 Marc Munro
#      Fileset:	skit - a database schema management toolset
#      Author:  Marc Munro
#      License: GPL V3
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


.PHONY:	test_data test/data_clean test/data_help

TESTDATA_DIR = test/data
DATA_SOURCES = $(wildcard $(TESTDATA_DIR)/*.sql)
DATA_TARGETS = $(DATA_SOURCES:.sql=.xml)

DROP_TESTDATA_DB = sh $(TESTDATA_DIR)/depdiffs_drop.sh
CONNECT_STR = dbname='regressdb' port=${REGRESSDB_PORT} host=${REGRESSDB_HOST}


# Rebuild 
%.xml: %.sql
	$(MAKE) --silent ensure_db_available
	@echo Recreating $@...
	@export PGPORT=$(REGRESSDB_PORT); \
	  psql -l | grep test_data >/dev/null 2>&1 && \
			(echo dropping test_data database; \
	                 $(DROP_TESTDATA_DB)); \
	  echo "Building db using $<"; \
	  sh $<; \
	  echo "Extracting xml schema from db"; \
	  ./skit --extract --connect "$(CONNECT_STR)" >$@; \
	  $(DROP_TESTDATA_DB)

# We repurpose the regression test db cluster for building our xml files.
ensure_db_available:
	@$(MAKE) --silent cluster_is_up || \
	  (echo "Creating regression test db cluster to enable" \
	       "test data creation..."; \
	   $(MAKE) --silent regress_cluster)

test_data: skit $(DATA_TARGETS)

data_clean:
	@echo Purging test/data...
	@rm -f $(DATA_TARGETS)

test_data_clean:

# Describe what this makefile can build
test_data_help:
	@echo "test_data    - Create test xml files from sql files."
