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
       qualified name).  --> 
  <xsl:template match="role/grant">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="name" select="concat(@priv, ':', @to)"/>
      <xsl:with-param name="owner" select="@from"/>
      <xsl:with-param name="fqn" select="concat('grant.role.', 
					        @to, '.', @priv, ':', @from)"/>
      <xsl:with-param name="others">
	<param name="subtype" value="role"/>
	<param name="pqn" 
	       value="{concat('grant.role.', @to, '.', @priv)}"/>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="role/grant" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat(name(..), '.', $parent_core)}"/>
    <dependency fqn="{concat('role.', @priv)}"/>
    <xsl:if test="@priv != @from">
      <dependency fqn="{concat('role.', @from)}"/>
    </xsl:if>

    <!-- Dependencies on previous grant. -->
    <xsl:choose>
      <xsl:when test="@from=@priv">
	<!-- No dependency if the role is granted from the role -->
      </xsl:when>
      <xsl:when 
	  test="../../role[@name=@from]/privilege[@priv='superuser']">
	<!-- No dependency if the role is granted from a superuser -->
      </xsl:when>
      <xsl:otherwise>  
	<dependency pqn="{concat('grant.role.', @from, '.', @priv)}"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- These are grants of object privilege.  Note that the fqn may be
       in three forms: 
       1) For the owner of the object:
          grant.<object>.<privilege>
       2) For anyone else, granted by the owner
          grant.<object>.<privilege>:<grantee>
       3) For anyone else:
          grant.<object>.<privilege>:<grantee>.<grantor>

       This allows the fqns of grants from the owner to match when
       comparing objects for diffs.
  -->
  <xsl:template match="grant">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="name" select="concat(@priv, ':', @to)"/>
      <xsl:with-param name="owner" select="../@owner"/>
      <xsl:with-param name="fqn">
	<xsl:value-of select="concat('grant.', name(..), '.', 
			             $parent_core, '.', @priv)"/>

<!-- New code -->
	<xsl:if test="@to != @from">
	  <xsl:value-of select="concat(':', @to)"/>
	  <xsl:if test="@from != ../@owner">
	    <xsl:value-of select="concat(':', @from)"/>
	  </xsl:if>
	</xsl:if>
<!--
    Previous version of the code from above
	<xsl:if test="not(@automatic='yes' and @to = @from)">
	  <xsl:value-of 
	      select="concat(':', @to, ':', @from)"/>
	</xsl:if>
-->
      </xsl:with-param>
      <xsl:with-param name="others">
	<param name="pqn" 
	       value="{concat('grant.', name(..), '.', 
			      $parent_core, '.', @priv, ':', @to)}"/>
	<param name="subtype">
	  <xsl:attribute name="value">
	    <xsl:choose>
	      <xsl:when test="name(..)='sequence'">
		<xsl:value-of select="'table'"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:value-of select="name(..)"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:attribute>
	</param>
	<param name="on">
	  <xsl:attribute name="value">
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
	</param>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="grant" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat(name(..), '.', $parent_core)}"/>
    <!-- Roles -->
    <xsl:if test="@to != 'public'">
      <dependency fqn="{concat('role.', @to)}"/>
    </xsl:if>
    <xsl:if test="(@from != 'public') and (@to != @from)">
      <dependency fqn="{concat('role.', @from)}"/>
    </xsl:if>

    <!-- Dependencies on usage of schema. -->
    <xsl:if test="../@schema">
      <dependency-set
	  fallback="{concat('privilege.role.', @from, '.superuser')}"
	  parent="ancestor::dbobject[database]">
	    
	<xsl:call-template name="deps-schema-usage">
	  <xsl:with-param name="to" select="@from"/>
	</xsl:call-template>
	<xsl:call-template name="deps-schema-usage">
	  <xsl:with-param name="to" select="'public'"/>
	</xsl:call-template>
	<xsl:call-template name="deps-schema-create">
	  <xsl:with-param name="to" select="@from"/>
	</xsl:call-template>
	<dependency 
	    fqn="{concat('privilege.role.', @from, '.superuser')}"/>
      </dependency-set>
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
	<xsl:call-template name="pqn-dep">
	  <xsl:with-param name="to" select="@from"/>
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
    
    <!-- Special case for functions that are type handlers: in this
         case, the grant should also be dependent on the type for which
         the function is a handler.  This is because the function can
         only be dropped by dropping the type with cascade, so any
         revokation of grants to the function should be done before the
         type is dropped. -->
    <xsl:if test="parent::function/handler-for-type">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.',
		               parent::function/handler-for-type/@schema, '.',
			       parent::function/handler-for-type/@name)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>