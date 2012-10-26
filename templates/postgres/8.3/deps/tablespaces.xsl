<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


 <!-- Tablespaces -->
  <xsl:template match="tablespace">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('tablespace.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="tablespace" name="{@name}" qname="{skit:dbquote(@name)}"
	      fqn="{$tbs_fqn}">
      <dependencies>
	<dependency fqn="cluster"/>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.', $parent_core, '.',
			   @owner)}"/>
	</xsl:if>
      </dependencies>
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
