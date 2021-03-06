<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">

  <!-- This template handles copy-only mode.  This is used when we
       discover that a document may not have its deps removed -->
  <xsl:template match="*" mode="copy">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="/">
    <xsl:choose>
      <xsl:when test="dump[@retain_deps='true']">
	<xsl:apply-templates select="." mode="copy"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:copy>
	  <xsl:copy-of select="@*"/>
	  <xsl:apply-templates/>
	</xsl:copy>
      </xsl:otherwise>	
    </xsl:choose>
  </xsl:template>

  <!-- Ignore text nodes when in ignore mode.  -->
  <xsl:template match="text()" mode="ignore"/>

  <!-- Eliminate dbincluster objects which are artificially created
    by add_deps.xsl -->
  <xsl:template match="dbincluster"/>

  <!-- Eliminate dependencies and their contents -->
  <xsl:template match="dbobject/dependencies">
    <xsl:apply-templates mode="ignore"/>
  </xsl:template>

  <!-- Eliminate dependencies and their contents -->
  <xsl:template match="dbobject/dependency-set">
    <xsl:apply-templates mode="ignore"/>
  </xsl:template>

  <!-- Eliminate dependencies and their contents -->
  <xsl:template match="dbobject/dependency">
    <xsl:apply-templates mode="ignore"/>
  </xsl:template>

  <!-- Eliminate context elements -->
  <xsl:template match="dbobject/context">
    <xsl:apply-templates mode="ignore"/>
  </xsl:template>

  <!-- Ignore extra-schema-privs elements -->
  <xsl:template match="dbobject/extra-schema-privs"/>

  <!-- Eliminate dependency convenience objects -->
  <xsl:template match="allroles"/>
  <xsl:template match="alltbs"/>

  <!-- Ignore dbobjects for privileges (these are duplicated by add_deps). -->
  <xsl:template match="dbobject[@type='privilege']">
    <xsl:apply-templates mode="ignore"/>
  </xsl:template>
  
  <!-- Ignore dbobjects but not their contents -->
  <xsl:template match="dbobject">
    <xsl:for-each select="*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>
    
  <!-- Lose the duplicate database definition within dbincluster -->
  <xsl:template match="dbobject[@type='dbincluster']/database"/>

  <!-- Ignore column dbobjects as the column info appears at both the
       table and column levels. -->
  <xsl:template match="dbobject/column">
    <xsl:for-each select="*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>
    

  <!-- Main template for database objects -->
  <xsl:template match="*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>

