<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Table grant dependencies -->
  <xsl:template name="TableGrant">
    <xsl:param name="owner" select="@owner"/>
    <xsl:param name="schema" select="@schema"/>
    <xsl:param name="table" select="@table"/>
    <xsl:param name="priv"/>
    <!-- Dependency on table usage grant to owner, public or self -->
    <dependency-set
	fallback="{concat('privilege.cluster.', $owner, 
		          '.superuser')}"
	parent="ancestor::dbobject[database]">
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
    </dependency-set>
  </xsl:template>

  <!-- Tables -->
  <xsl:template match="table">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="table" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
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

    <!-- Dependency on sequences -->
    <xsl:for-each select="depends[@schema]">
      <dependency fqn="{concat('sequence.', 
			       ancestor::database/@name, '.', 
			       @schema, '.', @sequence)}"/>
    </xsl:for-each>

    <!-- The table depends on all of its columns -->
    <xsl:for-each select="column">
      <dependency fqn="{concat('column.', $parent_core, '.', ../@name, 
		               '.', @name)}"/>

    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
