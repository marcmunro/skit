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
    <dependency-set priority="1"
	fallback="{concat('privilege.role.', $owner, '.superuser')}"
	parent="ancestor::dbobject[database]">
      <xsl:call-template name="schema-deps">
	<xsl:with-param name="to" select="'public'"/>
      	<xsl:with-param name="priv" select="$priv"/>
      </xsl:call-template>

      <xsl:if test="$owner">
	<xsl:call-template name="schema-deps">
	  <xsl:with-param name="to" select="$owner"/>
	  <xsl:with-param name="priv" select="$priv"/>
	</xsl:call-template>
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
      <dependency fqn="{concat('tablespace.', @tablespace)}"/>
    </xsl:if>
    <xsl:for-each select="constraint[@tablespace]">
      <xsl:if test="@tablespace != ../@tablespace">
	<dependency fqn="{concat('tablespace.', @tablespace)}"/>
      </xsl:if>
    </xsl:for-each>
    <xsl:for-each select="index[@tablespace]">
      <xsl:if test="@tablespace != ../@tablespace">
	<dependency fqn="{concat('tablespace.', @tablespace)}"/>
      </xsl:if>
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

    <!-- The table depends, softly, on all of its columns -->
    <xsl:for-each select="column">
      <dependency fqn="{concat('column.', $parent_core, '.', ../@name, 
		               '.', @name)}"
		  soft="'true'"/>

    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
