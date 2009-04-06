#      Makefile for skit test database
#
#      Copyright (c) 2009 Marc Munro
#      Fileset:	skit - a database schema management toolset
#      Author:  Marc Munro
#      License: GPL V3
#
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
.PHONY: dbstart dbstop droptestdb

INITDB=$(shell grep INITDB= dbscript/mkdb.sh | cut -d= -f2)
PSQL=$(shell grep INITDB= dbscript/testdb.sh | cut -d= -f2)
PORT=$(shell grep ' *port *=' dbscript/postgresql.conf | cut -d= -f2)

testdb:
	@(groups | grep postgres >/dev/null 2>&1) || \
		(echo "You must belong to the postgres group to create "\
		     "a test database"; exit 1)
	@test -x $(INITDB) || \
		(echo "INITDB is not valid as defined in mkdb.sh"; exit 1)
	@test -x $(PSQL) || \
		(echo "PSQL is not valid as defined in testdb.sh"; exit 1)
	@echo "Creating test database in `pwd`/testdb..."
	@dbscript/mkdb.sh | grep pg_ctl >testdb.start

dbstart: testdb
	@echo Starting test database instance...
	@sh testdb.start
	@echo Access the test database server using psql -p $(PORT)
dbstop:
	@echo Stopping test database instance...
	@(sed -e 's/start/stop/' testdb.start | sh) || true

droptestdb: dbstop
	@echo "Dropping test database instance..."
	@rm -rf testdb
	@echo "Done"

dbscript_clean:

dbscript_help:
	@echo "testdb       - Create a local test database"
	@echo "dbstart      - Start the local test database"
	@echo "dbstop       - Stop the local test database"
	@echo "droptestdb   - Drop the local test database"