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
      <xsl:if test="@function">
	<xsl:variable name="schema_name" select="@schema"/>
	<xsl:variable name="table_name" select="@table"/>
	<xsl:variable name="table_owner" 
		      select="//schema[@name=$schema_name]/table[@name=$table_name]/@owner"/>
	<dependencies>
	  <dependency fqn="{concat('table.', $parent_core)}"/>
	  <dependency fqn="{concat('function.', 
			            ancestor::database/@name, '.', 
				    @function)}"/>
	  <xsl:call-template name="SchemaGrant">
	    <xsl:with-param name="owner" select="../@owner"/>
	  </xsl:call-template>
	  <xsl:call-template name="TableGrant">
	    <xsl:with-param name="priv" select="'trigger'"/>
	    <xsl:with-param name="owner" select="$table_owner"/>
	  </xsl:call-template>
	</dependencies>
      </xsl:if>
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>

  </xsl:template>
</xsl:stylesheet>

