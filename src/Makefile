#      Makefile for skit src files
#
#      Copyright (c) 2008 - 2014 Marc Munro
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
# The whole strangeness of this makefile hierarchy derives from an 
# attempt, possibly misguided, to avoid recursive make (see the article
# "Recursive make considered harmful" for a rationale).

.PHONY: src_clean src_help

SRC_DIR = src

# Include makefiles from subdirs
SRC_SUBDIRS = $(SRC_DIR)/objects
include $(SRC_SUBDIRS:%=%/Makefile)

SRC_SOURCES = $(wildcard $(SRC_DIR)/*.c)
SRC_OBJECTS = $(SRC_SOURCES:%.c=%.o)
SRC_GARBAGE = $(garbage:%=$(SRC_DIR)/%)

SKIT_OBJECTS := $(SKIT_OBJECTS) $(SRC_OBJECTS)

# All skit objects except the one containing main() need to be part
# of the skit_test executable.  SKIT_OBJECTS_FOR_TEST defines that set
# of objects.
SKIT_OBJECTS_FOR_TEST = $(filter-out %/skit.o, $(SKIT_OBJECTS)) 

SRC_DEPS = $(SRC_OBJECTS:.o=.d)

# Include dependency files for each object
-include $(SRC_DEPS) 

# Build skit from sources.
skit:	$(SKIT_OBJECTS)
	@echo "  LINK" $@
	@$(CC) $(LDFLAGS) $(SKIT_OBJECTS) -o $@


# Remove the shared library and all generated/intermediate files
src_clean: $(SRC_SUBDIRS:%=%_clean)
	@echo Cleaning src...
	@rm -f $(SRC_OBJECTS) $(SRC_DEPS) $(SRC_GARBAGE) skit

# Describe what this makefile can build
src_help: $(SRC_SUBDIRS:%=%_help)
	@echo "skit             - build the skit executable"



