<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  version="1.0">

  <xsl:variable name="is_diff" select="boolean(/*/dbobject/@diff)"/>

  <xsl:template match="/*">
    <xsl:copy select=".">
      <!-- Eliminate retain_deps if provided (list *must* remove deps) -->
      <xsl:for-each select="@*">
	<xsl:if test="name(.) != 'retain_deps'">
	  <xsl:copy-of select="."/>
	</xsl:if>
      </xsl:for-each>
      <printable/>  <!-- Simple tag to show whether the output doc is
			 printable as other than an xml document -->
      <xsl:apply-templates>
	<xsl:with-param name="depth" select="1"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="dbobject[not(@nolist='true')]">
    <xsl:param name="depth"/>
    <xsl:choose>
      <xsl:when test="(self::*[@type='grant']) and 
		      (skit:eval('grants') != 't')"/>
      <xsl:when test="(self::*[@type='context']) and 
                      (skit:eval('contexts') != 't')"/>
      <xsl:otherwise>
	<xsl:call-template name="printObject">
	  <xsl:with-param name="depth" select="$depth"/>
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="printObject">
    <xsl:param name="depth"/>
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <print>
	<xsl:attribute name="text">
	  <xsl:if test="@type!='cluster' and @type != 'database'">
	    <xsl:value-of 
	       select="substring('                            ', 1, $depth*2)"/>
	  </xsl:if>

	  <xsl:value-of select="@type"/>
	  <xsl:text>=</xsl:text>
	  <xsl:value-of select="@name"/>
	  <xsl:text>|</xsl:text>
	  <xsl:value-of select="@fqn"/>
	  <xsl:if test="$is_diff">
	    <xsl:text>|diff=</xsl:text>
	    <xsl:value-of select="@diff"/>
	  </xsl:if>
	  <xsl:if test="@action">
	    <xsl:text>|</xsl:text>
	    <xsl:value-of select="@action"/>
	  </xsl:if>
	  <xsl:text>&#xA;</xsl:text>
	</xsl:attribute>
      </print>
      <xsl:choose>
	<xsl:when test="@type='cluster'">
	  <xsl:apply-templates>
	    <xsl:with-param name="depth" select="$depth"/>
	  </xsl:apply-templates>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates>
	    <xsl:with-param name="depth" select="$depth+1"/>
	  </xsl:apply-templates>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
  </xsl:template>
</xsl:stylesheet>


