<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="depends">
    <!-- Add explicitly identified dependencies -->
    <xsl:for-each select="depends">
      <xsl:choose>
	<xsl:when test="@operator_class">
	  <dependency fqn="{concat('operator_class.', 
			           ancestor::database/@name,
			           '.', @operator_class)}"/>
	</xsl:when>
	<xsl:when test="@cast">
	  <dependency fqn="{concat('cast.', 
			   ancestor::database/@name, 
			   '.', @cast)}"/>
	</xsl:when>	
	<xsl:when test="@function">
	  <dependency fqn="{concat('function.', ancestor::database/@name,
			        '.', @function)}"/>
	</xsl:when>
	<xsl:when test="@column">
	  <dependency fqn="{concat('column.', 
			   ancestor::database/@name, '.', 
			   ancestor::schema/@name, '.',
			   ancestor::table/@name, '.', @column)}"/>
	</xsl:when>	
	<xsl:otherwise>
	  <UNHANDLED_DEPENDS_NODE>
	    <xsl:copy-of select="@*"/>
	  </UNHANDLED_DEPENDS_NODE>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>

  <!-- Indices -->
  <xsl:template match="index">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="index_fqn" 
		  select="concat('index.', $parent_core, '.', @name)"/>
    <dbobject type="index" fqn="{$index_fqn}" name="{@name}"
	      qname="{skit:dbquote(../@schema, @name)}"
	      parent="{concat(name(..), '.', $parent_core)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<dependency fqn="{concat('table.', $parent_core)}"/>
	<xsl:if test="@tablespace">
	  <dependency fqn="{concat('tablespace.cluster.', @tablespace)}"/>
	</xsl:if>
	<xsl:for-each select="reftable[@refschema != 'pg_catalog']">
	  <dependency fqn="{concat('table.', ancestor::database/@name, '.', 
			           @refschema, '.', @reftable)}"/>
	  
	</xsl:for-each>
	<xsl:call-template name="depends"/>
	<xsl:call-template name="SchemaGrant"/>
      </dependencies>
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>

  </xsl:template>
</xsl:stylesheet>

