<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/schema">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:choose>
	  <xsl:when test="../@name='public'">
            <xsl:text>&#x0A;alter schema </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> owner to </xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
            <xsl:text>&#x0A;create schema </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> authorization </xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
        <xsl:value-of select="skit:dbquote(@owner)"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:apply-templates/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
        <xsl:text>&#x0A;drop schema </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:text>&#x0A;alter schema </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> owner to </xsl:text>
            <xsl:value-of select="skit:dbquote(@new)"/>
            <xsl:text>;&#x0A;</xsl:text>
	  </xsl:if>
	  </xsl:for-each>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>


  </xsl:template>
</xsl:stylesheet>

