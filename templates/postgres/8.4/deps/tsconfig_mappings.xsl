<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- text serach configurations -->
  <xsl:template match="tsconfig_map">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="tsconfig_map" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('tsconfig.', $parent_core)}"/>

    <xsl:for-each select="tsconfig_mapping[@dictionary_schema!='pg_catalog']">
      <dependency fqn="{concat('schema.', ancestor::database/@name,
		               '.', @dictionary_schema)}"/>
      <dependency fqn="{concat('dictionary.', ancestor::database/@name,
		               '.', @dictionary_schema,
			       '.', @dictionary_name)}"/>
      <dependency fqn="{concat('role.', @dictionary_owner)}"/>
    </xsl:for-each>
    
  </xsl:template>
</xsl:stylesheet>
