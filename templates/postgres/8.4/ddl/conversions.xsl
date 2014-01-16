<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="conversion" mode="build">
    <xsl:text>&#x0A;create </xsl:text>
    <xsl:if test="@is_default='t'">
      <xsl:text>default </xsl:text>
    </xsl:if>
    <xsl:value-of 
	select="concat('conversion ', ../@qname,
	               '&#x0A;  for ', $apos, @source, $apos,
		       ' to ', $apos, @destination, $apos,
		       '&#x0A;  from ',
		       skit:dbquote(@function_schema,@function_name),
		       ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="conversion" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop conversion ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="conversion" mode="diffprep">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('&#x0A;alter conversion ', ../@qname,
		         ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

