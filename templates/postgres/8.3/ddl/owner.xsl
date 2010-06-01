<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Coding standard for postgres DDL generated from skit:
       Each object action is preceded by a blank line and
       followed by a blank line.  This means that there will be 2 lines
       between the creation of each different object.
    -->

  <xsl:template match="dbobject[@type='context']">
     <xsl:if test="@action='arrive'">
      <print>
	<xsl:text>set session authorization &apos;</xsl:text>
	<xsl:value-of select="@name"/>
	<xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="@action='depart'">
      <print>
	<xsl:text>reset session authorization;&#x0A;&#x0A;</xsl:text>
	</print>
    </xsl:if>	

  </xsl:template>

</xsl:stylesheet>

