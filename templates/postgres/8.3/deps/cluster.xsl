<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

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
	<xsl:copy-of select="comment"/>
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

  <!-- Languages -->
  <xsl:template match="language">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('language.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="language" name="{@name}" qname='"{@name}"'
	      fqn="{$tbs_fqn}">
      <dependencies>
	<xsl:if test="@handler_signature">
	  <dependency fqn="{concat('function.', ancestor::database/@name, 
			   '.', @handler_signature)}"/>
	</xsl:if>
	<xsl:if test="@validator_signature">
	  <dependency fqn="{concat('function.', ancestor::database/@name,
			   '.',  @validator_signature)}"/>
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

  <!-- Schemata -->
  <xsl:template match="schema">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('language.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="schema" name="{@name}" qname='"{@name}"'
	      fqn="{$tbs_fqn}">
      <xsl:if test="@owner != 'public'">
	<dependencies>
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</dependencies>
      </xsl:if>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core"
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- Domains -->
  <xsl:template match="domain">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('language.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="domain" name="{@name}" qname='"{@name}"'
	      fqn="{$tbs_fqn}">
      <xsl:if test="(@basetype_schema != 'pg_toast') and
		    (@basetype_schema != 'pg_catalog') and
		    (@basetype_schema != 'information_schema')">
	<dependencies>
          <dependency fqn="{concat('type.', 
			   ancestor::database/@name, '.',
			   @basetype_schema, '.', @basetype)}"/>
	</dependencies>
      </xsl:if>

      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core"
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- Generate dependency for a given type, ignoring built-ins -->
  <xsl:template name="TypeDep">
    <xsl:param name="ignore" select="NONE"/>
    <xsl:param name="type_name" select="@type_name"/>
    <xsl:param name="type_schema" select="@type_schema"/>
    <xsl:if test="$type_schema != 'pg_catalog'"> 
      <!-- Ignore builtin types -->
      <xsl:choose>
	<xsl:when test="($ignore/@type_schema = $type_schema) and
	  ($ignore/@type_name = $type_name)"/>
	<xsl:otherwise>
	  <dependency fqn="{concat('type.', ancestor::database/@name,
			   '.', $type_schema, '.', $type_name)}"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <!-- Functions -->
  <xsl:template match="function">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="function_fqn" 
		  select="concat('function.', 
			  ancestor::database/@name, '.', @signature)"/>
    <dbobject type="function" fqn="{$function_fqn}"
	      name="{@name}" qname="{@qname}">
      <dependencies>

	<xsl:if test="(@language != 'c') and (@language != 'internal')
	  and (@language != 'sql')">
	  <dependency fqn="{concat('language.', 
			   ancestor::database/@name, '.', @language)}"/>
	</xsl:if>

	<xsl:for-each select="result">
	  <xsl:call-template name="TypeDep">
	    <xsl:with-param name="ignore" select="../handler_for"/>
	  </xsl:call-template>
	</xsl:for-each>

	<xsl:for-each select="handler_for[@following]">
	  <xsl:call-template name="FuncDep">
	    <xsl:with-param name="fqn" select="@following"/>
	    <xsl:with-param name="schema" select="@following_schema"/>
	  </xsl:call-template>
	</xsl:for-each>
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

  <!-- Types -->
  <xsl:template match="type">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="type_fqn" select="concat('type.', 
					    $parent_core, '.', @name)"/>
    <dbobject type="type" name="{@name}"
	      fqn="{$type_fqn}">
      <dependencies>
	<!-- Only have dependencies if this type is defined.  If it is
	not defined, it only exists as a placemarker and no code will be
	generated for it -->
	<xsl:if test="@is_defined != 'f'">
	  <!-- Dependencies on functions -->
	  <xsl:if test="@input_sig">
            <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', @input_sig)}"
			cascades="yes"/>
          </xsl:if>
          <xsl:if test="@output_sig">
            <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', @output_sig)}"
			cascades="yes"/>
          </xsl:if>
          <xsl:if test="@send_sig">
            <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', @send_sig)}"
			cascades="yes"/>
          </xsl:if>
          <xsl:if test="@receive_sig">
            <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', @receive_sig)}"
			cascades="yes"/>
          </xsl:if>
          <xsl:if test="@analyze_sig">
            <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', @analyze_sig)}"
			cascades="yes"/>
          </xsl:if>
  	</xsl:if>
	<xsl:for-each select="column">
          <xsl:if test="(@type_schema != 'pg_toast') and
			(@type_schema != 'pg_catalog') and
			(@type_schema != 'information_schema')">
            <dependency fqn="{concat('type.', 
			     ancestor::database/@name, '.',
			     @type_schema, '.', @type)}"/>
          </xsl:if>
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
