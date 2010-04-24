<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="cast">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="cast_fqn" select="concat('cast.', 
					       $parent_core, '.', @name)"/>
    <dbobject type="cast" fqn="{$cast_fqn}" name="{@name}"
	      qname="{concat('(', 
		             skit:dbquote(source/@schema,source/@type),
			     ' as ', 
		             skit:dbquote(target/@schema,target/@type), ')')}">
      <dependencies>
	<!-- source type -->
	<xsl:if test="source[@schema != 'pg_catalog']">
	  <dependency fqn="{concat('type.', 
			            ancestor::database/@name, '.', 
				    source/@schema, '.',
				    source/@type)}"/>
	</xsl:if>
	<!-- target type -->
	<xsl:if test="target[@schema != 'pg_catalog']">
	  <dependency fqn="{concat('type.', 
			            ancestor::database/@name, '.', 
				    target/@schema, '.',
				    target/@type)}"/>
	</xsl:if>
	<!-- handler func -->
	<xsl:if test="handler-function">
	  <dependency fqn="{concat('function.', 
			            ancestor::database/@name, '.', 
				    handler-function/@signature)}"/>
	</xsl:if>
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

