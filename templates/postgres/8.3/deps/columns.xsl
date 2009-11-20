<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Columns -->
  <xsl:template match="table/column">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:variable name="column_fqn" 
		  select="concat('column.', 
			  ancestor::database/@name, '.', 
			  ancestor::schema/@name, '.', 
			  ancestor::table/@name, '.', @name)"/>
    <dbobject type="column" fqn="{$column_fqn}" name="{@name}"
	      qname="{concat(skit:dbquote(../@schema,../@name), '.',
		             skit:dbquote(@name))}">
      <xsl:if test="@type_schema != 'pg_catalog'">
	<dependencies>
	  <dependency fqn="{concat('type.', ancestor::database/@name, '.',
				   @type_schema, '.', @type)}"/>
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

    <!-- Create a second copy of column entry, outside of the dbobject
         definition and within the table definition.  This second copy
         will be used in create table ddl, but the first copy is needed
         for dependency tracking, particularly when processing diffs
         where constraints may be placed upon new (added) columns in the
         table. -->

    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
	<xsl:with-param name="parent_core" 
			select="concat($parent_core, '.', @name)"/>
      </xsl:apply-templates>
    </xsl:copy>
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
