<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template match="user_mapping" mode="build">
    <xsl:variable name="user">
      <xsl:choose>
	<xsl:when test="@user">
	  <xsl:value-of select="skit:dbquote(@user)"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text>public</xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:value-of 
	select="concat('create user mapping for ', $user,
                       ' server ', @server)"/>
    <xsl:if test="@options">
      <xsl:variable name="options" 
		    select="substring(@options, 2, 
			              string-length(@options) - 2)"/>
      <xsl:text>&#x0A;    options(</xsl:text>
      <xsl:call-template name="process-options">
	<xsl:with-param name="options" select="$options"/>
      </xsl:call-template>
      <xsl:text>)</xsl:text>
    </xsl:if>
     <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="user_mapping" mode="drop">
    <xsl:variable name="user">
      <xsl:choose>
	<xsl:when test="@user">
	  <xsl:value-of select="skit:dbquote(@user)"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text>public</xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:value-of 
	select="concat('&#x0A;drop user mapping for ', $user,
                       ' server ', @server, ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

