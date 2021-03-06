<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Operators -->
  <xsl:template match="operator">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="fqn"
		      select="concat('operator.', 
			             ancestor::database/@name, '.', 
			  	     @schema, '.', @name, '(', 
			  	     arg[@position='left']/@schema, '.', 
			  	     arg[@position='left']/@name, ',',
			  	     arg[@position='right']/@schema, '.', 
			  	     arg[@position='right']/@name, ')')"/>
      <xsl:with-param 
	  name="qname"
	  select="concat(skit:dbquote(@schema), '.',
		         @name, '(', 
			 skit:dbquote(arg[@position='left']/@schema,
			 	      arg[@position='left']/@name), ',',
			 skit:dbquote(arg[@position='right']/@schema,
				      arg[@position='right']/@name),
			 ')')"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="operator" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>
    <xsl:if test="arg[@position='left']/@schema != 'pg_catalog'">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
			       arg[@position='left']/@schema, '.', 
			       arg[@position='left']/@name)}"/>
    </xsl:if>
    <xsl:if test="arg[@position='right']/@schema != 'pg_catalog'">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
		               arg[@position='right']/@schema, '.', 
			       arg[@position='right']/@name)}"/>
    </xsl:if>
    <xsl:if test="result/@schema != 'pg_catalog'">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
		               result/@schema, '.', result/@name)}"/>
    </xsl:if>
    <xsl:if test="procedure/@schema != 'pg_catalog'">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
		               procedure/@signature)}"/>
    </xsl:if>
    <xsl:if test="restrict/@schema != 'pg_catalog'">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
		               restrict/@signature)}"/>
    </xsl:if>
    <xsl:if test="join/@schema != 'pg_catalog'">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
		               join/@signature)}"/>
    </xsl:if>

    <!-- Dependencies on other operator.  These are recorded as
         cascades_to, rather than as dependencies because they depend on
         each other and hence we would be creating a circular
         dependency.  Where dependencies are not mutual, cascading will
         occur to dependent objects.
    -->
    <xsl:if test="commutator">
      <related_to fqn="{concat('operator.', 
		        ancestor::database/@name, '.', 
			commutator/@schema, '.', commutator/@name, '(', 
			arg[@position='left']/@schema, '.', 
			arg[@position='left']/@name, ',',
			arg[@position='right']/@schema, '.', 
			arg[@position='right']/@name, ')')}"/>
    </xsl:if>
    <xsl:if test="negator">
      <related_to fqn="{concat('operator.', 
		        ancestor::database/@name, '.', 
			negator/@schema, '.', negator/@name, '(', 
			arg[@position='left']/@schema, '.', 
			arg[@position='left']/@name, ',',
			arg[@position='right']/@schema, '.', 
			arg[@position='right']/@name, ')')}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
