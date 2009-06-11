<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">

  <!-- This stylesheet adds dependency definitions to dbobjects unless
       they appear to already exist. The manual indentation provided
       by xsl_text elements below is intended to be just good enough to
       enable the output from add_deps to be readable.  It is not
       intended to be perfect or even close. -->

  <xsl:template match="/*">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:choose>
	<xsl:when test="//dbobject/dependencies">
	  <xsl:apply-templates mode="copy"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates/>
	  <xsl:apply-templates select="//database" mode="database"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
  </xsl:template>

  <!-- This template handles copy-only mode.  This is used when we
       discover that a document already has dependencies defined -->
  <xsl:template match="*" mode="copy">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy"/>
    </xsl:copy>
  </xsl:template>

  <!-- Anything not matched explicitly will match this and be copied 
       This handles db objects, dependencies, etc -->
  <xsl:template match="*">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
	<xsl:with-param name="parent_core" select="$parent_core"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>

  <!-- Cluster and database:
       Handle the database and the cluster objects.  The cluster creates
       an interesting problem: the database can only be created from
       within a connection to a different database within the cluster.
       So, to create a database we must visit the cluster.  To create a
       database object, we must visit the database.  But in visiting the
       database, we do not want to visit the cluster.  To solve this, we
       create two distinct database objects, one within the cluster for
       db creation purposes, and one not within the cluster which
       depends on the first.  The first object we will call dbincluster,
       the second will be database.  -->

  <xsl:template match="cluster">
    <dbobject type="cluster" visit="true"
	      name="cluster" fqn="cluster">
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="'cluster'"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- The dbincluster object (responsible for actual database creation) -->
  <xsl:template match="database">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="fqn" select="concat('dbincluster.', 
				     $parent_core, '.', @name)"/>
    <dbobject type="dbincluster" name="{@name}" qname='"{@name}"' fqn="{$fqn}">
      <dependencies>
	<xsl:if test="@tablespace != 'pg_default'">
	  <dependency fqn="{concat('tablespace.', $parent_core, 
			   '.', @tablespace)}"/>
	</xsl:if>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.', $parent_core, 
			   '.', @owner)}"/>
	</xsl:if>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- The database object, from which other database objects may be
       manipulated.  This is the only template called with a mode of
       "database" -->
  <xsl:template match="database" mode="database">
    <dbobject type="database" visit="true" name="{@name}" qname='"{@name}"' 
	      fqn="{concat('database.cluster.', @name)}">
      <dependencies>
	<dependency fqn="{concat('dbincluster.cluster.', @name)}"/>
      </dependencies>
      <database name="{@name}">
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
      </database>
    </dbobject>
  </xsl:template>

  <!-- Roles: roles are cluster level objects. -->
  <xsl:template match="role">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="role_name" select="concat($parent_core, '.', @name)"/>
    <dbobject type="role" name="{@name}" qname='"{@name}"'
	      fqn="{concat('role.', $role_name)}">
      <dependencies>
	<dependency fqn="cluster"/>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="$role_name"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

 <!-- Tablespaces -->
  <xsl:template match="tablespace">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('tablespace.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="tablespace" name="{@name}" qname='"{@name}"'
	      fqn="{$tbs_fqn}">
      <dependencies>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.', $parent_core, '.',
			   @owner)}"/>
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
	      qname='"{concat(@priv, ":", @to)}"'
	      pqn="{concat('grant.', $grant_name)}"
	      fqn="{concat('grant.', $grant_name, ':', @from)}">

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
	      qname='"{concat(@priv, ":", @to)}"'
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
	  <xsl:when test="../@qname">
	    <xsl:value-of select="../@qname"/>
	  </xsl:when>	
	  <xsl:otherwise>
	    <xsl:value-of select="concat('&quot;', ../@name, '&quot;')"/>
	  </xsl:otherwise>
	</xsl:choose>	
      </xsl:attribute>

      <dependencies>
	<!-- Roles -->
	<xsl:if test="@to != 'public'">
	  <dependency fqn="{concat('role.cluster.', @to)}"/>
	</xsl:if>
	<xsl:if test="@from != 'public'">
	  <dependency fqn="{concat('role.cluster.', @from)}"/>
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

<!-- Keep this comment at the end of the file
Local variables:
mode: xml
sgml-omittag:nil
sgml-shorttag:nil
sgml-namecase-general:nil
sgml-general-insert-case:lower
sgml-minimize-attributes:nil
sgml-always-quote-attributes:t
sgml-indent-step:2
sgml-indent-data:t
sgml-parent-document:nil
sgml-exposed-tags:nil
sgml-local-catalogs:nil
sgml-local-ecat-files:nil
End:
-->
