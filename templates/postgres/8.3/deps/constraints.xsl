<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Table (not type) constraints -->
  <xsl:template match="table/constraint">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="constraint_fqn" 
		  select="concat('constraint.', $parent_core, '.', @name)"/>
    <dbobject type="constraint" fqn="{$constraint_fqn}" name="{@name}"
	      qname="{skit:dbquote(@name)}"
	      table_qname="{skit:dbquote(../@schema, ../@name)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<!-- Dependencies on other constraints -->
	<xsl:if test="reftable/@refconstraintname">
	  <dependency fqn="{concat('constraint.', 
			           ancestor::database/@name, '.',
				   reftable/@refschema, '.',
				   reftable/@reftable, '.',
				   reftable/@refconstraintname)}"/>
	</xsl:if>
	<!-- Add explicitly identified dependencies -->
	<xsl:for-each select="depends">
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

