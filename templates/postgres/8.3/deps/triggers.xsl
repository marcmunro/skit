<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- triggers -->
  <xsl:template match="trigger">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="trigger_fqn" 
		  select="concat('trigger.', $parent_core, '.', @name)"/>
    <dbobject type="trigger" fqn="{$trigger_fqn}" name="{@name}"
	      qname="{skit:dbquote(@name)}"
	      table_qname="{skit:dbquote(../@schema, ../@name)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <xsl:if test="@function">
	<dependencies>
	  <dependency fqn="{concat('function.', 
			            ancestor::database/@name, '.', 
				    @function)}"/>
	  <xsl:call-template name="SchemaGrant"/>
	  <xsl:call-template name="TableGrant">
	    <xsl:with-param name="priv" select="'trigger'"/>
	  </xsl:call-template>
	</dependencies>
      </xsl:if>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>

  </xsl:template>
</xsl:stylesheet>

