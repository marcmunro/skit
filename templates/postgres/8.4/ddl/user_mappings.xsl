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

  <xsl:template match="user_mapping" mode="diff">
    <do-print/>
    <xsl:text>&#x0A;</xsl:text>
    <xsl:for-each select="../element/option">
      <xsl:value-of select="concat('alter user mapping for ', 
			           skit:dbquote(../user_mapping/@user), 
				   ' server ', ../user_mapping/@server,
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

