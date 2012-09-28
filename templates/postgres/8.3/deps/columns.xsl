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
	      qname="{skit:dbquote(@name)}">
      <dependencies>
	<!-- Dependencies on types for columns -->
	<xsl:if test="@type_schema != 'pg_catalog'">
	  <dependency fqn="{concat('type.', 
			    ancestor::database/@name, '.', 
			    @type_schema, '.', @type)}"/>
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
    <!-- Also create a copy of the column independently of the dbobject
	 copy.   This allows columns to be processed either as part of
	 the table dbobject, or directly as columns.  --> 
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
	<xsl:with-param name="parent_core" 
			select="concat($parent_core, '.', @name)"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>
</xsl:stylesheet>

