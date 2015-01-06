<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="alter_rule_enablement">
    <xsl:value-of 
	select="concat('alter table ', skit:dbquote(@schema, @table),
		       '&#x0A;   ')"/>
    <xsl:call-template name="alter_enablement"/>
    <xsl:value-of 
	select="concat(' rule ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="rule" mode="build">
    <xsl:value-of 
	select="concat(source/text(), ';&#x0A;')"/>
    <xsl:if test="@enabled!='O'">
      <xsl:call-template name="alter_rule_enablement"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="rule" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop rule ', ../@qname, ' on ',
		       skit:dbquote(@schema,@table), ';&#x0A;')"/>
  </xsl:template>
</xsl:stylesheet>


