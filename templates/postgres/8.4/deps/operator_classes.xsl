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
    <xsl:variable name="operator_class_fqn" 
		  select="concat('operator_class.', 
			  ancestor::database/@name, '.', 
			  @schema, '.', @name, '(',
			  @method, ')')"/>
    <dbobject type="operator_class" fqn="{$operator_class_fqn}"
	      name="{@name}" qname="{skit:dbquote(@schema, @name)}"
	      parent="{concat(name(..), '.', $parent_core)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<dependency fqn="{concat('schema.', $parent_core)}"/>
	<!-- owner -->
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>

	<!-- operator family -->
	<dependency fqn="{concat('operator_family.', 
			  ancestor::database/@name, '.', 
			  @family_schema, '.', @family, '(',
			  @method, ')')}"/>

	<!-- types will be a dependency of operators, etc -->

	<xsl:for-each select="opclass_operator">
	  <xsl:call-template name="operator_dep"/>
	</xsl:for-each>

	<xsl:for-each select="opclass_function">
	  <xsl:call-template name="operator_function_dep"/>
	</xsl:for-each>

	<xsl:call-template name="SchemaGrant"/>
      </dependencies>
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @signature)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>
</xsl:stylesheet>

