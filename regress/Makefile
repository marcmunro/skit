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

.PHONY: regress regress_cluster cluster_is_up rdbstart wait_for_rdb \
	wait_while_proc rdbstop drop_regress_cluster 


REGRESS_DIR = regress
REGRESS_LOG = $(REGRESS_DIR)/regress.log
REGRESSDB_PORT = 54325

PGBIN = $(shell pg_config --bindir)
PSQL = $(PGBIN)/psql -p $(REGRESSDB_PORT) -v home=$(top_builddir) 
PG_DUMP = $(PGBIN)/pg_dump -p $(REGRESSDB_PORT)


# Check state of regression test database cluster.
cluster_is_up:
	@$(PGBIN)/psql -p $(REGRESSDB_PORT) -d postgres \
		-c "select 'o' || 'k'" 2>&1 | grep ok >/dev/null

# Wait 10 seconds for regression test database to be available
wait_for_rdb:
	@count=10; while [ $${count} -gt 0 ]; do \
		if $(MAKE) cluster_is_up >/dev/null 2>&1; then exit 0; fi; \
		count=`expr $${count} - 1`; sleep 1; done; exit 1

# Wait for a process $(PROC) to finish
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


# Regression test targets.
regress_cluster: 
	@if [ -d $(REGRESS_DIR)/REGRESSDB ]; then \
	    echo Error: database files already exist.; exit 1; fi
	@echo Creating new cluster for regression tests...
	@echo ...building cluster...
	@$(PGBIN)/initdb --pgdata $(REGRESS_DIR)/REGRESSDB 2>&1 >$(REGRESS_LOG)
	@echo ...Creating tablespace dirs...
	@mkdir -p $(REGRESS_DIR)/REGRESSDB/tbs/tbs2 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/tbs3 \
		  $(REGRESS_DIR)/REGRESSDB/tbs/tbs4
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

regression_test1: drop_regress_db
	@echo Running regression test 1...
	@mkdir regress/scratch 2>/dev/null || true
	@echo ...creating initial database definitions...
	@echo ==== BUILDING DB FROM regression1.sql ==== \
	    >$(REGRESS_LOG) 2>&1
	@export PGPORT=$(REGRESSDB_PORT); \
	    sh $(REGRESS_DIR)/regression1.sql 2>&1 | \
	    regress/errcheck.sh >>$(REGRESS_LOG)
	@echo ==== FINISHED BUILD FROM regression1.sql ==== \
	    >>$(REGRESS_LOG) 2>&1
	@echo ...taking initial pg_dump snapshot...
	@$(PG_DUMP) regressdb >$(REGRESS_DIR)/scratch/regressdb_test1a.dmp
	@echo ...running skit extract...
	@./skit --extract \
	    --connect "dbname='regressdb' port=$(REGRESSDB_PORT)" \
	    >$(REGRESS_DIR)/scratch/regressdb_dump1.xml
	@echo ...running skit generate...
	@echo ......drop...
	@./skit --generate --drop $(REGRESS_DIR)/scratch/regressdb_dump1.xml \
	    >$(REGRESS_DIR)/scratch/regressdb_drop1.sql
	@echo ......build...
	@./skit --generate --build $(REGRESS_DIR)/scratch/regressdb_dump1.xml \
	    >$(REGRESS_DIR)/scratch/regressdb_build1.sql
	@echo ......both...
	@./skit --generate --build --drop \
	    $(REGRESS_DIR)/scratch/regressdb_dump1.xml \
	    >$(REGRESS_DIR)/scratch/regressdb_both1.sql
	@echo ...editing drop script to allow dangerous drop statements...
	@sed -e  "/\"`whoami`\"/! s/^-- //" \
	    $(REGRESS_DIR)/scratch/regressdb_drop1.sql > \
	    $(REGRESS_DIR)/scratch/regressdb_drop1a.sql
	@echo ...executing drop script...
	@echo ==== RUNNING DROP SCRIPT regressdb_drop1a.sql ==== \
	    >>$(REGRESS_LOG) 2>&1
	@export PGPORT=$(REGRESSDB_PORT); \
	    sh $(REGRESS_DIR)/scratch/regressdb_drop1a.sql 2>&1 | \
	    regress/errcheck.sh >>$(REGRESS_LOG)
	@echo ...checking success of drop script...
	@pg_dumpall -p $(REGRESSDB_PORT) | grep ^CREATE | wc -l | \
            grep ^1$ >/dev/null
	@echo ==== FINISHED DROP SCRIPT regressdb_drop1a.sql ==== \
	    >>$(REGRESS_LOG) 2>&1
	@echo ...executing build script...
	@echo ==== RUNNING BUILD SCRIPT regressdb_build1.sql ==== \
	    >>$(REGRESS_LOG) 2>&1
	@echo ...editing build script to not create role `whoami`...
	@sed -e  "/create role \"`whoami`\"/ s/^/-- /" \
	    $(REGRESS_DIR)/scratch/regressdb_build1.sql > \
	    $(REGRESS_DIR)/scratch/regressdb_build1a.sql
	@export PGPORT=$(REGRESSDB_PORT); \
	    sh $(REGRESS_DIR)/scratch/regressdb_build1a.sql 2>&1 | \
	    regress/errcheck.sh >>$(REGRESS_LOG) 
	@echo ...checking success of build script...
	@echo "......running skit extract (again)..."
	@./skit --extract \
	    --connect "dbname='regressdb' port=$(REGRESSDB_PORT)" \
		>$(REGRESS_DIR)/scratch/regressdb_dump2.xml
	@regress/diffcheck.sh $(REGRESS_DIR)/scratch/regressdb_dump1.xml \
	    $(REGRESS_DIR)/scratch/regressdb_dump2.xml
	@echo ...taking final pg_dump snapshot...
	@$(PG_DUMP) regressdb >$(REGRESS_DIR)/scratch/regressdb_test1b.dmp
	@echo ...checking results of pg_dump snapshots...
	@regress/diffdump.sh $(REGRESS_DIR)/scratch/regressdb_test1a.dmp \
	    $(REGRESS_DIR)/scratch/regressdb_test1b.dmp
	@echo ==== FINISHED BUILD SCRIPT regressdb_build1.sql ==== \
	    >>$(REGRESS_LOG) 2>&1
	@echo ...regression test 1 complete


regress: skit regress_cluster regression_test1
	@$(MAKE) --no-print-directory drop_regress_cluster
	@echo Regression tests completed successfully

regress_clean: drop_regress_cluster
	@echo Cleaning regress...
	@rm -rf $(REGRESS_LOG)/scratch/*
	@rm -rf REGRESSDB $(REGRESS_LOG)/regressdb_*.sql \
	    $(REGRESS_LOG)/regressdb_*.xml

regress_distclean: regress_clean

# Describe what this makefile can build
regress_help:
	@echo "regress      - Run regression test suite"
	@echo "drop_regress_cluster - Drop regression test database cluster"
