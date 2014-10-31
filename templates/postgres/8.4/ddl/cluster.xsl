<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Code layout standard for postgres DDL generated from skit:
       Each object action is preceded by a blank line and
       followed by a blank line.  This means that there will be 2 lines
       between the creation of each different object.
    -->

  <xsl:template match="dbobject/cluster">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="shell-feedback"/>
	<xsl:text>psql -d postgres &lt;&lt;&apos;CLUSTEREOF&apos;&#x0A;</xsl:text>
	<xsl:text>set standard_conforming_strings = off;&#x0A;</xsl:text>
	<xsl:text>set escape_string_warning = off;&#x0A;</xsl:text>
      </print>
    </xsl:if>	
  </xsl:template>
</xsl:stylesheet>

