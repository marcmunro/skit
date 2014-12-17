<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Columns -->
  <xsl:template match="table/column">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>

    <!-- Make a second copy of the column (outside of the dbobject
	 element.  This is so that the table element will directly
	 contain the columns, which is necessary for ddl generation. -->
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
	<xsl:with-param name="parent_core" 
			select="concat($parent_core, '.', @name)"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="table/column" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    
    <dependencies>
      <dependency fqn="{concat('schema.', 
		               ancestor::database/@name, '.', 
			       ancestor::schema/@name)}"/>
      <!-- Dependencies on types for columns -->
      <xsl:if test="@type_schema != 'pg_catalog'">
	<dependency fqn="{concat('type.', 
		                 ancestor::database/@name, '.', 
				 @type_schema, '.', @type)}"/>
      </xsl:if>	

      <xsl:if test="@../extension">
	<dependency fqn="{concat('extension.', ancestor::database/@name,
			         '.',  @extension)}"/>
      </xsl:if>
      <xsl:if test="../inherits">
	<xsl:variable name="colname" select="@name"/>
	<!-- If this table inherits from other tables, then our column
	     will depend on the matching column(s) from the inheritees.
	     Also the inheritee will treat the mirror of the inheritor,
	     to be its own mirror.  This should ensure that any change
	     to an inherited column occurs before any change to a
	     matching non-inherited one.
	--> 
	<xsl:for-each
	    select="../inherits/inherited-column[@name=$colname]">
	  <dependency
	      propagate-mirror="true"
	      fqn="{concat('column.', ancestor::database/@name, '.', 
		           @schemaname, '.', @tablename, '.', @name)}"/>
	</xsl:for-each>
      </xsl:if>
    </dependencies>
  </xsl:template>
</xsl:stylesheet>

