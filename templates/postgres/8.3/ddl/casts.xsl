<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/cast">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
      	<xsl:text>create cast(</xsl:text>
        <xsl:value-of select="skit:dbquote(source/@schema,source/@type)"/>
      	<xsl:text> as </xsl:text>
        <xsl:value-of select="skit:dbquote(target/@schema,target/@type)"/>
      	<xsl:text>)&#x0A;  </xsl:text>
	<xsl:choose>
	  <xsl:when test="handler-function">
      	    <xsl:text>with function </xsl:text>
            <xsl:value-of select="skit:dbquote(handler-function/@schema,
				               handler-function/@name)"/>
      	    <xsl:text>(</xsl:text>
            <xsl:value-of select="skit:dbquote(source/@schema,source/@type)"/>
      	    <xsl:text>)&#x0A;  </xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
      	    <xsl:text>without function</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="@context='a'">
      	  <xsl:text>as assignment</xsl:text>
	</xsl:if>
	<xsl:if test="@context='i'">
      	  <xsl:text>as implicit</xsl:text>
	</xsl:if>
      	<xsl:text>;&#x0A;</xsl:text>
	<xsl:apply-templates/>  <!-- Deal with comments -->
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
      	<xsl:text>&#x0A;</xsl:text>
      	<xsl:text>drop cast(</xsl:text>
        <xsl:value-of select="skit:dbquote(source/@schema,source/@type)"/>
      	<xsl:text> as </xsl:text>
        <xsl:value-of select="skit:dbquote(target/@schema,target/@type)"/>
      	<xsl:text>);&#x0A;  </xsl:text>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

