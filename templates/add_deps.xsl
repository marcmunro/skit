<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  version="1.0">

  <!-- This stylesheet adds dependency definitions to dbobjects unless
       they appear to already exist. -->

  <xsl:template match="/">
    <skit:stylesheet>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:choose>
	  <xsl:when test="//dbobject/dependencies">
	    <xsl:apply-templates mode="copy"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:apply-templates/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:copy>
    </skit:stylesheet>
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


  <!-- Handle the database and the cluster objects.  The cluster creates
  an interesting problem: the database can only be created from within a
  connection to a different database within the cluster.  So, to create
  a database we must visit the cluster.  To create a database object, we
  must visit the database.  But in visiting the database, we do not want
  to visit the cluster.  To solve this, we create two distinct database
  objects, one within the cluster for db creation purposes, and one not
  within the cluster which depends on the first.  The first object we
  will call dbincluster. -->

  <xsl:template match="cluster">
    <dbobject type="cluster" root="true" visit="true"
	      name="{@name}" fqn="{concat('cluster.', @name)}"
	      qname='"{@name}"'>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>
      
  <xsl:template match="database">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="db_fqn" select="concat('database.', 
					  $parent_core, '.', @name)"/>
    <xsl:variable name="dbv_fqn" select="concat('db_visit.', @name)"/>

    <!-- Create the database object.  This is the object responsible for
      database creation, etc -->
    <dbobject type="database" visit="true"
	      name="{@name}" qname='"{@name}"' fqn="{$db_fqn}">
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

    <!-- now define the database visit object.  All non-cluster objects
      will be children of this object.  This object exists purely to
      encapsulate the code for visiting the database -->
    <dbobject root="true" type="db_visit" visit="true"
	      name="{@name}" qname='"{@name}"' fqn="{$dbv_fqn}">
      <dependencies>
	<dependency fqn="{$db_fqn}"/>
      </dependencies>
      <db_visit name="{@name}">
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
      </db_visit>
    </dbobject>

  </xsl:template>
  
  
  <!-- Roles are cluster level objects. -->

  <xsl:template match="role">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="role_name" select="concat($parent_core, '.', @name)"/>
    <dbobject type="role" name="{@name}" qname='"{@name}"'
	      fqn="{concat('role.', $role_name)}">
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="$role_name"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- Grants to roles are performed at the cluster, rather than
  database level.  These grants can be differentiated from other grants
  by their dbobject having a subtype of "role".  Note that grants
  depend on all of the roles involved in the grant, and on any grants of
  the necessary role to the grantor for this grant.  Because the
  dependencies on a grantor may poentially be satisfied by a number of
  grants, these dependencies are specified in terms of pqn (partially
  qualified name) rather than fqn (fully qualified name). -->

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
	<dependency fqn="{concat('role.', ../../@name, '.', @priv)}"/>
	<dependency fqn="{concat('role.', ../../@name, '.', @from)}"/>
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
	    <dependency pqn="{concat('grant.', 
			     ../../@name, '.', @from, '.', @priv)}"/>
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
	  <dependency fqn="{concat('role.',
			   ancestor::cluster/@name, '.', @to)}"/>
	</xsl:if>
	<xsl:if test="@from != 'public'">
	  <dependency fqn="{concat('role.',
			   ancestor::cluster/@name, '.', @from)}"/>
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

  <!-- Schemata -->
  <xsl:template match="schema">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="schema_fqn" select="concat('schema.', 
					    $parent_core, '.', @name)"/>
    <dbobject type="schema" visit="true" name="{@name}" qname='"{@name}"'
	      fqn="{$schema_fqn}">
      <dependencies>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.',
			   ancestor::cluster/@name, '.', @owner)}"/>
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
    <!-- This is a dummy object that depends on the schema and all of
	 its grants.  Dbobjects that operate within the schema should be
	 dependent on this rather than on the schema, to ensure that the
	 creator account has the necessary rights within the schema. -->
    <dbobject type="schema_access" visit="true" name="{@name}" qname='"{@name}"'
	      fqn="{concat('schema_access.', $parent_core, '.', @name)}">
      
      <schema_access name="{@name}"/>
      <dependencies>
	<!-- Dependency on the schema -->
	<dependency fqn="{$schema_fqn}"/>
	<!-- Dependencies on each grant -->
	<xsl:for-each select="grant">
	  <dependency fqn="{concat('grant.', $parent_core, '.', ../@name, 
			   '.', @priv, ':', @to, ':', @from)}"/>
	</xsl:for-each>
      </dependencies>
    </dbobject>
  </xsl:template>


  <xsl:template match="cast">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="cast_fqn" select="concat('cast.', 
					      $parent_core, '.', @name)"/>
    <dbobject type="cast" name="{@name}" qname='"{@name}"'
	      fqn="{$cast_fqn}">
      <dependencies>
	<xsl:if test="@source_type_schema != 'pg_catalog'">
	  <dependency fqn="{concat('type.', $parent_core, '.', 
			   @source_type_schema, '.', @source_type)}"/>
	</xsl:if>
	<xsl:if test="@target_type_schema != 'pg_catalog'">
	  <dependency fqn="{concat('type.', $parent_core, '.', 
			   @target_type_schema, '.', @target_type)}"/>
	</xsl:if>
	<xsl:if test="@fn_schema != 'pg_catalog'">
	  <dependency fqn="{concat('function.', $parent_core, '.', @fn_sig)}"/>
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

  <!-- (Procedural) Languages -->
  <xsl:template match="language">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="language_fqn" select="concat('language.', 
					      $parent_core, '.', @name)"/>
    <dbobject type="language" name="{@name}" qname='"{@name}"'
	      fqn="{$language_fqn}">
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

  <!-- Operators-->
  <xsl:template match="operator">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="operator_fqn" select="concat('operator.', 
					      ancestor::database/@name, '.', 
					      @signature)"/>
    <dbobject type="operator" fqn="{$operator_fqn}"
	      name="{@name}" qname='"{@name}"'>
      <dependencies>
	<!-- Dependencies on other operator.  These are recorded as
	     cascades_to, rather than as dependencies because they
	     depend on each other and hence we would be creating a
	     circular dependency.  Where dependencies are not mutual, 
	     cascading will occur to dependent objects.
	-->
	<xsl:if test="@commutator">
	  <cascades_to fqn="{concat('operator.', 
			    ancestor::database/@name, '.', 
			    @commutator_schema, '.',
			    @commutator, @params)}"/>
	</xsl:if>
	<xsl:if test="@negator">
	  <cascades_to fqn="{concat('operator.', 
			    ancestor::database/@name, '.', 
			    @negator_schema, '.',
			    @negator, @params)}"/>
	</xsl:if>
	<!-- Dependencies on parameter and result types -->
	<xsl:call-template name="TypeDep">
	  <xsl:with-param name="type_name" select="@leftarg"/>
	  <xsl:with-param name="type_schema" select="@leftarg_schema"/>
	</xsl:call-template>
	<xsl:call-template name="TypeDep">
	  <xsl:with-param name="type_name" select="@rightarg"/>
	  <xsl:with-param name="type_schema" select="@rightarg_schema"/>
	</xsl:call-template>
	<xsl:call-template name="TypeDep">
	  <xsl:with-param name="type_name" select="@result"/>
	  <xsl:with-param name="type_schema" select="@result_schema"/>
	</xsl:call-template>
	<!-- Dependencies on functions -->

	<xsl:if test="self::*[@procedure_schema!='pg_catalog']">
	  <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
			   @procedure_signature)}"/>
	</xsl:if>
	<xsl:if test="self::*[@restrict_proc_schema!='pg_catalog']">
	  <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
			   @restrict_signature)}"/>
	</xsl:if>
	<xsl:if test="self::*[@join_proc_schema!='pg_catalog']">
	  <dependency fqn="{concat('function.', ancestor::database/@name, '.', 
			   @join_signature)}"/>
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

  <xsl:template match="operator_class">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="opclass_fqn" select="concat('operator_class.', 
					     $parent_core, '.', 
					     @signature)"/>
    <dbobject type="operator_class"
	      fqn="{$opclass_fqn}" name="{@name}" qname='"{@name}"'>
      <dependencies>
	<!-- Dependency on types -->
	<dependency fqn="{concat('type.', ancestor::database/@name,
			 '.', @intype_schema, '.', @intype_name)}"/>
	<xsl:if test="(@intype_schema != @keytype_schema) or
			(@intype_name != @keytype_name)">
	  <dependency fqn="{concat('type.', ancestor::database/@name,
			   '.', @keytype_schema, '.', @keytype_name)}"/>
	</xsl:if>
	<!-- Dependencies on operators -->
	<xsl:for-each select="opclass_oper">
	  <dependency fqn="{concat('operator.', 
			   ancestor::database/@name, '.', 
			   @op_schema, '.', @op_name, '(',
			   @op_leftarg_schema, '.', @op_leftarg_name,
			   ',', @op_rightarg_schema, '.', 
			   @op_rightarg_name, ')')}"/>
	</xsl:for-each>
	<!-- Dependencies on functions -->
	<xsl:for-each select="opclass_func">
	  <dependency fqn="{concat('function.', 
			   ancestor::database/@name, '.', 
			   @function_signature)}"/>
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


  <xsl:template match="aggregate">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="aggregate_fqn" 
		  select="concat('aggregate.', $parent_core, '.', 
			  @name, '(', @base_type_sig, ')')"/>
    <dbobject type="aggregate" fqn="{$aggregate_fqn}" name="{@name}" 
	      qname='"{@name}"("{@base_type_schema}"."{@base_type}")'>
      <dependencies>
	<!-- Types -->
	<xsl:if test="@base_type_schema != 'pg_catalog'">
	  <dependency fqn="{concat('type.', 
			   ancestor::database/@name, '.', 
			   @base_type_schema, '.', @base_type)}"/>
	</xsl:if>
	<xsl:if test="@trans_type_schema != 'pg_catalog'">
	  <dependency fqn="{concat('type.', ancestor::database/@name, '.',
			   @trans_type_schema, '.', @transition_type)}"/>
	</xsl:if>
	<!-- Functions -->
	<xsl:if test="@transition_signature">
	  <dependency fqn="{concat('function.', 
			   ancestor::database/@name, '.', 
			   @transition_signature)}"/>
	</xsl:if>
	<xsl:if test="@final_signature">
	  <dependency fqn="{concat('function.', 
			   ancestor::database/@name, '.', 
			   @final_signature)}"/>
	</xsl:if>
	<!-- Operator -->
	<xsl:if test="@sort_operator">
	  <dependency fqn="{concat('operator.', 
			   ancestor::database/@name, '.', 
			   @sort_op_schema, '.', @sort_operator, '(',
			   @base_type_schema, '.', @base_type,
			   ',', @base_type_schema, '.', 
			   @base_type, ')')}"/>
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
	  <!-- Dependency on language if not internal, c, or sql -->
	  <dependency fqn="{concat('language.', 
			   ancestor::database/@name, '.', @language)}"/>
	</xsl:if>
	<!-- Dependencies on types. -->
	<xsl:for-each select="param">
	  <xsl:call-template name="TypeDep">
	    <xsl:with-param name="ignore" select="../handler_for"/>
	  </xsl:call-template>
	</xsl:for-each>
	<xsl:for-each select="result">
	  <xsl:call-template name="TypeDep">
	    <xsl:with-param name="ignore" select="../handler_for"/>
	  </xsl:call-template>
	</xsl:for-each>
	<!-- Dependencies on other functions. -->
	<xsl:for-each select="handler_for[@following]">
	  <xsl:call-template name="FuncDep">
	    <xsl:with-param name="fqn" select="@following"/>
	    <xsl:with-param name="schema" select="@following_schema"/>
	  </xsl:call-template>
	</xsl:for-each>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.',
			   ancestor::cluster/@name, '.', @owner)}"/>
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

  <!-- Generate dependency for a given function, ignoring built-ins -->
  <xsl:template name="FuncDep">
    <xsl:param name="fqn"/>
    <xsl:param name="schema"/>
    <xsl:if test="$schema != 'pg_catalog'"> 
      <!-- Ignore builtin types -->
      <dependency fqn="{concat('function.', 
		       ancestor::database/@name, '.', $fqn)}"/>
    </xsl:if>
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


  <xsl:template match="domain">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="domain_fqn" select="concat('type.', 
					    $parent_core, '.', @name)"/>
    <dbobject type="domain" name="{@name}"
	      fqn="{$domain_fqn}">
      <dependencies>
        <xsl:if test="(@basetype_schema != 'pg_toast') and
		      (@basetype_schema != 'pg_catalog') and
		      (@basetype_schema != 'information_schema')">
          <dependency fqn="{concat('type.', 
			   ancestor::database/@name, '.',
			   @basetype_schema, '.', @basetype)}"/>
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
  
  <xsl:template match="sequence">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="sequence_fqn" select="concat('sequence.', 
					   $parent_core, '.', @name)"/>
    <dbobject type="sequence" name="{@name}" fqn="{$sequence_fqn}">
      <!-- Dependency only on owner -->
      <dependencies>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.',
			   ancestor::cluster/@name, '.', @owner)}"/>
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

  <!-- Tables -->
  <xsl:template match="table">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="table_fqn" select="concat('table.', 
					   $parent_core, '.', @name)"/>
    <dbobject type="table" name="{@name}" fqn="{$table_fqn}">
      <dependencies>
	<!-- Dependencies on inherited tables -->
	<xsl:for-each select="inherits">
	  <dependency fqn="{concat('table.', 
			   ancestor::database/@name,
			   '.', @inherit_schema, '.', @inherit_table)}"/>
	</xsl:for-each>
	<!-- And on owner -->
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.',
			   ancestor::cluster/@name, '.', @owner)}"/>
	</xsl:if>
	<!-- And on tablespace -->
	<xsl:if test="@tablespace != 'pg_default'">
	  <dependency fqn="{concat('tablespace.',
			   ancestor::cluster/@name, '.', @tablespace)}"/>
	  <!-- And table owner's rights on tablespace, this is optional
	       as the table owner might be a superuser in which case
	       they will not have an explicit grant -->
	  <dependency pqn="{concat('grant.',
			   ancestor::cluster/@name, '.', @tablespace,
			   '.create:', @owner)}" optional="yes"/>
	</xsl:if>
	
	<!-- And on access to owner schema-->
	<dependency fqn="{concat('schema_access.',
			 ancestor::database/@name, '.', 
			 ancestor::schema/@name)}"/>

	<xsl:for-each select="column">
	  <!-- Dependencies on types (duplicated in columns, below) -->
	  <xsl:if test="self::*[@type_schema != 'pg_catalog']">
	    <dependency fqn="{concat('type.', 
			     ancestor::database/@name,
			     '.', @type_schema, '.', @type)}"/>
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

  <!-- Columns -->
  <xsl:template match="table/column">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="column_fqn" select="concat('column.', 
					   $parent_core, '.', @name)"/>
    <dbobject type="column" name="{@name}" fqn="{$column_fqn}"
	      parent_builds_us="yes" hide="yes" tablename="{../@name}"
	      schemaname="{../../@name}">
      <!-- Dependencies on types -->
      <xsl:if test="self::*[@type_schema != 'pg_catalog']">
	<dependencies>
	  <dependency cascades_to="yes"
		      fqn="{concat('type.', ancestor::database/@name,
			   '.', @type_schema, '.', @type)}"/>
	</dependencies>
      </xsl:if>
      <xsl:copy-of select="."/>
    </dbobject>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core"
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
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
