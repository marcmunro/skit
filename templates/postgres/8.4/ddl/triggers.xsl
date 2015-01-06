<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="alter_enablement">
    <xsl:choose>
      <xsl:when test="@enabled='D'">
	<xsl:text> disable</xsl:text>
      </xsl:when>
      <xsl:when test="@enabled='A'">
	<xsl:text> enable always</xsl:text>
      </xsl:when>
      <xsl:when test="@enabled='R'">
	<xsl:text> enable replica</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text> enable</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="alter_trigger_enablement">
    <xsl:value-of 
	select="concat('alter table ', skit:dbquote(@schema, @table),
		       '&#x0A;   ')"/>
    <xsl:call-template name="alter_enablement"/>
    <xsl:value-of 
	select="concat(' trigger ', ../@qname, ';&#x0A;')"/>
  </xsl:template>


  <xsl:template match="trigger" mode="build">
    <xsl:value-of 
	select="concat(source/text(), ';&#x0A;')"/>

    <xsl:if test="@enabled!='O'">
      <xsl:call-template name="alter_trigger_enablement"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="trigger" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop trigger ', ../@qname, ' on ',
		       skit:dbquote(@schema,@table), ';&#x0A;')"/>
  </xsl:template>
</xsl:stylesheet>


