#      Makefile for skit test files
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

.PHONY: tests_all test_clean

TEST_DIR = test
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_HEADERS = $(wildcard $(TEST_DIR)/*.h)

TEST_OBJECTS = $(TEST_SOURCES:%.c=%.o)
TEST_DEPS = $(TEST_SOURCES:.c=.d)
TEST_GARBAGE = $(garbage:%=$(TEST_DIR)/%)

#LIBCHECK = /usr/lib/libcheck.a
#LIBCHECK = /home/marc/dpkg/check-0.9.5/src/.libs/libcheck.a

MEMCHECK_OPTIONS = --leak-check=full --show-reachable=yes --trace-children=yes
MEMCHECK_SUPPRESSIONS = --suppressions=valgrind/glib_hash_suppressions \
		        --suppressions=valgrind/libxml2_suppressions \
		        --suppressions=valgrind/libpq_suppressions

GEN_SUPPRESSIONS = --gen-suppressions=yes
VALGRIND_OPTIONS = --tool=memcheck $(MEMCHECK_OPTIONS) $(MEMCHECK_SUPPRESSIONS)

# Include dependency files for each test file
include $(TEST_DEPS)


# Build skit unit test executable from sources.
skit_test:	$(TEST_OBJECTS) $(SRC_TESTED_OBJECTS)
	$(CC) $(LDFLAGS) $(TEST_OBJECTS) $(SRC_TESTED_OBJECTS) $(LIBCHECK) -o $@

test/log:
	@-mkdir test/log 2>/dev/null
	@touch test/log

unit: skit_test test/log
	@./skit_test $(TESTS)

# Build a list of all tests known to the test suite.
valgrind/alltests: skit_test
	@./skit_test -r >valgrind/alltests

grind: skit_test
	valgrind $(VALGRIND_OPTIONS) ./skit_test $(TESTS)

mygrind: skit_test valgrind/alltests
	valgrind/run_valgrind $(VALGRIND_OPTIONS) ./skit_test $(TESTS)

# Remove the shared library and all generated/intermediate files
test_clean:
	@echo Cleaning test...
	@rm -f $(SRC_OBJECTS) $(SRC_TEST_OBJECTS) $(SRC_DEPS) \
	       $(SRC_TESTDEPS) $(SRC_GARBAGE) skit skit_test \
	       valgrind/alltests valgrind/valgrind.log

tests_distclean: tests_clean

# Describe what this makefile can build
test_help:
	@echo "grind       - Run unit test executable for skit with valgrind"
	@echo "mygrind     - Run valgrind one test at a time, filtering output"
	@echo "unit        - Run unit test executable for skit"
	@echo "skit_test   - A Unit test executable for skit"
