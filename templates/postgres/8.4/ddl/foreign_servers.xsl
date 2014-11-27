<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template match="foreign_server" mode="build">
    <xsl:value-of 
	select="concat('create server ', ../@qname,
                       '&#x0A;    ')"/>
    <xsl:if test="@type">
      <xsl:text>type '</xsl:text>
      <xsl:value-of select="@type"/>
      <xsl:text>'&#x0A;    </xsl:text>
    </xsl:if>
    <xsl:if test="@version">
      <xsl:text>version '</xsl:text>
      <xsl:value-of select="@version"/>
      <xsl:text>'&#x0A;    </xsl:text>
    </xsl:if>
    <xsl:value-of select="concat('foreign data wrapper ', 
			         @foreign_data_wrapper)"/>
    <xsl:if test="@options">
      <xsl:variable name="options" 
		    select="substring(@options, 2, 
			              string-length(@options) - 2)"/>
      <xsl:text>&#x0A;    options(</xsl:text>
      <xsl:call-template name="process-options">
	<xsl:with-param name="options" select="$options"/>
      </xsl:call-template>
      <xsl:text>)</xsl:text>
    </xsl:if>
     <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="foreign_server" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop server ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

