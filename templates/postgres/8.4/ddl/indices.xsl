<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="index" mode="build">
    <xsl:value-of select="concat('&#x0A;', @indexdef, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="index" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop index ', ../@qname, ';&#x0A;')"/>
  </xsl:template>
</xsl:stylesheet>


