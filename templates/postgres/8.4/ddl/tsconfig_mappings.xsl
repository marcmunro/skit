<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="tsconfig_mapping" mode="build">
    <xsl:value-of 
	select="concat('alter text search configuration ', 
		       skit:dbquote(@config_schema, @config_name),
		       '&#x0A;    add mapping for ', @name,
		       ' with ', 
		       skit:dbquote(@dictionary_schema, @dictionary_name),
		       ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="tsconfig_mapping" mode="drop">
    <xsl:value-of 
	select="concat('alter text search configuration ', 
		       skit:dbquote(@config_schema, @config_name),
		       '&#x0A;    drop mapping for ', @name,
		       ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

