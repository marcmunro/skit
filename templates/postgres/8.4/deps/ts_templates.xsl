<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- text search template -->
  <xsl:template match="text_search_template">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="text_search_template" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@init_proc and @init_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @init_schema, '.', 
			       @init_proc, 
			       '(pg_catalog.internal)')}"/>
    </xsl:if>
    <xsl:if test="@lexize_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @lexize_schema, '.', 
			       @lexize_proc, 
			       '(pg_catalog.internal,pg_catalog.internal,',
			       'pg_catalog.internal,pg_catalog.internal)')}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
