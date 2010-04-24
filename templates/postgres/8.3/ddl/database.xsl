<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='dbincluster']/database">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:text>&#x0A;create database </xsl:text>
	<xsl:value-of select="../@qname"/>
	<xsl:text> with&#x0A;</xsl:text>
	<xsl:text> owner </xsl:text>
	<xsl:value-of select="skit:dbquote(@owner)"/>
	<xsl:text>&#x0A; encoding &apos;</xsl:text>
	<xsl:value-of select="@encoding"/>
	<xsl:text>&apos;&#x0A;</xsl:text>
	<xsl:text> tablespace </xsl:text>
	<xsl:value-of select="skit:dbquote(@tablespace)"/>
	<xsl:text>&#x0A; connection limit = </xsl:text>
	<xsl:value-of select="@connections"/>
	<xsl:text>;&#x0A;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:text>&#x0A;-- drop database </xsl:text>
	<xsl:value-of select="../@qname"/>
	<xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="dbobject[@type='database']/database">

    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;psql -d </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> &lt;&lt;&apos;DBEOF&apos;&#x0A;</xsl:text>
	<xsl:text>set standard_conforming_strings = off;&#x0A;</xsl:text>
        <xsl:text>set escape_string_warning = off;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>DBEOF&#x0A;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='arrive'">
      <print>
        <xsl:text>psql -d </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> &lt;&lt;&apos;DBEOF&apos;&#x0A;</xsl:text>
	<xsl:text>set standard_conforming_strings = off;&#x0A;</xsl:text>
        <xsl:text>set escape_string_warning = off;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='depart'">
      <print>
        <xsl:text>DBEOF&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:apply-templates/>
  </xsl:template>
</xsl:stylesheet>

