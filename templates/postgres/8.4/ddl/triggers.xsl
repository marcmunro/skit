<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="trigger" mode="build">
    <xsl:value-of 
	select="concat(source/text(), ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="trigger" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop trigger ', ../@qname, ' on ',
		       skit:dbquote(@schema,@table), ';&#x0A;')"/>
  </xsl:template>
</xsl:stylesheet>


