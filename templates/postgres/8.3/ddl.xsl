<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <!-- Anything not matched explicitly will match this and be copied 
       This handles dbobject, dependencies, etc -->
  <xsl:template match="*">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <!-- Template for dealing with comments.  This is invoked simply by
       using xsl:apply-templates from within the template for the 
       current dbobject -->
  
  <xsl:template match="comment">
    <xsl:text>&#x0A;comment on </xsl:text>
    <xsl:choose>
      <xsl:when test="contains(name(..), '_')">
	<xsl:value-of select="substring-before(name(..), '_')" />
	<xsl:text> </xsl:text>
	<xsl:value-of select="substring-after(name(..), '_')" />
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="name(..)"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> </xsl:text>
    <xsl:choose>
      <xsl:when test="(name(..) = 'constraint') or (name(..) = 'trigger')">
	<xsl:value-of select="skit:dbquote(../@name)"/>
	<xsl:text> on </xsl:text>
	<xsl:value-of select="../../@table_qname"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="../../@qname"/>
	<xsl:if test="../@method">
	  <xsl:text> using </xsl:text>
	  <xsl:value-of select="../@method"/>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text> is&#x0A;</xsl:text>
    <xsl:value-of select="text()"/>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template name="set_owner">
    <xsl:if test="@owner != //cluster/@username">
      <xsl:text>set session authorization &apos;</xsl:text>
      <xsl:value-of select="@owner"/>
      <xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template name="reset_owner">
    <xsl:if test="@owner != //cluster/@username">
      <xsl:text>reset session authorization;&#x0A;</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:include href="skitfile:ddl/owner.xsl"/>
  <xsl:include href="skitfile:ddl/cluster.xsl"/>
  <xsl:include href="skitfile:ddl/database.xsl"/>
  <xsl:include href="skitfile:ddl/tablespace.xsl"/>
  <xsl:include href="skitfile:ddl/roles.xsl"/>
  <xsl:include href="skitfile:ddl/grants.xsl"/>
  <xsl:include href="skitfile:ddl/languages.xsl"/>
  <xsl:include href="skitfile:ddl/schemata.xsl"/>
  <xsl:include href="skitfile:ddl/domains.xsl"/>
  <xsl:include href="skitfile:ddl/types.xsl"/>
  <xsl:include href="skitfile:ddl/functions.xsl"/>
  <xsl:include href="skitfile:ddl/aggregates.xsl"/>
  <xsl:include href="skitfile:ddl/casts.xsl"/>
  <xsl:include href="skitfile:ddl/operators.xsl"/>
  <xsl:include href="skitfile:ddl/operator_classes.xsl"/>
  <xsl:include href="skitfile:ddl/operator_families.xsl"/>
  <xsl:include href="skitfile:ddl/comments.xsl"/>
  <xsl:include href="skitfile:ddl/sequences.xsl"/>
  <xsl:include href="skitfile:ddl/tables.xsl"/>
  <xsl:include href="skitfile:ddl/constraints.xsl"/>
  <xsl:include href="skitfile:ddl/indices.xsl"/>
  <xsl:include href="skitfile:ddl/triggers.xsl"/>
  <xsl:include href="skitfile:ddl/rules.xsl"/>
  <xsl:include href="skitfile:ddl/views.xsl"/>
  <xsl:include href="skitfile:ddl/conversions.xsl"/>
</xsl:stylesheet>
