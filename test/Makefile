#      Makefile for skit test files
#
#      Copyright (c) 2009 - 2011 Marc Munro
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
TEST_DEPS = $(TEST_OBJECTS:.o=.d)
TEST_GARBAGE = $(garbage:%=$(TEST_DIR)/%)

# Include dependency files for each test file
-include $(TEST_DEPS) $(TEST_DIR)/alltests.dep

LIBCHECK = /usr/lib/libcheck.a

MEMCHECK_OPTIONS = --leak-check=full --show-reachable=yes --trace-children=yes 
SUPPRESSION_FILES = $(wildcard valgrind/*_suppressions)
MEMCHECK_SUPPRESSIONS = $(SUPPRESSION_FILES:%=--suppressions=%)

VALGRIND_BASIC_OPTIONS = --tool=memcheck $(MEMCHECK_OPTIONS) \
			 --num-callers=20 --track-origins=yes -v
GEN_SUPPRESSIONS = $(VALGRIND_BASIC_OPTIONS) --gen-suppressions=all
VALGRIND_OPTIONS = $(VALGRIND_BASIC_OPTIONS) $(MEMCHECK_SUPPRESSIONS)



# Build skit unit test executable from sources.
# Also create the alltests.dep Makefile.  This identifies all of the
# components of the grind and skgrind targets, which are used to run
# individual grind and skgrind tests.
skit_test:	$(TEST_OBJECTS) $(SKIT_OBJECTS_FOR_TEST)
	@echo "  LINK" $@
	@$(CC) $(LDFLAGS) $(TEST_OBJECTS) $(SKIT_OBJECTS_FOR_TEST) \
		$(LIBCHECK) -o $@
	@echo "  CREATE" $(TEST_DIR)/alltests.dep USING $@ 
	@./skit_test -r $(SUPP_FLAG) 2>/dev/null | \
	    while read suite test; do \
	        echo grind__$${suite}: $${suite}__$${test}.grind; \
	        echo _grind: grind__$${suite}; \
	        echo skgrind__$${suite}: $${suite}__$${test}.skgrind; \
	        echo _skgrind: skgrind__$${suite}; \
	    done > $(TEST_DIR)/alltests.dep

test/log:
	@-mkdir test/log 2>/dev/null
	@touch test/log

# Major target for running all, or as specified in the TESTS variable, unit 
# tests.
unit: skit_test test/log test_data
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
# is considered safe enough.  The variable THIS_TEST must be supplied to
# this target to define what it will actually create
suppress_one:
	@echo Creating valgrind/$(THIS_TEST)...
	suppression_files=`$(EXISTING_SUPPRESSIONS)` ; \
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
	./skit_test -r $(SUPP_FLAG) >valgrind/alltests

# Convert from a target name to the strings to be used for the test
# parameters to skit_test.
TARG_TO_TEST = echo $@ | sed -e 's/\(.*\)__\(.*\)\..*/\1 \2/'

# Given a value for the TESTS variable, convert this into the
# appropriate target name.
TEST_TO_TARG = echo "$(TESTS)" | (read suite test; \
		if [ "x$${suite}" = "x" ]; then echo _$@; \
		elif [ "x$${test}" = "x" ]; then echo $@__$${suite}; \
		else echo $${suite}__$${test}.$@; fi)

skgrind grind: skit_test valgrind/suppressions
	@rm -f valgrind.log
	@$(MAKE) `$(TEST_TO_TARG)`


# Remove the shared library and all generated/intermediate files
test_clean: test_data_clean
	@echo Cleaning test...
	@rm -f $(TEST_OBJECTS) $(TEST_DEPS) \
	       $(TEST_GARBAGE) skit skit_test \
	       valgrind/alltests valgrind/valgrind.log \
	       valgrind/suppressions $(SUPPRESSION_FILES) \
		$(TEST_DIR)/alltests.dep

test_distclean: data_clean


# How to build a .grind target.  These targets are specified in
# tests/alltests.dep and are automatically generated below
%.grind:
	@valgrind $(VALGRIND_OPTIONS) ./skit_test -n `$(TARG_TO_TEST)`

# How to build a .skgrind target.  These targets are specified in
# tests/alltests.dep and are automatically generated below
%.skgrind:
	@valgrind/run_valgrind $(VALGRIND_OPTIONS) \
		./skit_test -n `$(TARG_TO_TEST)` | grep .



# Describe what this makefile can build
test_help:
	@echo "grind        - Run unit test executable for skit with valgrind"
	@echo "skgrind      - Run valgrind one test at a time, filtering output"
	@echo "suppressions - Run valgrind to generate suppression files"
	@echo "unit         - Run skit unit tests (define TESTS for specific tests)"
	@echo "data_clean   - Remove auto-generated test data"
	@echo "skit_test    - A Unit test executable for skit"
