<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Indices -->
  <xsl:template name="depends">
    <xsl:for-each select="depends">
      <xsl:choose>
	<xsl:when test="@type='operator class'">
	  <dependency fqn="{concat('operator_class.', 
			           ancestor::database/@name,
			           '.', @name)}"/>
	</xsl:when>
	<xsl:when test="@type='function'">
	  <dependency fqn="{concat('function.', ancestor::database/@name,
			        '.', @name)}"/>
	</xsl:when>
	<xsl:otherwise>
	  <UNHANDLED_DEPENDS_NODE type="{@type}" name="{@name}"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="index">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="index_fqn" 
		  select="concat('index.', $parent_core, '.', @name)"/>
    <dbobject type="index" fqn="{$index_fqn}" name="{@name}"
	      qname="{skit:dbquote(../@schema, @name)}">
      <dependencies>
	<xsl:if test="@tablespace">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:if>
	<xsl:for-each select="reftable[@refschema != 'pg_catalog']">
	  <dependency fqn="{concat('table.', ancestor::database/@name, '.', 
			           @refschema, '.', @reftable)}"/>
	  
	</xsl:for-each>
	<xsl:call-template name="depends"/>
      </dependencies>
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

