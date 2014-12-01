#      Makefile for skit test files
#
#      Copyright (c) 2009 - 2014 Marc Munro
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

.PHONY: regress regress_cluster cluster_is_up rdbstart wait_for_rdb \
	wait_while_proc rdbstop drop_regress_cluster regression_test1 \
	regression_test2 regression_test3 prep regress_rmlog


REGRESS_DIR = regress
REGRESS_RUN = $(REGRESS_DIR)/regress_run.sh
REGRESS_LOG = $(REGRESS_DIR)/regress.log
REGRESSDB_PORT = 54325
# This should probably be discovered rather thean hard-coded;
# suggestions are welcomed to marc@bloodnok.com
REGRESSDB_HOST = /var/run/postgresql

PGBIN = $(shell $(REGRESS_RUN) pgbin)
PGLIB = $(shell $(REGRESS_RUN) pglib)
PSQL = psql -p $(REGRESSDB_PORT) -h $(REGRESSDB_HOST) -v home=$(top_builddir) 
PG_DUMP = pg_dump -p $(REGRESSDB_PORT)


# Check state of regression test database cluster.
cluster_is_up:
	@$(PSQL) -d postgres -c "select 'o' || 'k'" 2>&1 | grep ok >/dev/null

# Wait up to 10 seconds for regression test database to be available
wait_for_rdb:
	@count=10; while [ $${count} -gt 0 ]; do \
		if $(MAKE) cluster_is_up >/dev/null 2>&1; then exit 0; fi; \
		count=`expr $${count} - 1`; sleep 1; done; exit 1

# Wait for a process, $(PROC), to finish
wait_while_proc:
	@count=10; while [ $${count} -gt 0 ]; do \
		if ps -fp $(PROC) >/dev/null 2>&1; then true; else exit 0; fi; \
		count=`expr $${count} - 1`; sleep 1; done; exit 1

# Start up the regression test database
rdbstart:
	@echo ...Starting postmaster...
	@$(PGBIN)/postmaster -D $(REGRESS_DIR)/REGRESSDB -p $(REGRESSDB_PORT)\
		>$(REGRESS_DIR)/postgresql.log 2>&1 &
	@echo ...Waiting for database startup...
	@$(MAKE) --no-print-directory wait_for_rdb || \
		(cat $(REGRESS_DIR)/postgresql.log; exit 1)
	@echo "Regression test database started"

# Stop the regression test database
rdbstop:
	@if $(MAKE) --no-print-directory cluster_is_up; then \
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

check_pglibs:
	@if [ ! -f $(PGLIB)/seg.so ]; then\
	    echo "ERROR: seg library (from contrib) is not installed!"; \
	    exit 2; \
	 fi


# Regression test targets.
regress_cluster: check_pglibs
	@if [ -d $(REGRESS_DIR)/REGRESSDB ]; then \
	    $(MAKE) --silent drop_regress_cluster; fi
	@echo Creating new cluster for regression tests...
	@echo ...building cluster...
	@$(PGBIN)/initdb --pgdata $(REGRESS_DIR)/REGRESSDB 2>&1 >$(REGRESS_LOG)
	@echo ...Copying config file templates...
	@#cp $(REGRESS_DIR)/pgtemplates/* $(REGRESS_DIR)/REGRESSDB
	@#echo ...Creating tablespace dirs...
	@mkdir -p $(REGRESS_DIR)/REGRESSDB/tbs/tbs2 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/tbs3 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/tbs4 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/TBS4
	@$(MAKE) --no-print-directory rdbstart


drop_regress_cluster: rdbstop
	@rm -rf $(REGRESS_DIR)/REGRESSDB
	@echo "Regression test database removed"

drop_regress_db: 
	@echo Dropping any existing regression test database...
	@$(PSQL) -d postgres -c "drop database regressdb" \
		>/dev/null 2>&1 || true
	@$(PSQL) -d postgres -f $(REGRESS_DIR)/drop_cluster_objects.sql \
		>/dev/null 2>&1 || true

# Show the environment used for postgres by the regression tests.
regression_env: 
	@echo "export REGRESSDB_PORT=$(REGRESSDB_PORT); \
	 export REGRESSDB_HOST=$(REGRESSDB_HOST); \
	 export REGRESS_LOG=$(REGRESS_LOG); \
	 export REGRESS_DIR=$(REGRESS_DIR); \
	 export PGPORT=$(REGRESSDB_PORT); \
	 export PG_DUMP=\"$(PG_DUMP)\"; "

