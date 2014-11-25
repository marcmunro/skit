<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="text_search_template" mode="build">
    <xsl:value-of 
	select="concat('create text search template ', ../@qname,
		       ' (&#x0A;    lexize = ', 
		       skit:dbquote(@lexize_schema, @lexize_proc))"/>
    <xsl:if test="@init_proc">
    <xsl:value-of 
	select="concat(',&#x0A;    init = ', 
		       skit:dbquote(@init_schema, @init_proc))"/>
    </xsl:if>
    <xsl:text>);&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="text_search_template" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop text search template ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

