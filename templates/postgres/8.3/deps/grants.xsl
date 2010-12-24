<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Grants: grants to roles are performed at the cluster, rather than
       database level.  These grants can be differentiated from other
       grants by their dbobject having a subtype of "role".  Note that
       grants depend on all of the roles involved in the grant, and on
       any grants of the necessary role to the grantor for this grant.
       Because the dependencies on a grantor may potentially be
       satisfied by a number of grants, these dependencies are specified
       in terms of pqn (partially qualified name) rather than fqn (fully
       qualified name). --> 
 <xsl:template match="role/grant">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="grant_name" select="concat($parent_core, '.', @priv)"/>
    <xsl:variable name="grantor" select="@from"/>
    <dbobject type="grant" subtype="role" name="{concat(@priv, ':', @to)}"
	      pqn="{concat('grant.', $grant_name)}"
	      fqn="{concat('grant.', $grant_name, ':', @from)}">

      <context name="owner" value="{@from}" 
	       default="{//cluster/@username}"/>	
      <dependencies>
	<!-- Dependencies on roles from, to and priv -->
	<dependency fqn="{concat('role.cluster.', @priv)}"/>
	<dependency fqn="{concat('role.cluster.', @from)}"/>
	<dependency fqn="{concat('role.', $parent_core)}"/>

	<!-- Dependencies on previous grant. -->
	<xsl:choose>
	  <xsl:when test="@from=@priv">
	    <!-- No dependency if the role is granted from the role -->
	  </xsl:when>
	  <xsl:when 
	     test="../../role[@name=$grantor]/privilege[@priv='superuser']">
	    <!-- No dependency if the role is granted from a superuser -->
	  </xsl:when>
	  <xsl:otherwise>  
	    <dependency pqn="{concat('grant.cluster.', @from, '.', @priv)}"/>
	  </xsl:otherwise>
	</xsl:choose>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- DB object grants -->
  <!-- pqn format for this type of grant is: grant.<parent_name>.<priv>:<to>
       -->
  <xsl:template match="grant">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="grant_name" select="concat($parent_core, '.', 
					    @priv, ':', @to)"/>
    <xsl:variable name="grantor" select="@from"/>
    <!-- The owner attribute is needed when a grant is being done/revoked
	 from the owner and the owner has changed (in a diff). --> 
    <dbobject type="grant" 
	      parent="{concat(name(..), '.', $parent_core)}"
	      name="{concat(@priv, ':', @to)}"
	      pqn="{concat('grant.', $grant_name)}"
	      fqn="{concat('grant.', $grant_name, ':', @from)}"
	      owner="{../@owner}">
      <xsl:attribute name="subtype">
	<xsl:choose>
	  <xsl:when test="name(..)='sequence'">
	    <xsl:value-of select="'table'"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="name(..)"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:attribute>
      <xsl:attribute name="on">
	<xsl:choose>
	  <xsl:when test="name(..)='function'">
	    <xsl:for-each select="..">
	      <xsl:call-template name="function-qname"/>
	    </xsl:for-each>
	  </xsl:when>
	  <xsl:when test="../@schema">
	    <xsl:value-of select="skit:dbquote(../@schema, ../@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of select="skit:dbquote(../@name)"/>
	  </xsl:otherwise>
	</xsl:choose>	
      </xsl:attribute>

      <xsl:if test="@from">
	<context name="owner" value="{@from}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<!-- Roles -->
	<xsl:if test="@to != 'public'">
	  <dependency fqn="{concat('role.cluster.', @to)}"/>
	</xsl:if>
	<xsl:if test="@from != 'public'">
	  <dependency fqn="{concat('role.cluster.', @from)}"/>
	</xsl:if>

	<!-- Dependencies on usage of schema. -->
	<xsl:if test="../@schema">
	  <dependency pqn="{concat('grant.', ancestor::database/@name, '.',
		       ../@schema, '.usage:', @from)}"/>
	  <dependency pqn="{concat('grant.', ancestor::database/@name, '.',
		       ../@schema, '.usage:public')}"/>
	</xsl:if>
	<!-- Dependencies on previous grant. -->
	<xsl:choose>
	  <xsl:when test="@from=../@owner">
	    <!-- No dependency if the role is granted from the owner
		 of the object -->
	  </xsl:when>
	  <xsl:when 
	     test="//cluster/role[@name=$grantor]/privilege[@priv='superuser']">
	    <!-- No dependency if the role is granted from a superuser -->
	  </xsl:when>
	  <xsl:otherwise>  
	    <dependency pqn="{concat('grant.', $parent_core, '.', 
			     @priv, ':', @from)}"/>
	  </xsl:otherwise>
	</xsl:choose>

	<!-- Special case for functions that are type handlers: in this
	     case, the grant should also be dependent on the type for
	     which the function is a handler.  This is because the
	     function can only be dropped by dropping the type with
	     cascade, so any revokation of grants to the function should
	     be done before the type is dropped. -->
	<xsl:if test="parent::function/handler-for-type">
	  <dependency fqn="{concat('type.', ancestor::database/@name, '.',
			            parent::function/handler-for-type/@schema,
				    '.',
				    parent::function/handler-for-type/@name)}"/>
	</xsl:if>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>


</xsl:stylesheet>

