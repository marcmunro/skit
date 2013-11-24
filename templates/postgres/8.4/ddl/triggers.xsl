<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/trigger">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of select="source/text()"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>
	<xsl:text>&#x0A;drop trigger </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text> on </xsl:text>
	<xsl:value-of select="skit:dbquote(@schema,@table)"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


