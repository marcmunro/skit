<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Functions -->
  <xsl:template name="function-qname">
    <xsl:value-of select="concat(skit:dbquote(@schema, @name), '(')"/>
    <xsl:for-each select="params/param">
      <xsl:if test="@mode != 'o'">
	<!-- Check whether there is a preceding in or inout
	     parameter.  If so, we need a comma. -->
	<xsl:if test="preceding-sibling::node()[@mode != 'o']">
	  <xsl:value-of select="','"/>
	</xsl:if>
	<xsl:value-of select="skit:dbquote(@schema, @type)"/>
	<xsl:if test="@array">
	  <xsl:value-of select="'[]'"/>
	</xsl:if>
      </xsl:if>
    </xsl:for-each>
    <xsl:value-of select="')'"/>
  </xsl:template>

  <xsl:template match="function">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:variable name="function_qname">
      <xsl:call-template name="function-qname"/>
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

  <xsl:template match="function" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>
    <xsl:if test="@leakproof">
      <dependency-set 
	  fallback="{concat('privilege.role.', @owner, '.superuser')}"
	  parent="//dbobject[database]">
	<dependency fqn="{concat('privilege.role.', @owner, '.superuser')}"/>
      </dependency-set>
    </xsl:if>
    <xsl:if test="(@language != 'c') and (@language != 'internal')
	           and (@language != 'sql')">
      <dependency fqn="{concat('language.', ancestor::database/@name, 
		               '.', @language)}"/>
    </xsl:if>

    <xsl:for-each select="result">
      <xsl:call-template name="TypeDep">
	<xsl:with-param name="handler_for" select="../handler-for-type"/>
      </xsl:call-template>
    </xsl:for-each>

    <xsl:for-each select="params/param">
      <xsl:call-template name="TypeDep">
	<xsl:with-param name="handler_for" select="../../handler-for-type"/>
      </xsl:call-template>
    </xsl:for-each>

    <xsl:if test="handler-for-type[@type_input_signature]">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.',
			       handler-for-type/@type_input_signature)}"/>
    </xsl:if>
    <xsl:if test="handler-for-type[@type_output_signature]">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.',
		                handler-for-type/@type_output_signature)}"/>
    </xsl:if>
    <xsl:if test="handler-for-type[@type_send_signature]">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.',
		                handler-for-type/@type_send_signature)}"/>
    </xsl:if>
    <xsl:if test="handler-for-type[@type_receive_signature]">
      <dependency fqn="{concat('function.', ancestor::database/@name, '.',
			       handler-for-type/@type_receive_signature)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

