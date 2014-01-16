<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="cast" mode="build">
    <xsl:value-of 
	select="concat('&#x0A;create cast(',
		        skit:dbquote(source/@schema,source/@type), ' as ',
			skit:dbquote(target/@schema,target/@type),
			')&#x0A;  ')"/>
    <xsl:choose>
      <xsl:when test="handler-function">
	<xsl:value-of 
	    select="concat('with function ',
		           skit:dbquote(handler-function/@schema,
			                handler-function/@name),
			   '(', 
			   skit:dbquote(source/@schema,source/@type),
			   ')&#x0A;  ')"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>without function</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="@context='a'">
      <xsl:text>as assignment</xsl:text>
    </xsl:if>
    <xsl:if test="@context='i'">
      <xsl:text>as implicit</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="cast" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop cast(',
		       skit:dbquote(source/@schema,source/@type), ' as ',
		       skit:dbquote(target/@schema,target/@type),
		       ');&#x0A;  ')"/>
  </xsl:template>
</xsl:stylesheet>

