<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='view']/view">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create or replace view </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text> as&#x0A;  </xsl:text>
        <xsl:value-of select="source/text()"/>
	<xsl:text>&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="set_owner"/>
	<xsl:text>&#x0A;drop view </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

  </xsl:template>

  <xsl:template match="dbobject[@type='viewbase']/view">
    <xsl:if test="../@action='build' or ../@action='drop'">
      <print>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create or replace view </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text> as select &#x0A;</xsl:text>
	<xsl:for-each select="column">
	  <xsl:text>    null::</xsl:text>
	  <xsl:value-of select="skit:dbquote(@type_schema, @type)"/>
	  <xsl:text> as </xsl:text>
	  <xsl:value-of select="@name"/>
	  <xsl:if test="position() != last()">
	    <xsl:text>,&#x0A;</xsl:text>
	  </xsl:if>
	</xsl:for-each>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>


