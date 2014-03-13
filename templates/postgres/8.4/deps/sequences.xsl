<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:skit="http://www.bloodnok.com/xml/skit"
    xmlns:exsl="http://exslt.org/common"
    extension-element-prefixes="exsl"
    version="1.0">

  <!-- Sequences -->
  <xsl:template match="sequence">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="sequence" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@tablespace">
      <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

