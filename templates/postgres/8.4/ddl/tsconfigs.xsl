<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="text_search_configuration" mode="build">
    <xsl:value-of 
	select="concat('create text search configuration ', ../@qname,
		       ' (&#x0A;    parser = ', 
		       skit:dbquote(@parser_schema, @parser_name),
		       ');&#x0A;')"/>
  </xsl:template>

  <xsl:template match="text_search_configuration" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop text search configuration ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="text_search_configuration" mode="diffprep">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('&#x0A;alter text search configration ', ../@qname,
		         ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

