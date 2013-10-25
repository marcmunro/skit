<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Operator families -->
  <xsl:template match="operator_family">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="operator_family_fqn" 
		  select="concat('operator_family.', 
			  ancestor::database/@name, '.', 
			  @schema, '.', @name, 
			  '(', @method, ')')"/>
    <dbobject type="operator_family" fqn="{$operator_family_fqn}"
	      name="{@name}" qname="{skit:dbquote(@schema,@name)}"
	      parent="{concat(name(..), '.', $parent_core)}">
      <xsl:if test="@owner">
	<context name="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<dependency fqn="{concat('schema.', $parent_core)}"/>
	<!-- owner -->
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>

	<xsl:call-template name="operator_deps"/>
	<xsl:call-template name="operator_function_deps"/>
	<xsl:call-template name="SchemaGrant"/>
      </dependencies>
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @signature)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>

    <xsl:if test="@auto_generated='t'">
      <xsl:if test="comment">
	<!-- If the operator family was auto-created, we must create the
	     comment only after the operator class has been created. -->
	<xsl:variable name="comment_fqn" 
		      select="concat('comment.', 
			             ancestor::database/@name, '.', 
				     @schema, '.', @name, 
				     '(', @method, ')')"/>
	<dbobject type="comment" fqn="{$comment_fqn}"
	          name="{@name}" qname="{skit:dbquote(@schema,@name)}"
		  nolist="true" method="{@method}"
		  parent="{concat(name(..), '.', $parent_core)}">
	  <dependencies>
	    <dependency fqn="{concat('schema.', $parent_core)}"/>
	    <dependency fqn="{concat('operator_class.', $parent_core,
			      '.', @name, '(', @method, ')')}"/>
	  </dependencies>
	  <xsl:for-each select="comment">
	    <xsl:copy>
	      <xsl:copy-of select="text()"/>
	    </xsl:copy>
	  </xsl:for-each>
	</dbobject>
      </xsl:if>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

