<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/operator_family">
    <xsl:if test="(../@action='build') and (@auto_generated!='t')">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create operator family </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text>using </xsl:text>
        <xsl:value-of select="@method"/>
	<xsl:for-each select="opclass_operator">
	  <xsl:sort select="arg[@position='left']/@name"/>
	  <xsl:sort select="arg[@position='right']/@name"/>
	  <xsl:sort select="@strategy"/>
	  <xsl:if test="position() != 1">
	    <xsl:text>,</xsl:text>
	  </xsl:if>
	  <xsl:text>&#x0A;  operator </xsl:text>
	  <xsl:value-of select="@strategy"/>
	  <xsl:value-of select="skit:dbquote(@schema)"/>
	  <xsl:text>.</xsl:text>
	  <xsl:value-of select="@name"/>
	  <xsl:text>(</xsl:text>
	  <xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
				             arg[@position='left']/@name)"/>
	  <xsl:text>,</xsl:text>
	  <xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
				             arg[@position='right']/@name)"/>
	  <xsl:text>)</xsl:text>

	</xsl:for-each>

	<xsl:for-each select="opclass_function">
	  <xsl:sort select="params/param[@position='1']/@type"/>
	  <xsl:sort select="params/param[@position='2']/@type"/>
	  <xsl:sort select="@proc_num"/>
	  <xsl:text>,&#x0A;  function </xsl:text>
	  <xsl:value-of select="@proc_num"/>
	  <xsl:text> </xsl:text>
	  <xsl:value-of select="skit:dbquote(@schema,@name)"/>
	  <xsl:text>(</xsl:text>
	  <xsl:value-of
	     select="skit:dbquote(params/param[@position='1']/@schema,
		                  params/param[@position='1']/@type)"/>
	  <xsl:text>,</xsl:text>
	  <xsl:value-of
	     select="skit:dbquote(params/param[@position='2']/@schema,
		                  params/param[@position='2']/@type)"/>
	  <xsl:text>)</xsl:text>
	</xsl:for-each>

	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
      	<xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>
	  
	<xsl:text>drop operator family </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text> using </xsl:text>
        <xsl:value-of select="@method"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>


