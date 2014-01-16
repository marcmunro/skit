<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Operator families - because they must be created by superusers, 
       we have enough special cases that the default dbobject mechanism
       is not used.  -->
  <xsl:template match="operator_family">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="operator_family_fqn" 
		  select="concat('operator_family.', 
			  ancestor::database/@name, '.', 
			  @schema, '.', @name, 
			  '(', @method, ')')"/>
    <xsl:variable name="owner" select="@owner"/>
    <!-- If the owner of the operator family is a superuser, this gets
	 set to "superuser". -->
    <xsl:variable name="owner_is_superuser"
		  select="//cluster/role[@name=$owner]/privilege[@priv='superuser']/@priv"/>
    <dbobject type="operator_family" fqn="{$operator_family_fqn}"
	      name="{@name}" qname="{skit:dbquote(@schema,@name)}"
	      parent="{concat(name(..), '.', $parent_core)}"
	      owner_is_superuser="{$owner_is_superuser}">

      <!-- Operator Families must be created by superusers, so the
           context stuff here is tricky.  -->
      <xsl:choose>
	<xsl:when test="$owner_is_superuser='superuser'">
	  <xsl:if test="@owner">
	    <context name="owner" value="{@owner}" 
		     default="{//cluster/@username}"/>	
	  </xsl:if>
	</xsl:when>
	<xsl:otherwise>
	    <context name="owner" value="{//cluster/@username}" 
		     default="{//cluster/@username}"/>	
	</xsl:otherwise>
      </xsl:choose>
      <dependencies>
	<dependency fqn="{concat('schema.', $parent_core)}"/>
	<!-- owner -->
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>

	<xsl:for-each select="opfamily_operator">
	  <xsl:call-template name="operator_dep"/>
	</xsl:for-each>

	<xsl:for-each select="opfamily_function">
	  <xsl:call-template name="operator_function_dep"/>
	</xsl:for-each>

	<xsl:call-template name="SchemaGrant"/>

        <!-- Depend on someone being a superuser - either owner or
	     the current user (or rather the one that ran the extract). --> 
	<dependency-set
	    fallback="{concat('privilege.cluster.', 
		              //cluster/@username, '.superuser')}"
	    parent="ancestor::dbobject[database]">
	  <dependency fqn="{concat('privilege.cluster.', 
		                   //cluster/@username, '.superuser')}"/>
	  <dependency fqn="{concat('privilege.cluster.', 
			           $owner, '.superuser')}"/>
	</dependency-set>
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

