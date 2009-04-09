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

.PHONY: tests_all test_clean grind skgrind suppressions

TEST_DIR = test
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_HEADERS = $(wildcard $(TEST_DIR)/*.h)

TEST_OBJECTS = $(TEST_SOURCES:%.c=%.o)
TEST_DEPS = $(TEST_SOURCES:.c=.d)
TEST_GARBAGE = $(garbage:%=$(TEST_DIR)/%)

#LIBCHECK = /usr/lib/libcheck.a
#LIBCHECK = /home/marc/dpkg/check-0.9.5/src/.libs/libcheck.a

MEMCHECK_OPTIONS = --leak-check=full --show-reachable=yes --trace-children=yes
SUPPRESSION_FILES = $(wildcard valgrind/*_suppressions)
MEMCHECK_SUPPRESSIONS = $(SUPPRESSION_FILES:%=--suppressions=%)

VALGRIND_BASIC_OPTIONS = --tool=memcheck $(MEMCHECK_OPTIONS) 
GEN_SUPPRESSIONS = $(VALGRIND_BASIC_OPTIONS) --gen-suppressions=all
VALGRIND_OPTIONS = $(VALGRIND_BASIC_OPTIONS) $(MEMCHECK_SUPPRESSIONS)

# Include dependency files for each test file
include $(TEST_DEPS)


# Build skit unit test executable from sources.
skit_test:	$(TEST_OBJECTS) $(SRC_TESTED_OBJECTS)
	$(CC) $(LDFLAGS) $(TEST_OBJECTS) $(SRC_TESTED_OBJECTS) $(LIBCHECK) -o $@

test/log:
	@-mkdir test/log 2>/dev/null
	@touch test/log

# Major target for running all, or as specified in the TESTS variable, unit 
# tests.
unit: skit_test test/log
	@./skit_test $(TESTS)

# Build new suppression files for valgrind from the suppressions test suite
# within skit_test.
suppressions: skit_test valgrind/alltests
	@rm -f $(SUPPRESSION_FILES) 2>/dev/null
	@grep -i suppressions valgrind/alltests | \
	   while read suite test; do $(MAKE) suppress_one \
		SUITE=$${suite} THIS_TEST=$${test}; done
	@touch valgrind/suppressions

# Command to dynamically create suppressions arguments for valgrind while
# we are creating new suppression files.  The point of this is to allow
# suppressions from previous suppresions file to be used while creating the
# next suppression file.  This prevents duplicates from appearing in the
# suppressions files.
EXISTING_SUPPRESSIONS = ls valgrind/?*suppressions 2>/dev/null | \
	    xargs -I Z echo --suppressions=Z

SUPP_FLAG = -$(shell if test -d testdb; then echo S; else echo s; fi)

# Build a single suppression file.  This is called recursively from the
# suppressions target.  As suppression files rarely require updating, this
# is considered safe enough.  
suppress_one:
	@echo Creating valgrind/$(THIS_TEST)...
	@suppression_files=`$(EXISTING_SUPPRESSIONS)` ; \
	valgrind $(GEN_SUPPRESSIONS) $${suppression_files} \
	    ./skit_test $(SUPP_FLAG) $(SUITE) $(THIS_TEST) 2>&1 | \
		valgrind/make_suppression >valgrind/tmpout 
	@mv valgrind/tmpout valgrind/$(THIS_TEST)

# Target that simply ensures that suppression files have been created
# It is not generally necessary to keep them up to date since they will
# change very infrequently.  If there is any doubt about the accuracy
# of the suppression files, simply run make suppressions explicitly.
valgrind/suppressions: 
	$(MAKE) suppressions

# Build a list of all tests known to the test suite.
valgrind/alltests: skit_test
	@./skit_test -r $(SUPP_FLAG) >valgrind/alltests

grind: skit_test valgrind/suppressions
	valgrind $(VALGRIND_OPTIONS) ./skit_test $(TESTS)

skgrind: skit_test valgrind/alltests valgrind/suppressions
	valgrind/run_valgrind $(VALGRIND_OPTIONS) ./skit_test $(TESTS)

# Remove the shared library and all generated/intermediate files
test_clean:
	@echo Cleaning test...
	@rm -f $(TEST_OBJECTS) $(TEST_DEPS) \
	       $(TEST_GARBAGE) skit skit_test \
	       valgrind/alltests valgrind/valgrind.log \
	       valgrind/suppressions $(SUPPRESSION_FILES)

tests_distclean: tests_clean


# Describe what this makefile can build
test_help:
	@echo "grind        - Run unit test executable for skit with valgrind"
	@echo "skgrind      - Run valgrind one test at a time, filtering output"
	@echo "suppressions - Run valgrind to generate suppression files"
	@echo "unit         - Run skit unit tests (define TESTS for specific tests)"
	@echo "skit_test    - A Unit test executable for skit"
