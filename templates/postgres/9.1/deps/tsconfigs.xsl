<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- text serach configurations -->
  <xsl:template match="text_search_configuration">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="text_search_configuration" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>
    <dependency fqn="{concat('role.', @owner)}"/>

    <xsl:if test="@parser_schema!='pg_catalog'">
      <dependency fqn="{concat('text_search_parser.', 
		               ancestor::database/@name, '.',
			       @parser_schema, '.', @parser_name)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
