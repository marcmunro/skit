<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="collation" mode="build">
    <xsl:value-of 
	select='concat("create collation ", ../@qname, 
		       " (lc_collate = &apos;", @lc_collate,
		       "&apos;, lc_ctype = &apos;", @lc_ctype,
		       "&apos;);&#x0A;")'/>
  </xsl:template>

  <xsl:template match="collation" mode="drop">
    <xsl:value-of 
	select="concat('drop collation ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

<!--
  <xsl:template match="extension" mode="diff">
    <xsl:for-each select="../attribute[@name='version']">
      <do-print/>
      <xsl:value-of 
	  select='concat("alter extension ", ../@qname,
	                 " update to &apos;", @new, 
			 "&apos;;")'/>
    </xsl:for-each>
    <xsl:text>&#x0A;</xsl:text>
  </xsl:template>
-->
</xsl:stylesheet>

