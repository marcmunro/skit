#      Makefile for skit test files
#
#      Copyright (c) 2009 Marc Munro
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

.PHONY: regress regress_db rdb_is_up rdbstart wait_for_rdb \
	wait_while_proc rdbstop drop_regress_db


REGRESS_DIR = regress
REGRESS_LOG = $(REGRESS_DIR)/regress_log
REGRESSDB_PORT = 54325

PGBIN = $(shell pg_config --bindir)
PSQL = $(PGBIN)/psql -p $(REGRESSDB_PORT) 


rdb_is_up:
	@$(PGBIN)/psql -p $(REGRESSDB_PORT) -d postgres \
		-c "select 'o' || 'k'" 2>&1 | grep ok >/dev/null

wait_for_rdb:
	@count=10; while [ $${count} -gt 0 ]; do \
		if $(MAKE) rdb_is_up >/dev/null 2>&1; then exit 0; fi; \
		count=`expr $${count} - 1`; sleep 1; done; exit 1

wait_while_proc:
	@count=10; while [ $${count} -gt 0 ]; do \
		if ps -fp $(PROC) >/dev/null 2>&1; then true; else exit 0; fi; \
		count=`expr $${count} - 1`; sleep 1; done; exit 1

rdbstart:
	@echo ...Starting postmaster...
	@$(PGBIN)/postmaster -D $(REGRESS_DIR)/REGRESSDB -p $(REGRESSDB_PORT)\
		>$(REGRESS_DIR)/postgresql.log 2>&1 &
	@echo ...Waiting for database startup...
	@$(MAKE) --no-print-directory wait_for_rdb || \
		(cat $(REGRESS_DIR)/postgresql.log; exit 1)
	@echo "Regression test database started"

rdbstop:
	@if $(MAKE) --no-print-directory rdb_is_up; then \
	  echo ...Stopping postmaster...; \
	  pid=`head -1 $(REGRESS_DIR)/REGRESSDB/postmaster.pid`; \
	  kill -SIGTERM $${pid}; \
	  $(MAKE) --no-print-directory wait_while_proc PROC=$${pid}; \
	  if ps -fp $${pid} >/dev/null; then true; else \
	    echo "Regression test database stopped"; exit 0; fi ; \
	  echo "Still running.  Killing process..."; \
	  echo kill -9 $${pid}; \
	  if ps -fp $${pid}; then exit 1; else \
	    echo "Regression test database stopped"; fi; \
        else echo "database is already down"; fi

# Regression test targets.
regress_db: 
	@if [ -d $(REGRESS_DIR)/REGRESSDB ]; then \
	    echo Error: database files already exist.; exit 1; fi
	@echo Creating new cluster for regression tests...
	@echo ...building database...
	$(PGBIN)/initdb --pgdata $(REGRESS_DIR)/REGRESSDB 2>&1 >$(REGRESS_LOG)
	@echo ...Creating tablespace dirs...
	@mkdir -p $(REGRESS_DIR)/REGRESSDB/tbs/tbs2 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/tbs3 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/tbs4
	@$(MAKE) --no-print-directory rdbstart

drop_regress_db: rdbstop
	@rm -rf $(REGRESS_DIR)/REGRESSDB
	@echo "Regression test database removed"

regression_test1: skit regress_db
	@echo Running regression test 1...
	@echo ...creating initial database definitions...
	$(PSQL) <$(REGRESS_DIR)/regression1.sql >>$(REGRESS_LOG) 2>&1


regress: skit regress_db regression_test1
	@echo Regression tests completed successfully

regress_clean: drop_regress_db
	@echo Cleaning regress...
	@rm -rf REGRESSDB $(REGRESS_LOG)

regress_distclean: regress_clean

# Describe what this makefile can build
regress_help:
	@echo "regress      - Run regression test suite"
