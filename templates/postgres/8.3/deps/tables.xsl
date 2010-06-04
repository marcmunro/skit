<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Schema grant dependencies -->
  <xsl:template name="SchemaGrant">
    <xsl:param name="owner" select="@owner"/>
    <!-- Dependency on schema usage grant to owner, public or self -->
    <dependency pqn="{concat('grant.', 
		     ancestor::database/@name, '.', 
		     ancestor::schema/@name, '.usage:public')}"/>
    <dependency pqn="{concat('grant.', 
		     ancestor::database/@name, '.', 
		     ancestor::schema/@name, '.usage:', 
		     //cluster/@username)}"/>
    <xsl:if test="$owner">
      <dependency pqn="{concat('grant.', 
		       ancestor::database/@name, '.', 
		       ancestor::schema/@name, '.usage:', $owner)}"/>
    </xsl:if>
  </xsl:template>

  <!-- Table grant dependencies -->
  <xsl:template name="TableGrant">
    <xsl:param name="owner" select="@owner"/>
    <xsl:param name="schema" select="@schema"/>
    <xsl:param name="table" select="@table"/>
    <xsl:param name="priv"/>
    <!-- Dependency on table usage grant to owner, public or self -->
    <dependency pqn="{concat('grant.', 
		     ancestor::database/@name, '.', 
		     $schema, '.', $table, '.', $priv, ':public')}"/>

    <dependency pqn="{concat('grant.', 
		     ancestor::database/@name, '.', 
		     $schema, '.', $table, '.', $priv, ':', 
		     //cluster/@username)}"/>
    <xsl:if test="$owner">
      <dependency pqn="{concat('grant.', 
		     ancestor::database/@name, '.', 
		     $schema, '.', $table, '.', $priv, ':', $owner)}"/>
    </xsl:if>
  </xsl:template>

  <!-- Tables -->
  <xsl:template match="table">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="table_fqn" 
		  select="concat('table.', 
			  ancestor::database/@name, '.', 
			  ancestor::schema/@name, '.', @name)"/>
    <dbobject type="table" fqn="{$table_fqn}" name="{@name}"
	      qname="{skit:dbquote(@schema,@name)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
	<xsl:if test="@tablespace">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:if>
	<xsl:for-each select="constraint[@tablespace]">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:for-each>
	<xsl:for-each select="index[@tablespace]">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:for-each>
	<!-- Dependencies on inherited tables -->
	<xsl:for-each select="inherits">
	  <dependency fqn="{concat('table.', 
			           ancestor::database/@name, '.',
				   @schema, '.', @name)}"/>
	</xsl:for-each>
	<!-- Dependencies on types for columns -->
	<xsl:for-each select="column[@type_schema != 'pg_catalog']">
	  <dependency fqn="{concat('type.', 
			    ancestor::database/@name, '.', 
			    @type_schema, '.', @type)}"/>
	</xsl:for-each>

	<xsl:call-template name="SchemaGrant"/>
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

