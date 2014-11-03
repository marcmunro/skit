<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Cannot use set session auth to change user in order to set the
       owner, so this template is explicit. -->
  <xsl:template match="dbobject[@action='build']/schema">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:choose>
	<xsl:when test="../@name='public'">
	  <xsl:value-of 
	      select="concat('&#x0A;alter schema ', ../@qname, ' owner to ')"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of 
	      select="concat('&#x0A;create schema ', ../@qname, 
		             ' authorization ')"/>
	</xsl:otherwise>
      </xsl:choose>
      <xsl:value-of select="concat(skit:dbquote(@owner), ';&#x0A;')"/>
      <xsl:apply-templates/>
    </print>
  </xsl:template>

  <xsl:template match="schema" mode="drop">
    <xsl:value-of select="concat('&#x0A;drop schema ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="schema" mode="diffprep">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('&#x0A;alter schema ', ../@qname, ' owner to ',
			 skit:dbquote($username), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="schema" mode="diff">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('&#x0A;alter schema ', ../@qname, ' owner to ',
			 skit:dbquote(@new), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

