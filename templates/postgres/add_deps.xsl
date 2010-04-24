<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  version="1.0">

  <xsl:output method="xml" indent="yes"/>
  <xsl:strip-space elements="*"/>

  <!-- This stylesheet adds dependency definitions to dbobjects unless
       they appear to already exist. -->

  <xsl:template match="/*">
    <dump>
      <xsl:copy-of select="@*"/>
      <xsl:choose>
	<xsl:when test="//dbobject/dependencies">
	  <xsl:apply-templates mode="copy"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates/>
	  <xsl:apply-templates select="//database" mode="database"/>
	</xsl:otherwise>
      </xsl:choose>
    </dump>
  </xsl:template>

  <!-- This template handles copy-only mode.  This is used when we
       discover that a document already has dependencies defined -->
  <xsl:template match="*" mode="copy">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy"/>
    </xsl:copy>
  </xsl:template>

  <!-- Anything not matched explicitly will match this and be copied 
       This handles db objects, dependencies, etc -->
  <xsl:template match="*">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
        <xsl:with-param name="parent_core" select="$parent_core"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>

  <xsl:include href="skitfile:deps/cluster.xsl"/>

</xsl:stylesheet>

