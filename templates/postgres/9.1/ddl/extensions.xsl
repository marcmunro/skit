<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="extension" mode="build">
    <xsl:if test="(@name!='plpgsql') or @diff">
      <!-- If this is plpgsql, it should only be created if we
	   are processing a diff.  -->
      <xsl:value-of 
	  select='concat("create extension ", ../@qname,
		         "&#x0A;    schema ", skit:dbquote(@schema),
			 "&#x0A;    version &apos;", @version,
			 "&apos;")'/>
      <xsl:text>;&#x0A;</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="extension" mode="drop">
    <xsl:if test="(@name!='plpgsql') or @diff">
      <!-- If this is plpgsql, it should only be created if we
	   are processing a diff.  -->
      <xsl:value-of 
	  select="concat('&#x0A;drop extension ', ../@qname, ';&#x0A;')"/>
    </xsl:if>
  </xsl:template>

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

</xsl:stylesheet>

