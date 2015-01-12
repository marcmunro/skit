#      Makefile for skit documentation
#
#      Copyright (c) 2011 - 2015 Marc Munro
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

.PHONY: html man doc_version

DOC_DIR = doc
DOC_VERSION = $(DOC_DIR)/version.xml
DOC_SOURCES = $(DOC_DIR)/skit.xml $(DOC_VERSION) \
		$(wildcard $(DOC_DIR)/src/*.xml) 

HTMLDIR = $(DOC_DIR)/html
HTMLINDEX = $(HTMLDIR)/index.html

XHTML_XSL = $(DOC_DIR)/skit_customlayer.xsl
MAN_XSL = $(DOC_DIR)/stylesheet_man.xsl

XSLTPROCFLAGS = --xinclude 

# Create entities for the various bits of version information needed.
doc_version $(DOC_VERSION): configure
	{ \
	  echo "<!ENTITY version \"$(VERSION)\">"; \
	  echo "<!ENTITY majorversion \"$(MAJOR_VERSION)\">"; \
	} > $@

# Create html docs
html : $(HTMLINDEX)

$(HTMLINDEX): $(DOC_SOURCES) $(XHTML)
	$(XSLTPROC) $(XSLTPROCFLAGS) --output $(HTMLDIR)/ \
		$(XHTML_XSL) $(DOC_DIR)/skit.xml


man : $(DOC_DIR)/man_stamp

$(DOC_DIR)/man_stamp: $(DOC_DIR)/stylesheet_man.xsl $(DOC_SOURCES)
	$(XSLTPROC) $(XSLTPROCFLAGS) $(MAN_XSL) $(DOC_DIR)/skit.xml
	touch $@


# Remove all generated/intermediate filedoc_clean: 
	@rm -rf $(DOC_VERSION) $(HTMLDIR)

doc_distclean: 

# Describe what this makefile can build
doc_help: 
	@echo "html             - build skit html documentation"
	@echo "man              - build skit man pages"
