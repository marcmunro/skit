<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Languages -->
  <xsl:template match="language">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('language.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="language" name="{@name}" qname="{skit:dbquote(@name)}"
	      fqn="{$tbs_fqn}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<dependency fqn="{concat('database.', $parent_core)}"/>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
	<xsl:if test="@handler_signature">
	  <dependency fqn="{concat('function.', ancestor::database/@name, 
			   '.', @handler_signature)}"/>
	</xsl:if>
	<xsl:if test="@validator_signature">
	  <dependency fqn="{concat('function.', ancestor::database/@name,
			   '.',  @validator_signature)}"/>
	</xsl:if>
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

