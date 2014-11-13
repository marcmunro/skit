<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
    xmlns:skit="http://www.bloodnok.com/xml/skit"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:exsl="http://exslt.org/common"
    extension-element-prefixes="exsl"
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
       same as the schema owner ($owner).  -->
  <xsl:template name="schema-deps">
    <xsl:param name="owner" select="../@owner"/>
    <xsl:param name="priv" select="@priv"/>
    <xsl:param name="type" select="name(..)"/>
    <xsl:param name="root">
      <xsl:call-template name="fqn-root"/>
    </xsl:param>
    <xsl:param name="to"/>
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
      <dependency-set priority="1"
	  fallback="{concat('privilege.role.', $owner, '.superuser')}"
	  parent="ancestor::dbobject[database]"
	  applies="forwards">
	<xsl:call-template name="deps-schema-create">
	  <xsl:with-param name="to" select="@owner"/>
	</xsl:call-template>
	<dependency fqn="{concat('privilege.role.', $owner, '.superuser')}"/>
      </dependency-set>
      <!-- Dependency on schema create grant to owner, public or self -->
      <dependency-set priority="1"
	  fallback="{concat('privilege.role.', $owner, '.superuser')}"
	  parent="ancestor::dbobject[database]"
	  applies="backwards">
	<xsl:if test="$owner">
	  <xsl:call-template name="deps-schema-usage">
	    <xsl:with-param name="to" select="@owner"/>
	  </xsl:call-template>
	  <dependency fqn="{concat('privilege.role.', 
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
    <xsl:param name="owner">
      <xsl:choose>
	<xsl:when test="@owner">
	  <xsl:value-of select="@owner"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="../@owner"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:param>
    <xsl:param name="this_core" select="concat($parent_core, '.', @name)"/>
    <xsl:param name="do_schema_grant" select="'yes'"/>
    <xsl:param name="do_context" select="'yes'"/>
    <xsl:param name="others" select="false()"/>
    <dbobject type="{$type}" name="{$name}" fqn="{$fqn}" qname="{$qname}">
      <xsl:if test="$parent != 'None'">
	<xsl:attribute name="parent">
	  <xsl:value-of select="$parent"/>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="$others">
	<xsl:for-each select="exsl:node-set($others)/param">
	  <xsl:attribute name="{@name}">
	  <xsl:value-of select="@value"/>
	</xsl:attribute>
	</xsl:for-each>
      </xsl:if>

      <xsl:if test="$owner and $do_context = 'yes'">
	<context type="owner" value="{$owner}" 
		 default="{//cluster/@username}"/>	
      </xsl:if>

      <dependencies>
	<xsl:apply-templates select="." mode="dependencies">
	  <xsl:with-param name="parent_core" select="$parent_core"/>
	  <xsl:with-param name="parent" select="$parent"/>
	</xsl:apply-templates>

	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.', @owner)}"/>
	</xsl:if>

	<xsl:if test="$do_schema_grant = 'yes'">
	  <xsl:call-template name="SchemaGrant"/>
	</xsl:if>

	<xsl:if test="ancestor::schema and comment">
	  <!-- This object is a descendent of a schema and contains a
	       comment.  To modify a comment, "usage" privilege is
	       required on the schema.  Note: the default schema
	       privilege required to create the object is "create".  -->
	  <dependency-set 
	      priority="1"
	      applies="forwards"
	      condition="element[@type='comment']"
	      fallback="{concat('privilege.role.', $owner, '.superuser')}"
	      parent="ancestor::dbobject[database]">
	    <xsl:call-template name="deps-schema-usage">
	      <xsl:with-param name="to" select="@owner"/>
	    </xsl:call-template>
	  </dependency-set>
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

