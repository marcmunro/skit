<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="text_search_parser" mode="build">
    <xsl:value-of 
	select="concat('create text search parser ', ../@qname,
		       ' (&#x0A;    start = ', 
		       skit:dbquote(@start_schema, @start_proc),
		       ',&#x0A;    gettoken = ', 
		       skit:dbquote(@token_schema, @token_proc),
		       ',&#x0A;    end = ', 
		       skit:dbquote(@end_schema, @end_proc),
		       ',&#x0A;    lextypes = ', 
		       skit:dbquote(@lextype_schema, @lextype_proc))"/>
    <xsl:if test="@headline_proc">
    <xsl:value-of 
	select="concat(',&#x0A;    headline = ', 
		       skit:dbquote(@headline_schema, @headline_proc))"/>
    </xsl:if>
    <xsl:text>);&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="text_search_parser" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop text search parser ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

