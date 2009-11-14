<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Tables -->
  <xsl:template match="table">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="table_fqn" 
		  select="concat('table.', 
			  ancestor::database/@name, '.', 
			  ancestor::schema/@name, '.', @name)"/>
    <dbobject type="table" fqn="{$table_fqn}" name="{@name}"
	      qname="{skit:dbquote(@schema,@name)}">
      <dependencies>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
	<xsl:if test="@tablespace">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:if>
	<!-- Dependencies on inherited tables -->
	<xsl:for-each select="inherits">
	  <dependency fqn="{concat('table.', 
			           ancestor::database/@name, '.',
				   @schema, '.', @name)}"/>
	</xsl:for-each>
	<!-- Dependencies on types -->
	<xsl:for-each select="column">
	  <xsl:if test="@type_schema != 'pg_catalog'">
	    <dependency fqn="{concat('type.', 
			             ancestor::database/@name, '.',
				     @type_schema, '.', @type)}"/>
	  </xsl:if>
	</xsl:for-each>
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
