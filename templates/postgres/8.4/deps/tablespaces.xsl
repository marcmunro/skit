<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Tablespaces -->
  <xsl:template match="tablespace">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="parent" select="'cluster'"/>
      <xsl:with-param name="qname" select="skit:dbquote(@name)"/>
      <xsl:with-param name="do_schema_grant" select="'no'"/>
      <xsl:with-param name="do_context" select="'no'"/>

    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="tablespace" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <dependency fqn="cluster"/>
  </xsl:template>
</xsl:stylesheet>
