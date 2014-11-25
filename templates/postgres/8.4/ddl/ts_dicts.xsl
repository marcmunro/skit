<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="text_search_dictionary" mode="build">
    <xsl:value-of 
	select="concat('create text search dictionary ', ../@qname,
		       ' (&#x0A;    template = ', 
		       skit:dbquote(@template_schema, @template_name))"/>
    <xsl:if test="@init_options">
      <xsl:value-of 
	  select="concat(',&#x0A;    ', @init_options)"/>
    </xsl:if>
    <xsl:text>);&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="text_search_dictionary" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop text search dictionary ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

