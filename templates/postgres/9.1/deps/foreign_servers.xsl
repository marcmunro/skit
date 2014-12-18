<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Foreign Server -->
  <xsl:template match="foreign_server">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="foreign_server" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>

    <dependency fqn="{concat('database.', $parent_core)}"/>
    <dependency fqn="{concat('foreign_data_wrapper.', 
		              ancestor::database/@name, '.', 
			      @foreign_data_wrapper)}"/>
  </xsl:template>
</xsl:stylesheet>

