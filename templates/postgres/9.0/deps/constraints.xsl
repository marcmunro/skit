<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Table (not type) constraints -->
  <xsl:template match="table/constraint">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="others">
	<param name="table_qname" value="{skit:dbquote(../@schema, ../@name)}"/>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="table/constraint" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('table.', $parent_core)}"/>
    <xsl:if test="@tablespace">
      <dependency fqn="{concat('tablespace.', @tablespace)}"/>
    </xsl:if>
    <!-- Dependencies on other constraints -->
    <xsl:if test="reftable/@refconstraintname">
      <dependency fqn="{concat('constraint.', 
			       ancestor::database/@name, '.',
			       reftable/@refschema, '.',
			       reftable/@reftable, '.',
			       reftable/@refconstraintname)}"/>
    </xsl:if>

    <xsl:for-each select="column[@opclass_schema!='pg_catalog']">
      <dependency fqn="{concat('operator_class.', 
			       ancestor::database/@name, '.',
			       skit:dbquote(@opclass_schema,
			                    @opclass_name),
			       '(', ../@access_method, ')')}"/>
    </xsl:for-each>
    <xsl:call-template name="depends"/>
  </xsl:template>
</xsl:stylesheet>

