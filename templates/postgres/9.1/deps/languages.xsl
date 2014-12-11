<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Languages -->
  <xsl:template match="language">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="language" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('database.', $parent_core)}"/>
    <xsl:if test="@handler_signature">
      <dependency fqn="{concat('function.', ancestor::database/@name, 
		               '.', @handler_signature)}"/>
    </xsl:if>
    <xsl:if test="@validator_signature">
      <dependency fqn="{concat('function.', ancestor::database/@name,
			       '.',  @validator_signature)}"/>
    </xsl:if>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>

    <!-- Must be a superuser or (directly) the owner of the database. -->
    <dependency-set
	fallback="{concat('privilege.role.', @owner, '.superuser')}"
	parent="ancestor::dbobject[database]" >
	<dependency fqn="{concat('privilege.role.', @owner, '.superuser')}"/>
	<xsl:if test="@owner=ancestor::database/@owner">
	  <!-- This dependency is pretty easy to satisfy! -->
	  <dependency fqn="{concat('role.', @owner)}"/>
	</xsl:if>
    </dependency-set>
  </xsl:template>
</xsl:stylesheet>

