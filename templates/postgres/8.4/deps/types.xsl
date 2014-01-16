<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Generate a dependency for a given type, ignoring built-ins -->
  <xsl:template name="TypeDep">
    <xsl:param name="ignore" select="NONE"/>
    <xsl:param name="type_name" select="@type"/>
    <xsl:param name="type_schema" select="@schema"/>
    <xsl:if test="$type_schema != 'pg_catalog'"> 
      <!-- Ignore builtin types -->
      <xsl:if test="not(($ignore/@schema = $type_schema) and
	  ($ignore/@name = $type_name))">
	  <dependency fqn="{concat('type.', ancestor::database/@name,
			   '.', $type_schema, '.', $type_name)}"/>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <!-- Domains -->
  <xsl:template match="domain">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="fqn" 
		      select="concat('type.', $parent_core, '.', @name)"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="domain" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>

    <xsl:if test="(@basetype_schema != 'pg_toast') and
		   (@basetype_schema != 'pg_catalog') and
		   (@basetype_schema != 'information_schema')">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.',
			@basetype_schema, '.', @basetype)}"/>
    </xsl:if>
  </xsl:template>


  <!-- Types -->
  <xsl:template match="type">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="type" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:for-each select="handler-function">
      <dependency fqn="{concat('function.', ancestor::database/@name, 
			       '.', @signature)}"/>
    </xsl:for-each>

    <xsl:for-each select="column">
      <xsl:if test="(@type_schema != 'pg_toast') and
		    (@type_schema != 'pg_catalog') and
		    (@type_schema != 'information_schema')">
	<dependency fqn="{concat('type.', ancestor::database/@name, '.',
			  @type_schema, '.', @type)}"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>


