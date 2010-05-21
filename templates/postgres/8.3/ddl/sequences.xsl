<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/sequence">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:if test="skit:eval('echoes') = 't'">
          <xsl:text>\echo sequence </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>create sequence </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text>&#x0A;  start with </xsl:text>
        <xsl:value-of select="@start_with"/>
	<xsl:text>&#x0A;  increment by </xsl:text>
        <xsl:value-of select="@increment_by"/>
	<xsl:text>&#x0A;  minvalue </xsl:text>
        <xsl:value-of select="@min_value"/>
	<xsl:text>&#x0A;  maxvalue </xsl:text>
        <xsl:value-of select="@max_value"/>
	<xsl:text>&#x0A;  cache </xsl:text>
        <xsl:value-of select="@cache"/>
	<xsl:if test="@cycled='t'">
	  <xsl:text>&#x0A;  cycle</xsl:text>
	</xsl:if>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;drop sequence </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


