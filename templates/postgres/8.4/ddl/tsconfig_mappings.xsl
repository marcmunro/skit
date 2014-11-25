<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="text_search_configuration_map" mode="build">
    <xsl:value-of 
	select="concat('alter text search configuration ', 
		       skit:dbquote(@config_schema, @config_name),
		       '&#x0A;    add mapping for ', @name, 
		       ' with ')"/>
    <xsl:for-each select="tsconfig_mapping"> 
      <xsl:if test="position()!=1">
	<xsl:text>,&#x0A;            </xsl:text>
      </xsl:if>
      <xsl:value-of 
	  select="skit:dbquote(@dictionary_schema, @dictionary_name)"/>
    </xsl:for-each>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="text_search_configuration_map" mode="drop">
    <xsl:value-of 
	select="concat('alter text search configuration ', 
		       skit:dbquote(@config_schema, @config_name),
		       '&#x0A;    drop mapping for ', @name,
		       ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

