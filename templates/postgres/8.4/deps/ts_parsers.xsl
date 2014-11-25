<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- text search parser -->
  <xsl:template match="text_search_parser">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="text_search_parser" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@start_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @start_schema, '.', 
			       @start_proc, 
			       '(pg_catalog.internal,pg_catalog.integer)')}"/>
    </xsl:if>
    <xsl:if test="@gettoken_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @gettoken_schema, '.', 
			       @gettoken_proc, 
			       '(pg_catalog.internal,pg_catalog.internal,',
			       'pg_catalog.internal)')}"/>
    </xsl:if>
    <xsl:if test="@end_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @end_schema, '.', 
			       @end_proc, 
			       '(pg_catalog.internal)')}"/>
    </xsl:if>
    <xsl:if test="@lextypes_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @lextypes_schema, '.', 
			       @lextypes_proc, 
			       '(pg_catalog.internal)')}"/>
    </xsl:if>
    <xsl:if test="@headline_schema and @headline_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', 
		               ancestor::database/@name, '.',
			       @headline_schema, '.', 
			       @headline_proc, 
			       '(pg_catalog.internal,pg_catalog.internal,',
			       'pg_catalog.tsquery)')}"/>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>
