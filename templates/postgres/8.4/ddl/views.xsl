<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="build_viewbase">
    <xsl:value-of 
	select="concat('&#x0A;create or replace view ', ../@qname,
		       ' as select &#x0A;')"/>
    <xsl:for-each select="column">
      <xsl:value-of 
	  select="concat('    null::', skit:dbquote(@type_schema, @type),
		         ' as ', @name)"/>
      <xsl:if test="position() != last()">
	<xsl:text>,&#x0A;</xsl:text>
      </xsl:if>
    </xsl:for-each>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="view" mode="build">
    <xsl:choose>
      <xsl:when test="../@type='viewbase'">
	<xsl:call-template name="build_viewbase"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of 
	    select="concat('&#x0A;create or replace view ', ../@qname,
		           ' as&#x0A;  ', source/text(), '&#x0A;')"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="view" mode="drop">
    <xsl:choose>
      <xsl:when test="../@type='viewbase'">
	<xsl:call-template name="build_viewbase"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of 
	    select="concat('&#x0A;drop view ', ../@qname, ';&#x0A;')"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>


