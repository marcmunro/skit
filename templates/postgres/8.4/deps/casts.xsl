<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="cast">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="qname" 
		      select="concat('(', 
		                     skit:dbquote(source/@schema,source/@type),
			             ' as ', 
		                     skit:dbquote(target/@schema,target/@type),
				     ')')"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="cast" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('database.', $parent_core)}"/>
    <xsl:if test="source[@schema != 'pg_catalog']">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
		               source/@schema, '.', source/@type)}"/>
    </xsl:if>
    <!-- target type -->
    <xsl:if test="target[@schema != 'pg_catalog']">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
				target/@schema, '.', target/@type)}"/>
    </xsl:if>
    <!-- handler func -->
    <xsl:if test="handler-function">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
				handler-function/@signature)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

