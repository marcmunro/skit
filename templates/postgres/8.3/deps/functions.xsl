<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Functions -->
  <xsl:template match="function">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="function_fqn" 
		  select="concat('function.', 
			  ancestor::database/@name, '.', @signature)"/>
    <xsl:variable name="function_qname">
      <xsl:value-of select="concat(skit:dbquote(@schema, @name), '(')"/>
      <xsl:for-each select="params/param">
	<xsl:if test="position() != 1">
	  <xsl:value-of select="','"/>
	</xsl:if>
	<xsl:value-of select="skit:dbquote(@schema, @type)"/>
      </xsl:for-each>
      <xsl:value-of select="')'"/>
    </xsl:variable>
    <dbobject type="function" fqn="{$function_fqn}"
	      name="{@name}" qname="{$function_qname}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>

	<xsl:if test="(@language != 'c') and (@language != 'internal')
	  and (@language != 'sql')">
	  <dependency fqn="{concat('language.', 
			   ancestor::database/@name, '.', @language)}"/>
	</xsl:if>

	<xsl:for-each select="result">
	  <xsl:call-template name="TypeDep">
	    <xsl:with-param name="ignore" select="../handler-for-type"/>
	  </xsl:call-template>
	</xsl:for-each>

	<xsl:for-each select="params/param">
	  <xsl:call-template name="TypeDep">
	    <xsl:with-param name="ignore" select="../../handler-for-type"/>
	  </xsl:call-template>
	</xsl:for-each>

	<xsl:if test="handler-for-type[@type_input_signature]">
	  <dependency fqn="{concat('function.', 
			           ancestor::database/@name, '.',
				   handler-for-type/@type_input_signature)}"/>
	</xsl:if>
	<xsl:if test="handler-for-type[@type_output_signature]">
	  <dependency fqn="{concat('function.', 
			           ancestor::database/@name, '.',
				   handler-for-type/@type_output_signature)}"/>
	</xsl:if>
	<xsl:if test="handler-for-type[@type_send_signature]">
	  <dependency fqn="{concat('function.', 
			           ancestor::database/@name, '.',
				   handler-for-type/@type_send_signature)}"/>
	</xsl:if>
	<xsl:if test="handler-for-type[@type_receive_signature]">
	  <dependency fqn="{concat('function.', 
			           ancestor::database/@name, '.',
				   handler-for-type/@type_receive_signature)}"/>
	</xsl:if>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @signature)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>
</xsl:stylesheet>

