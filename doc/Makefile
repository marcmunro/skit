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

.PHONY: html man doc_version doc_entities usage

DOC_DIR = doc
DOC_VERSION = $(DOC_DIR)/version.sgml
DOC_ENTITIES = $(DOC_DIR)/docbook_entities.sgml
DOC_SOURCES = $(DOC_DIR)/skit.xml $(DOC_VERSION) \
		$(wildcard $(DOC_DIR)/src/*.xml) \
		$(wildcard $(DOC_DIR)/src/*.sgml) 
MANPAGES    = skit.1

HTMLDIR = $(DOC_DIR)/html
HTMLINDEX = $(HTMLDIR)/index.html

XHTML_XSL = $(DOC_DIR)/skit_customlayer.xsl
MAN_XSL = $(DOC_DIR)/stylesheet_man.xsl

XSLTPROCFLAGS = --xinclude 

# Create entities for the various bits of version information needed.
doc_version: $(DOC_VERSION)

$(DOC_VERSION): configure
	{ \
	  echo "<!ENTITY version \"$(VERSION)\">"; \
	  echo "<!ENTITY majorversion \"$(MAJOR_VERSION)\">"; \
	} > $(DOC_VERSION)

doc_entities : $(DOC_ENTITIES)
$(DOC_ENTITIES):
	ln -s $(DOCBOOK_ENTITIES) $@


# Create html docs
html : $(HTMLINDEX)

$(HTMLINDEX): $(DOC_SOURCES) $(XHTML) $(DOC_VERSION) $(DOC_ENTITIES) \
	      $(XHTML_XSL)
	-$(XSLTPROC) $(XSLTPROCFLAGS) --output $(HTMLDIR)/ \
		$(XHTML_XSL)  $(DOC_DIR)/skit.xml
	@# There is a problem with our use of custom.css.source which
	@# causes errors above.  We deliberately ignore them and manually
	@# copy the css file to HTMLDIR
	cp -f $(DOC_DIR)/skit_docbook.css $(HTMLDIR)


man : $(MANPAGES) $(DOC_VERSION)

skit.1: $(DOC_DIR)/stylesheet_man.xsl $(DOC_SOURCES)
	$(XSLTPROC) $(XSLTPROCFLAGS) $(MAN_XSL) $(DOC_DIR)/skit.xml

usage: templates/usage_dbtype.txt man

templates/usage_dbtype.txt: $(DOC_DIR)/stylesheet_man.xsl $(DOC_SOURCES) 
	man -l skit.1 >skit_man.txt 2>/dev/null
	doc/extract_usage.sh skit_man.txt

# Remove all generated/intermediate filedoc_clean: 
doc_clean:
	@rm -rf $(DOC_VERSION) $(DOC_ENTITIES) $(HTMLDIR)

doc_distclean: 

# Describe what this makefile can build
doc_help: 
	@echo "html             - build skit html documentation"
	@echo "man              - build skit man pages"
