<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/conversion">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT </xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create </xsl:text>
	<xsl:if test="@is_default='t'">
	  <xsl:text>default </xsl:text>
	</xsl:if>
	<xsl:text>conversion </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text>&#x0A;  for &apos;</xsl:text>
        <xsl:value-of select="@source"/>
	<xsl:text>&apos; to &apos;</xsl:text>
        <xsl:value-of select="@destination"/>
	<xsl:text>&apos;&#x0A;  from </xsl:text>
        <xsl:value-of select="skit:dbquote(@function_schema,@function_name)"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT </xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>
	<xsl:text>&#x0A;drop conversion </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
        <xsl:text>---- DBOBJECT </xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:text>&#x0A;alter conversion </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> owner to </xsl:text>
            <xsl:value-of select="skit:dbquote(@new)"/>
            <xsl:text>;&#x0A;</xsl:text>
	  </xsl:if>
	  </xsl:for-each>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

