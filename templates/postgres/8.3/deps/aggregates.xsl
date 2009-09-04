<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="aggregate">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="aggregate_fqn" select="concat('aggregate.', 
					       $parent_core, '.', @name)"/>
    <dbobject type="aggregate" fqn="{$aggregate_fqn}"
	      name="{@name}" qname="{@qname}">
      <dependencies>
	<!-- owner -->
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
	<!-- base type -->
	<xsl:if test="basetype[@schema != 'pg_catalog']">
	  <dependency fqn="{concat('type.', 
			            ancestor::database/@name, '.', 
				    basetype/@schema, '.',
				    basetype/@name)}"/>
	</xsl:if>
	<!-- trans type -->
	<xsl:if test="transtype[@schema != 'pg_catalog']">
	  <dependency fqn="{concat('type.', 
			            ancestor::database/@name, '.', 
				    transtype/@schema, '.',
				    transtype/@name)}"/>
	</xsl:if>
	<!-- trans func -->
	<xsl:if test="transfunc[@schema != 'pg_catalog']">
	  <dependency fqn="{concat('function.', 
			            ancestor::database/@name, '.', 
				    transfunc/@schema, '.',
				    transfunc/@name, '(',
				    transtype/@schema, '.',
				    transtype/@name, ',',
				    transtype/@schema, '.',
				    transtype/@name, ')')}"/>
	</xsl:if>
	<!-- sort op -->
	<xsl:if test="sortop[@schema != 'pg_catalog']">
	  <dependency fqn="{concat('operator.', 
			            ancestor::database/@name, '.', 
				    sortop/@schema, '.',
				    sortop/@name, '(',
				    transtype/@schema, '.',
				    transtype/@name, ',',
				    transtype/@schema, '.',
				    transtype/@name, ')')}"/>
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