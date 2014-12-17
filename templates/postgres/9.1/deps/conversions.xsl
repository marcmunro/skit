<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- conversions -->
  <xsl:template match="conversion">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="conversion" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>

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
  </xsl:template>
</xsl:stylesheet>
