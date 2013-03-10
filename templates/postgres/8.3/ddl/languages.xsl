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
	<!-- This direct generation of set session auth is naff.
	     TODO: FIX THIS -->
	<xsl:text>set session authorization &apos;</xsl:text>
	<xsl:value-of select="@owner"/>
	<xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>

        <xsl:text>create language </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:apply-templates/>

	<xsl:text>reset session authorization;&#x0A;</xsl:text>
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
      </print>
    </xsl:if>
    <xsl:apply-templates/>

    <xsl:if test="../@action='diffcomplete'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:text>&#x0A;alter language </xsl:text>
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


