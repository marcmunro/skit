<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Operator classes -->
  <xsl:template match="operator_class">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="operator_class_fqn" 
		  select="concat('operator_class.', 
			  ancestor::database/@name, '.', 
			  @schema, '.', @name, '(',
			  @method, ')')"/>
    <dbobject type="operator_class" fqn="{$operator_class_fqn}"
	      name="{@name}" qname="{skit:dbquote(@schema, @name)}">
      <dependencies>
	<!-- operator family -->
	<dependency fqn="{concat('operator_family.', 
			  ancestor::database/@name, '.', 
			  @family_schema, '.', @family, '(',
			  @method, ')')}"/>

	<!-- types will be a dependency of operators, etc -->

	<!-- operators -->
	<xsl:for-each select="opclass_operator">
	  <xsl:if test="@schema != 'pg_catalog'">
	    <dependency fqn="{concat('operator.', 
			     ancestor::database/@name, '.', 
			     @schema, '.', @name, '(',
			     arg[@position='left']/@schema, '.',
			     arg[@position='left']/@name, ',',
			     arg[@position='right']/@schema, '.',
			     arg[@position='right']/@name, ')')}"/>
	    
	  </xsl:if>
	</xsl:for-each>

	<!-- functions -->
	<xsl:for-each select="opclass_function">
	  <xsl:if test="@schema != 'pg_catalog'">
	    <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', 
			     @schema, '.', @name, '(',
			     params/param[@position='1']/@schema, '.',
			     params/param[@position='1']/@type, ',',
			     params/param[@position='2']/@schema, '.',
			     params/param[@position='2']/@type, ')')}"/>
	    
	  </xsl:if>
	</xsl:for-each>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @signature)"/>
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
