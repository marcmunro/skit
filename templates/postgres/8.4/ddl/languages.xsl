<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="language" mode="build">
    <xsl:text>&#x0A;create </xsl:text>
    <xsl:if test="@trusted='yes'">
      <xsl:text>trusted </xsl:text>
    </xsl:if>
    <xsl:value-of 
	select="concat('language ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="language" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop language ', ../@qname, 
		       ';&#x0A;')"/>
    <xsl:if test="../@name = 'plpgsql'">
      <xsl:value-of 
	  select="concat('&#x0A;drop function plpgsql_validator(oid);',
		         '&#x0A;drop function plpgsql_call_handler();',
			 '&#x0A;')"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="language" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:for-each select="../attribute">
	<xsl:if test="@name='owner'">
	  <xsl:value-of 
	      select="concat('&#x0A;alter language ', ../@qname,
		                 ' owner to ', skit:dbquote(@new),
		             ';&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>


