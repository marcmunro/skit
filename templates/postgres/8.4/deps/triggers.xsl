<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- triggers -->
  <xsl:template match="trigger">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="qname" select="skit:dbquote(@name)"/>
      <xsl:with-param name="table_qname" 
		      select="skit:dbquote(../@schema, ../@name)"/>
      <xsl:with-param name="do_schema_grant" select="'no'"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="trigger" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('table.', $parent_core)}"/>
    <dependency fqn="{concat('function.', 
		             ancestor::database/@name, '.', @function)}"/>
    <xsl:call-template name="SchemaGrant">
      <xsl:with-param name="owner" select="../@owner"/>
    </xsl:call-template>
    <xsl:call-template name="TableGrant">
      <xsl:with-param name="priv" select="'trigger'"/>
      <xsl:with-param name="owner" select="../@owner"/>
    </xsl:call-template>
  </xsl:template>
</xsl:stylesheet>

