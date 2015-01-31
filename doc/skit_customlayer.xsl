<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:import 
    href="/usr/share/xml/docbook/stylesheet/docbook-xsl/xhtml/chunk.xsl"/>

<xsl:param name="section.autolabel" select="1"/>
<xsl:param name="section.label.includes.component.label" select="1"/>
<xsl:param name="chunk.section.depth" select="1"/>

<xsl:param name="custom.css.source">/home/marc/proj/skit/doc/skit_docbook.css.xml</xsl:param>

<xsl:param name="generate.toc">
book      toc,title,figure,table,example,equation
part      toc,title
</xsl:param>

<xsl:param name="toc.section.depth">1</xsl:param>

</xsl:stylesheet>
