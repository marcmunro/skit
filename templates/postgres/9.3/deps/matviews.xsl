<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="materialized_view">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="others">
	<param name="cycle_breaker" value="viewbase"/>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="materialized_view" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:for-each select="depends[@function]">
      <xsl:choose>
	<xsl:when test="@cast">
	  <dependency fqn="{concat('cast.', ancestor::database/@name, 
			           '.', @cast)}"/>
	</xsl:when>
	<xsl:otherwise>
	  <dependency fqn="{concat('function.', ancestor::database/@name, 
			           '.', @function)}"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
    <xsl:for-each select="depends[@table]">
      <dependency fqn="{concat('table.', ancestor::database/@name, 
			       '.', @schema, '.', @table)}"/>
    </xsl:for-each>
    <xsl:for-each select="depends[@view]">
      <dependency fqn="{concat('view.', ancestor::database/@name, 
			       '.', @schema, '.', @view)}"/>
    </xsl:for-each>
    <xsl:for-each select="depends[@function]">
      <dependency fqn="{concat('function.', ancestor::database/@name, 
			       '.', @function)}"/>
    </xsl:for-each>
    <xsl:if test="@tablespace">
      <dependency fqn="{concat('tablespace.', @tablespace)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

