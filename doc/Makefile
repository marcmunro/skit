#      Makefile for skit documentation
#
#      Copyright (c) 2011 Marc Munro
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
# The whole strangeness of this makefile hierarchy derives from an 
# attempt, possibly misguided, to avoid recursive make (see the article
# "Recursive make considered harmful" for a rationale).

.PHONY: html

DOC_DIR = doc
DOC_VERSION = $(DOC_DIR)/version.xml
DOC_SOURCES = $(wildcard $(DOC_DIR)/*.xml) $(DOC_VERSION)
HTMLDIR = $(DOC_DIR)/html
HTMLINDEX = $(HTMLDIR)/index.html

# TODO: Get configure to find this
XHTML_CHUNKS = file:///usr/share/xml/docbook/stylesheet/nwalsh/xhtml/chunk.xsl

XHTML = file:///usr/share/xml/docbook/stylesheet/docbook-xsl-ns/xhtml/chunk.xsl

XSLTPROCFLAGS = --xinclude --stringparam  section.autolabel 1 \
		--stringparam chunk.section.depth 2 \


$(DOC_VERSION): configure
	{ \
	  echo "<!ENTITY version \"$(VERSION)\">"; \
	} > $@


html:	$(HTMLINDEX)

$(HTMLINDEX): $(DOC_SOURCES) 
	$(XSLTPROC) $(XSLTPROCFLAGS) --output $(HTMLDIR)/ $(XHTML) $<

man:	
	@echo "Make the man pages (TODO)"

# Remove the shared library and all generated/intermediate files
doc_clean: 
	@echo "Cleaning doc (TODO)"

doc_distclean: 

# Describe what this makefile can build
doc_help: 
	@echo "html         - build skit html documentation"
	@echo "man          - build skit man pages"
