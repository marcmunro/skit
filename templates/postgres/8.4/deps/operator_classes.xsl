<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="operator_dep">
    <xsl:if test="@schema != 'pg_catalog'">
      <dependency fqn="{concat('operator.', 
			ancestor::database/@name, '.', 
			@schema, '.', @name, '(',
			arg[@position='left']/@schema, '.',
			arg[@position='left']/@name, ',',
			arg[@position='right']/@schema, '.',
			arg[@position='right']/@name, ')')}"/>
      </xsl:if>
  </xsl:template>

  <xsl:template name="operator_function_dep">
    <xsl:if test="@schema != 'pg_catalog'">
      <dependency fqn="{concat('function.', 
		        ancestor::database/@name, '.', @function)}"/>
	
    </xsl:if>
  </xsl:template>


  <!-- Operator classes -->
  <xsl:template match="operator_class">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="fqn"
		      select="concat('operator_class.', 
			             ancestor::database/@name, '.', 
				     @schema, '.', @name, '(',
				     @method, ')')"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="operator_class" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>

    <!-- operator family -->
    <dependency fqn="{concat('operator_family.', 
		             ancestor::database/@name, '.', 
			     @family_schema, '.', @family, '(',
			     @method, ')')}"/>

    <xsl:if test="@type_name and (@type_schema!='pg_catalog')">
      <dependency fqn="{concat('type.', 
		               ancestor::database/@name, '.', 
			       skit:dbquote(@type_schema, @type_name))}"/>
    </xsl:if>
    <!-- types will be a dependency of operators, etc -->
    <xsl:for-each select="opclass_operator">
      <xsl:call-template name="operator_dep"/>
    </xsl:for-each>

    <xsl:for-each select="opclass_function">
      <xsl:call-template name="operator_function_dep"/>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>

