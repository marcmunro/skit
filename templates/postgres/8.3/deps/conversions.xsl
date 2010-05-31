<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- conversions -->
  <xsl:template match="conversion">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="conversion_fqn" 
		  select="concat('conversion.', $parent_core, '.', @name)"/>
    <dbobject type="conversion" fqn="{$conversion_fqn}" name="{@name}"
	      qname="{skit:dbquote(@schema,@name)}">

      <xsl:if test="@owner">
	<context name="owner" value="{@owner}"/>	
      </xsl:if>
      <dependencies>
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
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
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

