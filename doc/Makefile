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
DOC_VERSION = $(DOC_DIR)/version.sgml
DOC_SOURCES = $(DOC_DIR)/skit.sgml $(DOC_VERSION) \
		$(wildcard $(DOC_DIR)/src/*.sgml) 
HTMLDIR = $(DOC_DIR)/html
HTMLINDEX = $(HTMLDIR)/index.html

# TODO: Get configure to find this
XHTML_CHUNKS = file:///usr/share/xml/docbook/stylesheet/nwalsh/xhtml/chunk.xsl
XHTML = /usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/chunk.xsl

XSLTPROCFLAGS = --xinclude --stringparam  section.autolabel 1 \
		--stringparam chunk.section.depth 2 \

# Create entities for the various bits of version information needed.
doc_version: $(DOC_VERSION)
$(DOC_VERSION): configure
	{ \
	  echo "<!ENTITY version \"$(VERSION)\">"; \
	  echo "<!ENTITY majorversion \"$(MAJOR_VERSION)\">"; \
	  echo "<!ENTITY xmlversion \"$(XML_VERSION)\">"; \
	} > $@

# Create an xml version of the docs from the sgml sources.
$(DOC_DIR)/skit.xml: $(DOC_DIR)/skit.sgml $(DOC_SOURCES)
	osx -D. -x lower $< >skit.xmltmp
	perl -p -e 's/\[(amp|copy|egrave|gt|lt|mdash|nbsp|ouml|pi|quot|uuml) *\]/\&\1;/g;' \
	           -e '$$_ .= qq{<!DOCTYPE book PUBLIC "-//OASIS//DTD DocBook XML V4.2//EN" "http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd">\n} if $$. == 1;' \
	  <skit.xmltmp > $@
	rm skit.xmltmp

# Create html docs.
html:	$(HTMLINDEX) 
	@echo SOURCES: $(DOC_SOURCES) 
	@echo VERSION: $(DOC_VERSION) 

# Create html docs
$(HTMLINDEX): $(DOC_DIR)/skit.xml
	$(XSLTPROC) $(XSLTPROCFLAGS) --output $(HTMLDIR)/ $(XHTML) $(DOC_DIR)/skit.xml
#
#
#GENERATED_SGML = bookindex.sgml version.sgml \
#	features-supported.sgml features-unsupported.sgml errcodes-table.sgml
#
#ALLSGML := $(wildcard $(srcdir)/*.sgml $(srcdir)/ref/*.sgml) $(GENERATED_SGML)
#
#man:	$(DOC_DIR)/man_stamp
#
#$(DOC_DIR)/man_stamp: $(DOC_DIR)/stylesheet_man.xsl $(DOC_DIR)/skit.xml
#	$(XSLTPROC) $(XSLTPROCFLAGS) $(DOC_DIR)/stylesheet_man.xsl \
#	    $(DOC_DIR)/skit.xml
#	touch $@
#

# Remove all generated/intermediate files
doc_clean: 
	@rm -rf $(DOC_VERSION) $(HTMLDIR)

doc_distclean: 

# Describe what this makefile can build
doc_help: 
	@echo "html             - build skit html documentation"
	@echo "man              - build skit man pages"
