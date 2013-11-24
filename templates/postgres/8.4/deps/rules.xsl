<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- rules -->
  <xsl:template match="rule">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="rule_fqn" 
		  select="concat('rule.', $parent_core, '.', @name)"/>
    <dbobject type="rule" fqn="{$rule_fqn}" name="{@name}"
	      qname="{skit:dbquote(@name)}"
	      table_qname="{skit:dbquote(../@schema, ../@name)}"
	      parent="{concat(name(..), '.', $parent_core)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<dependency fqn="{concat(name(..), '.', $parent_core)}"/>
	<!-- Add explicitly identified dependencies -->
	<xsl:for-each select="depends[@function]">
	  <xsl:choose>
	    <xsl:when test="@cast">
	      <dependency fqn="{concat('cast.', 
			               ancestor::database/@name, 
				       '.', @cast)}"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <dependency fqn="{concat('function.', 
			                ancestor::database/@name, 
					'.', @function)}"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:for-each>
	<xsl:for-each select="depends[@table]">
	  <xsl:if test="(@schema != ../@schema) or 
			(@table != ../@table)">
	    <dependency fqn="{concat('table.', ancestor::database/@name, 
			      '.', @schema, '.', @table)}"/>
	  </xsl:if>
	</xsl:for-each>
	<xsl:call-template name="SchemaGrant">
	  <xsl:with-param name="owner" select="@table_owner"/>
	</xsl:call-template>
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

