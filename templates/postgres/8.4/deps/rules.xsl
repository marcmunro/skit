<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- rules -->
  <xsl:template match="rule">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="qname" select="skit:dbquote(@name)"/>
      <xsl:with-param name="table_qname" 
		      select="skit:dbquote(../@schema, ../@name)"/>

    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="rule" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat(name(..), '.', $parent_core)}"/>
    <!-- Add explicitly identified dependencies -->
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
      <xsl:if test="(@schema != ../@schema) or (@table != ../@table)">
	<dependency fqn="{concat('table.', ancestor::database/@name, 
			         '.', @schema, '.', @table)}"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

