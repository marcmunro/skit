<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Indices -->
  <xsl:template match="index">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="qname" 
		      select="skit:dbquote(../@schema, @name)"/>
      <xsl:with-param name="others">
	<param name="table_qname" value="{skit:dbquote(../@schema, ../@name)}"/>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="index" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('table.', $parent_core)}"/>
    <xsl:if test="@tablespace">
      <dependency fqn="{concat('tablespace.', @tablespace)}"/>
    </xsl:if>
    <xsl:for-each select="reftable[@refschema != 'pg_catalog']">
      <dependency fqn="{concat('table.', ancestor::database/@name, '.', 
			        @refschema, '.', @reftable)}"/>
    </xsl:for-each>
    <xsl:call-template name="depends"/>
  </xsl:template>
</xsl:stylesheet>