regression_test_precond: regress_rmlog
	@if [ ! -d $(REGRESS_DIR)/REGRESSDB ]; then \
	    $(MAKE) --no-print-directory regress_cluster; \
	fi

regression_test1: skit regression_test_precond
	@$(MAKE) --no-print-directory drop_regress_db
	@export REGRESSDB_PORT=$(REGRESSDB_PORT); \
	 export REGRESSDB_HOST=$(REGRESSDB_HOST); \
	 export REGRESS_LOG=$(REGRESS_LOG); \
	 export REGRESS_DIR=$(REGRESS_DIR); \
	 export PGPORT=$(REGRESSDB_PORT); \
	 export PG_DUMP="$(PG_DUMP)"; \
	 $(REGRESS_RUN) 1

regression_test2: skit regression_test_precond
	@$(MAKE) --no-print-directory drop_regress_db
	@export REGRESSDB_PORT=$(REGRESSDB_PORT); \
	 export REGRESSDB_HOST=$(REGRESSDB_HOST); \
	 export REGRESS_LOG=$(REGRESS_LOG); \
	 export REGRESS_DIR=$(REGRESS_DIR); \
	 export PGPORT=$(REGRESSDB_PORT); \
	 export PG_DUMP="$(PG_DUMP)"; \
	 $(REGRESS_RUN) 2

regression_test3: skit regression_test_precond
	@$(MAKE) --no-print-directory drop_regress_db
	@export REGRESSDB_PORT=$(REGRESSDB_PORT); \
	 export REGRESSDB_HOST=$(REGRESSDB_HOST); \
	 export REGRESS_LOG=$(REGRESS_LOG); \
	 export REGRESS_DIR=$(REGRESS_DIR); \
	 export PGPORT=$(REGRESSDB_PORT); \
	 export PG_DUMP="$(PG_DUMP)"; \
	 $(REGRESS_RUN) 3

regression_test4: skit regression_test_precond
	@$(MAKE) --no-print-directory drop_regress_db
	@export REGRESSDB_PORT=$(REGRESSDB_PORT); \
	 export REGRESSDB_HOST=$(REGRESSDB_HOST); \
	 export REGRESS_LOG=$(REGRESS_LOG); \
	 export REGRESS_DIR=$(REGRESS_DIR); \
	 export PGPORT=$(REGRESSDB_PORT); \
	 export PG_DUMP="$(PG_DUMP)"; \
	 $(REGRESS_RUN) 4

# This integrates with unit testing so that unit tests can be run
# from the regression test data while development and testing is
# under way.
prep: regression_test_precond drop_regress_db
	@export REGRESSDB_PORT=$(REGRESSDB_PORT); \
	 export REGRESSDB_HOST=$(REGRESSDB_HOST); \
	 export REGRESS_LOG=$(REGRESS_LOG); \
	 export REGRESS_DIR=$(REGRESS_DIR); \
	 export PGPORT=$(REGRESSDB_PORT); \
	 export PG_DUMP="$(PG_DUMP)"; \
	 $(REGRESS_RUN) prep

regress: skit regress_cluster regression_test1 regression_test2 \
		regression_test3 regression_test4
	@$(MAKE) --no-print-directory drop_regress_cluster
	@echo Regression tests completed successfully

regress_rmlog:
	@rm -f $(REGRESS_DIR)/*.log

regress_clean: drop_regress_cluster
	@echo Cleaning regress...
	@rm -rf $(REGRESS_DIR)/scratch
	@rm -f $(REGRESS_DIR)/*.log
	@rm -f $(REGRESS_DIR)/*~

regress_distclean: regress_clean

# Describe what this makefile can build
regress_help:
	@echo "regress          - Run full regression test suite"
	@echo "regress_cluster  - Create regression test database cluster"
	@echo "regression_env   - Show env variables used for regression"
	@echo "regression_test1 - Regression test 1 (extract/generate)"
	@echo "regression_test2 - Regression test 2 (scatter/gather)"
	@echo "regression_test3 - Regression test 3 (diffs)"
	@echo "regression_test4 - Regression test 4 (diffs)"
	@echo -e "drop_regress_cluster\n                 - Drop regression test database cluster"
