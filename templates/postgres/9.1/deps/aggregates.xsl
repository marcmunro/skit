<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="aggregate">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:variable name="function_qname">
      <xsl:value-of select="concat(skit:dbquote(@schema, @name), '(')"/>
      <xsl:choose>
	<xsl:when test="basetype/@schema">
	  <xsl:value-of select="skit:dbquote(basetype/@schema, 
				             basetype/@name)"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="'*'"/>
	</xsl:otherwise>
      </xsl:choose>
      <xsl:value-of select="')'"/>
    </xsl:variable>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="fqn"
		      select="concat('function.', ancestor::database/@name, 
			             '.', @signature)"/>
      <xsl:with-param name="qname" select="$function_qname"/>
      <xsl:with-param name="this_core" 
		      select="concat(ancestor::database/@name, '.', 
				     @signature)"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="aggregate" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>
    <!-- base type -->
    <xsl:if test="basetype[@schema != 'pg_catalog']">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
		       basetype/@schema, '.', basetype/@name)}"/>
    </xsl:if>
    <!-- trans type -->
    <xsl:if test="transtype[@schema != 'pg_catalog']">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.', 
		               transtype/@schema, '.', transtype/@name)}"/>
    </xsl:if>
    <!-- trans func -->
    <xsl:if test="transfunc[@schema != 'pg_catalog']">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
		               transfunc/@schema, '.', transfunc/@name, '(',
			       transtype/@schema, '.', transtype/@name, ',',
			       transtype/@schema, '.', transtype/@name, ')')}"/>
    </xsl:if>
    <!-- sort op -->
    <xsl:if test="sortop[@schema != 'pg_catalog']">
      <dependency fqn="{concat('operator.', ancestor::database/@name, '.', 
		               sortop/@schema, '.',sortop/@name, '(',
			       transtype/@schema, '.', transtype/@name, ',',
			       transtype/@schema, '.', transtype/@name, ')')}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
