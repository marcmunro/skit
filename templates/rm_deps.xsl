<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">

  <!-- If this document may not have its deps removed, just copy it
       verbatim --> 
  <xsl:template match="/*[@retain_deps='true']">
    <dump>
      <wibble/>
    </dump>
  </xsl:template>

  <!-- This template handles copy-only mode.  This is used when we
       discover that a document may not have its deps removed -->
  <xsl:template match="*" mode="copy">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy"/>
    </xsl:copy>
  </xsl:template>


  <xsl:template match="/">
    <xsl:for-each select="//*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>

  <!-- Eliminate dbincluster objects which are artificially created
    by add_deps.xsl -->
  <xsl:template match="dbincluster"/>

  <!-- Eliminate dependencies and their contents -->
  <xsl:template match="dbobject/dependencies"/>

  <!-- Eliminate context elements -->
  <xsl:template match="dbobject/context"/>

  <!-- Eliminate dependency convenience objects -->
  <xsl:template match="allroles"/>
  <xsl:template match="alltbs"/>

  <!-- Ignore dbobjects but not their contents -->
  <xsl:template match="dbobject">
    <xsl:for-each select="*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>
    
  <!-- Ignore column dbobjects as the column info appears at both the
       table and column levels. -->
  <xsl:template match="dbobject/column">
    <xsl:for-each select="*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>
    
  <!-- Main template for database objects -->
  <xsl:template match="*">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>

