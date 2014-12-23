<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template match="foreign_server" mode="build">
    <xsl:value-of 
	select="concat('create server ', ../@qname,
                       '&#x0A;    ')"/>
    <xsl:if test="@type">
      <xsl:text>type '</xsl:text>
      <xsl:value-of select="@type"/>
      <xsl:text>'&#x0A;    </xsl:text>
    </xsl:if>
    <xsl:if test="@version">
      <xsl:text>version '</xsl:text>
      <xsl:value-of select="@version"/>
      <xsl:text>'&#x0A;    </xsl:text>
    </xsl:if>
    <xsl:value-of select="concat('foreign data wrapper ', 
			         @foreign_data_wrapper)"/>
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

  <xsl:template match="foreign_server" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop server ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="foreign_server" mode="diff">
    <do-print/>
    <xsl:text>&#x0A;</xsl:text>
    <xsl:for-each select="../attribute[@name='version']">
      <xsl:value-of select='concat("alter server ", ../@qname,
                                   " version &apos;", @new)'/>
    </xsl:for-each>
    <xsl:for-each select="../element/option">
      <xsl:value-of select="concat('alter server ', ../../@qname,
			           ' options (')"/>
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

