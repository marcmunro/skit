<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  version="1.0">

  <xsl:output method="xml" indent="yes"/>
  <xsl:strip-space elements="*"/>

  <!-- This stylesheet adds dependency definitions to dbobjects unless
       they appear to already exist. -->

  <xsl:template match="/*">
    <dump>
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
    </dump>
  </xsl:template>

  <!-- This template handles copy-only mode.  This is used when we
       discover that a document already has dependencies defined -->
  <xsl:template match="*" mode="copy">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy"/>
    </xsl:copy>
  </xsl:template>

  <!-- Anything not matched explicitly will match this and be copied 
       This handles dbobjects, dependencies, etc -->
  <xsl:template match="*">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
        <xsl:with-param name="parent_core" select="$parent_core"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>

  <!-- Identify the base (to schema) of the root part of the current
       object's fqn. --> 
  <xsl:template name="fqn-base">
    <xsl:value-of select="ancestor::database/@name"/>
    <xsl:if test="ancestor::schema">
      <xsl:value-of select="concat('.', ancestor::schema/@name)"/>
    </xsl:if>
  </xsl:template>

  <!-- Identify the root part of the current object's fqn. --> 
  <xsl:template name="fqn-root">
    <xsl:value-of select="ancestor::database/@name"/>
    <xsl:if test="ancestor::schema">
      <xsl:value-of select="concat('.', ancestor::schema/@name)"/>
      <xsl:if test="name(..) != 'schema'">
	<xsl:value-of select="concat('.', ../@name)"/>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <!-- Create schema dependency entries for a dependency-set.  
       This creates dependency entries for public grants of $priv, 
       grants of $priv to $to, and automatic grants to $to if $to is the
       same as $owner.  -->
  <xsl:template name="schema-deps">
    <xsl:param name="owner" select="@owner"/>
    <xsl:param name="priv" select="@priv"/>
    <xsl:param name="type" select="name(..)"/>
    <xsl:param name="root">
      <xsl:call-template name="fqn-root"/>
    </xsl:param>
    <xsl:param name="to"/>
    <xsl:if test="$to = $owner">
      <dependency fqn="{concat('grant.', $type, '.', $root, '.', $priv)}"/>
    </xsl:if>
    <dependency pqn="{concat('grant.', $type, '.', $root, '.', 
		             $priv, ':', $to)}"/>
    <dependency pqn="{concat('grant.', $type, '.', $root, '.', 
		             $priv, ':public')}"/>
  </xsl:template>

  <xsl:template name="deps-schema-create">
    <xsl:param name="owner" select="@owner"/>
    <xsl:param name="root">
      <xsl:call-template name="fqn-base"/>
    </xsl:param>
    <xsl:param name="to"/>
    <xsl:call-template name="schema-deps">
      <xsl:with-param name="to" select="$to"/>
      <xsl:with-param name="root" select="$root"/>
      <xsl:with-param name="priv" select="'create'"/>
      <xsl:with-param name="type" select="'schema'"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="deps-schema-usage">
    <xsl:param name="owner" select="@owner"/>
    <xsl:param name="root">
      <xsl:call-template name="fqn-base"/>
    </xsl:param>
    <xsl:param name="to"/>
    <xsl:call-template name="schema-deps">
      <xsl:with-param name="to" select="$to"/>
      <xsl:with-param name="root" select="$root"/>
      <xsl:with-param name="priv" select="'usage'"/>
      <xsl:with-param name="type" select="'schema'"/>
    </xsl:call-template>
  </xsl:template>

  <!-- Schema grant dependencies -->
  <xsl:template name="SchemaGrant">
    <xsl:param name="owner" select="@owner"/>
    <xsl:if test="$owner">
      <!-- Dependency on schema create grant to owner, public or self -->
      <dependency-set 
	  fallback="{concat('privilege.cluster.', $owner, '.superuser')}"
	  parent="ancestor::dbobject[database]"
	  condition="forwards">
	<xsl:call-template name="deps-schema-create">
	  <xsl:with-param name="to" select="@owner"/>
	</xsl:call-template>
	<dependency fqn="{concat('privilege.cluster.', $owner, '.superuser')}"/>
      </dependency-set>
      <!-- Dependency on schema create grant to owner, public or self -->
      <dependency-set 
	  fallback="{concat('privilege.cluster.', $owner, '.superuser')}"
	  parent="ancestor::dbobject[database]"
	  condition="backwards">
	<xsl:if test="$owner">
	  <xsl:call-template name="deps-schema-usage">
	    <xsl:with-param name="to" select="@owner"/>
	  </xsl:call-template>
	  <dependency fqn="{concat('privilege.cluster.', 
			           $owner, '.superuser')}"/>
	</xsl:if>
      </dependency-set>
    </xsl:if>
  </xsl:template>

  <!-- Handle explicitly identified dependencies -->
  <xsl:template name="depends">
    <xsl:for-each select="depends">
      <xsl:choose>
	<xsl:when test="@operator_class">
	  <dependency fqn="{concat('operator_class.', 
			           ancestor::database/@name,
			           '.', @operator_class)}"/>
	</xsl:when>
	<xsl:when test="@cast">
	  <dependency fqn="{concat('cast.', 
			   ancestor::database/@name, 
			   '.', @cast)}"/>
	</xsl:when>	
	<xsl:when test="@function">
	  <dependency fqn="{concat('function.', ancestor::database/@name,
			        '.', @function)}"/>
	</xsl:when>
	<xsl:when test="@column">
	  <dependency fqn="{concat('column.', 
			   ancestor::database/@name, '.', 
			   ancestor::schema/@name, '.',
			   ancestor::table/@name, '.', @column)}"/>
	</xsl:when>	
	<xsl:otherwise>
	  <UNHANDLED_DEPENDS_NODE>
	    <xsl:copy-of select="@*"/>
	  </UNHANDLED_DEPENDS_NODE>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>


  <!-- Prevent text nodes being implicitly copied within the dbobject
       handler. -->  
  <xsl:template match="text()" mode="dbobject"/>
  <xsl:template match="text()" mode="dependencies"/>

  <!-- This is the basic dbobject handler.  Most matched dbobjects will
       invoke this template to do the basic donkey-work.  -->
  <xsl:template match="*" mode="dbobject">
    <xsl:param name="parent_core"/>
    <xsl:param name="type" select="name(.)"/>
    <xsl:param name="name" select="@name"/>
    <xsl:param name="qname" select="skit:dbquote(@schema,@name)"/>
    <xsl:param name="fqn" select="concat(name(.), '.', $parent_core, 
					 '.', $name)"/>
    <xsl:param name="parent" select="concat(name(..), '.', $parent_core)"/>
    <xsl:param name="owner" select="@owner"/>
    <xsl:param name="table_qname"/>
    <xsl:param name="cycle_breaker"/>
    <xsl:param name="role_qname"/>
    <xsl:param name="subtype"/>
    <xsl:param name="pqn"/>
    <xsl:param name="on"/>
    <xsl:param name="this_core" select="concat($parent_core, '.', @name)"/>
    <xsl:param name="do_schema_grant" select="'yes'"/>
    <xsl:param name="do_context" select="'yes'"/>
    <dbobject type="{$type}" name="{$name}" fqn="{$fqn}" qname="{$qname}" 
	      parent="{$parent}">
      <xsl:if test="$cycle_breaker">
	<xsl:attribute name="cycle_breaker">
	  <xsl:value-of select="$cycle_breaker"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$table_qname">
	<xsl:attribute name="table_qname">
	  <xsl:value-of select="$table_qname"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$role_qname">
	<xsl:attribute name="role_qname">
	  <xsl:value-of select="$role_qname"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$subtype">
	<xsl:attribute name="subtype">
	  <xsl:value-of select="$subtype"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$pqn">
	<xsl:attribute name="pqn">
	  <xsl:value-of select="$pqn"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$on">
	<xsl:attribute name="on">
	  <xsl:value-of select="$on"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$owner and $do_context = 'yes'">
	<context name="owner" value="{$owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>
      <dependencies>
	<xsl:apply-templates select="." mode="dependencies">
	  <xsl:with-param name="parent_core" select="$parent_core"/>
	</xsl:apply-templates>

	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>

	<xsl:if test="$do_schema_grant = 'yes'">
	  <xsl:call-template name="SchemaGrant"/>
	</xsl:if>
      </dependencies>

      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="$this_core"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>
  

  <xsl:include href="skitfile:deps/cluster.xsl"/>

</xsl:stylesheet>

