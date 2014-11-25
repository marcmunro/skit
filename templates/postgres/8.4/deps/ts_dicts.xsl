<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- text search dictionaries -->
  <xsl:template match="tsdictionary">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="tsdictionary" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>

    <xsl:if test="@template_schema!='pg_catalog'">
      <dependency fqn="{concat('template.', 
		               ancestor::database/@name, '.',
			       @template_schema, '.', 
			       @template_name)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
