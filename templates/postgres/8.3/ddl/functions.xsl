<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="function_header">
    <xsl:value-of select="skit:dbquote(@schema,@name)"/>
    <xsl:text>(</xsl:text>
    
    <xsl:for-each select="params/param">
      <xsl:if test="position() != 1">
	<xsl:text>,</xsl:text>
      </xsl:if>
      <xsl:text>&#x0A;    </xsl:text>
      <xsl:if test="@name">
	<xsl:value-of select="@name"/>
	<xsl:text> </xsl:text>
      </xsl:if>
      <xsl:if test="@mode">
	<xsl:choose>
	  <xsl:when test="@mode='i'">
	    <xsl:text>in </xsl:text>
	  </xsl:when>
	  <xsl:when test="@mode='o'">
	    <xsl:text>out </xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>inout </xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:if>
      <xsl:value-of select="skit:dbquote(@schema,@type)"/>
    </xsl:for-each>
    <xsl:text>)</xsl:text>
  </xsl:template>


  <xsl:template match="dbobject/function">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="skit:eval('echoes') = 't'">
          <xsl:text>\echo function </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create or replace&#x0A;function </xsl:text>
	<xsl:call-template name="function_header"/>
	<xsl:text>&#x0A;  returns </xsl:text>
	<xsl:if test="@returns_set">
          <xsl:text>setof </xsl:text>
	</xsl:if>

        <xsl:value-of select="skit:dbquote(result/@schema,result/@type)"/>
	<xsl:text>&#x0A;as</xsl:text>

	<xsl:choose>
	  <xsl:when test="@language='internal'">
	    <xsl:text> &apos;</xsl:text>
            <xsl:value-of select="source/text()"/>
	    <xsl:text>&apos;&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:when test="@language='c'">
	    <xsl:text> &apos;</xsl:text>
            <xsl:value-of select="@bin"/>
	    <xsl:text>&apos;, &apos;</xsl:text>
            <xsl:value-of select="source/text()"/>
	    <xsl:text>&apos;&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>&#x0A;$_$&#x0A;</xsl:text>
            <xsl:value-of select="source/text()"/>
	    <xsl:text>&#x0A;$_$&#x0A;</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>language </xsl:text>
	<xsl:value-of select="@language"/>
	<xsl:text> </xsl:text>
	<xsl:value-of select="@volatility"/>
	<xsl:if test="@is_strict='yes'">
	  <xsl:text> strict</xsl:text>
	</xsl:if>
	<xsl:if test="@security_definer='yes'">
	  <xsl:text> security definer</xsl:text>
	</xsl:if>
	<xsl:text> cost </xsl:text>
	<xsl:value-of select="@cost"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <xsl:if test="not(handler-for-type)">
      	<print>
	  <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	  <xsl:value-of select="../@fqn"/>
	  <xsl:text>&#x0A;</xsl:text>
	  <xsl:call-template name="set_owner"/>
	  
      	  <xsl:text>&#x0A;drop function </xsl:text>
          <xsl:value-of select="../@qname"/>
      	  <xsl:text>;&#x0A;</xsl:text>
	  
	  <xsl:call-template name="reset_owner"/>
      	</print>
      </xsl:if>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

