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
    <dbobject type="cast" fqn="{$cast_fqn}"
	      name="{@name}" qname="{@qname}">
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

<!-- Keep this comment at the end of the file
Local variables:
mode: xml
sgml-omittag:nil
sgml-shorttag:nil
sgml-namecase-general:nil
sgml-general-insert-case:lower
sgml-minimize-attributes:nil
sgml-always-quote-attributes:t
sgml-indent-step:2
sgml-indent-data:t
sgml-parent-document:nil
sgml-exposed-tags:nil
sgml-local-catalogs:nil
sgml-local-ecat-files:nil
End:
-->
