# ----------
# GNUmakefile
#
#      Copyright (c) 2008 - 2015 Marc Munro
#      Fileset:	skit - a database schema management toolset
#      Author:  Marc Munro
#      License: GPL V3
#
# ----------
#




# Eliminate any existing default target.
.DEFAULT: 

# Ensure that phony targets of this directory's Makefile are always built.
.PHONY: html man doc_version
html man doc_version:
	cd ..; $(MAKE) $@

%::
	cd ..; $(MAKE) $@