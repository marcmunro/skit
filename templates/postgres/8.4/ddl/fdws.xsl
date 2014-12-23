<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template match="foreign_data_wrapper" mode="build">
    <xsl:value-of 
	select="concat('create foreign data wrapper ', ../@qname,
                       '&#x0A;    ')"/>
    <xsl:choose>
      <xsl:when test="@validator_proc">
	<xsl:value-of 
	    select="concat('validator ', 
		           skit:dbquote(@validator_schema,@validator_proc))"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>no validator</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="option">
      <xsl:text>&#x0A;    options(</xsl:text>
      <xsl:for-each select="option">
	<xsl:if test="position() != 1">
	  <xsl:text>,&#x0A;            </xsl:text>
	</xsl:if>
	<xsl:value-of 
	    select='concat(@name, " &apos;", @value, "&apos;")'/>
      </xsl:for-each>
      <xsl:text>)</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="foreign_data_wrapper" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop foreign data wrapper ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="foreign_data_wrapper" mode="diff">
    <do-print/>
    <xsl:text>&#x0A;</xsl:text>
    <xsl:for-each select="../element/option">
      <xsl:value-of select="concat('alter foreign data wrapper ', 
			           ../../@qname, ' options (')"/>
      <xsl:choose>
	<xsl:when test="../@status='gone'">
	  <xsl:value-of select="concat('drop ', @name, ');&#x0A;')"/>
	</xsl:when>
	<xsl:when test="../@status='new'">
	  <xsl:value-of select='concat("add ", @name, " &apos;",
				       @value, "&apos;);&#x0A;")'/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select='concat("add ", @name, " &apos;",
				       @value, "&apos;);&#x0A;")'/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

