<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/aggregate">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create aggregate </xsl:text>
        <xsl:value-of select="skit:dbquote(@schema,@name)"/>
	<xsl:text>(</xsl:text>
	<xsl:choose>
	  <xsl:when test="basetype/@name">
            <xsl:value-of 
	       select="skit:dbquote(basetype/@schema,basetype/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>*</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>) (&#x0A;  sfunc = </xsl:text>
        <xsl:value-of select="skit:dbquote(transfunc/@schema,transfunc/@name)"/>
	<xsl:text>,&#x0A;  stype = </xsl:text>
        <xsl:value-of select="skit:dbquote(transtype/@schema,transtype/@name)"/>
	<xsl:if test="@initcond">
	  <xsl:text>,&#x0A;  initcond = </xsl:text>
          <xsl:value-of select="@initcond"/>
	</xsl:if>
	<xsl:if test="finalfunc">
	  <xsl:text>,&#x0A;  finalfunc = </xsl:text>
          <xsl:value-of 
	     select="skit:dbquote(finalfunc/@schema,finalfunc/@name)"/>
	</xsl:if>
	<xsl:if test="sortop">
	  <xsl:text>,&#x0A;  sortop = </xsl:text>
          <xsl:value-of select="skit:dbquote(sortop/@schema,sortop/@name)"/>
	</xsl:if>
	<xsl:text>);&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
      	<xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>
	  
      	<xsl:text>drop aggregate </xsl:text>
        <xsl:value-of select="skit:dbquote(@schema,@name)"/>
	<xsl:text>(</xsl:text>
	<xsl:choose>
	  <xsl:when test="basetype/@name">
            <xsl:value-of 
	       select="skit:dbquote(basetype/@schema, basetype/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>*</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>);&#x0A;</xsl:text>
	  
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

