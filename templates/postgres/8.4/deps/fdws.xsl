<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Foreign Data Wrapper -->
  <xsl:template match="foreign_data_wrapper">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="foreign_data_wrapper" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('database.', $parent_core)}"/>
    <xsl:if test="@validator_schema and @validator_schema!='pg_catalog'">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
		               @validator_schema, '.',
			       @validator_proc, 
			       '(pg_catalog.text[],pg_catalog.oid')}"/>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

