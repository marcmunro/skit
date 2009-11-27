<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Table (not type) constraints -->
  <xsl:template match="table/constraint">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="constraint_fqn" 
		  select="concat('constraint.', $parent_core, '.', @name)"/>
    <dbobject type="constraint" fqn="{$constraint_fqn}" name="{@name}"
	      qname="{skit:dbquote(@name)}"
	      table_qname="{skit:dbquote(../@schema, ../@name)}">
      <dependencies>
	<xsl:if test="@tablespace">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:if>
	<xsl:for-each select="reftable[@refschema != 'pg_catalog']">
	  <dependency fqn="{concat('table.', ancestor::database/@name, '.', 
			           @refschema, '.', @reftable)}"/>
	  
	  <xsl:choose>
	    <xsl:when test="@refconstraintname">
	      <dependency fqn="{concat('constraint.', 
			                ancestor::database/@name, '.', 
			                @refschema, '.', @reftable,
					'.', @refconstraintname)}"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <!-- We have a direct dependency on an index rather than
		   a constraint -->
	      <dependency fqn="{concat('index.', 
			                ancestor::database/@name, '.', 
			                @refschema, '.', @reftable,
					'.', @refindexname)}"/>
	    </xsl:otherwise>
	  </xsl:choose>
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
