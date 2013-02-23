<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/language">

    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

        <xsl:text>create language </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:apply-templates/>

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;drop language </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:if test="../@name = 'plpgsql'">
          <xsl:text>&#x0A;drop function plpgsql_validator(oid);&#x0A;</xsl:text>
          <xsl:text>&#x0A;drop function plpgsql_call_handler();&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
    <xsl:apply-templates/>
  </xsl:template>
</xsl:stylesheet>


