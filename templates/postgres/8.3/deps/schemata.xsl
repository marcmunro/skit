<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Schemata -->
  <xsl:template match="schema">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('schema.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="schema" name="{@name}" qname="{skit:dbquote(@name)}"
	      fqn="{$tbs_fqn}">
      <xsl:if test="@owner != 'public'">
	<dependencies>
	  <dependency fqn="{concat('database.', $parent_core)}"/>
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
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

