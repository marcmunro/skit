<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/domain">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="skit:eval('echoes') = 't'">
          <xsl:text>\echo domain </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
	<xsl:call-template name="set_owner"/>
	
        <xsl:text>create domain </xsl:text>
        <xsl:value-of select="skit:dbquote(@schema,@name)"/>
        <xsl:text>&#x0A;  as </xsl:text>
        <xsl:value-of select="skit:dbquote(@basetype_schema,@basetype)"/>
	<xsl:for-each select="constraint">
          <xsl:text>&#x0A;  </xsl:text>
          <xsl:value-of select="source/text()"/>
	</xsl:for-each>
	<xsl:if test="@nullable='no'">
          <xsl:text> not null</xsl:text>
	</xsl:if>
	<xsl:if test="@default">
          <xsl:text>&#x0A;  default </xsl:text>
          <xsl:value-of select="@default"/>
	</xsl:if>
        <xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;drop domain </xsl:text>
        <xsl:value-of select="skit:dbquote(@schema,@name)"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


