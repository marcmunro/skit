<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Generate a dependency for a given type, ignoring built-ins -->
  <xsl:template name="TypeDep">
    <xsl:param name="handler_for" select="NONE"/>
    <xsl:param name="type_name" select="@type"/>
    <xsl:param name="type_schema" select="@schema"/>
    <xsl:if test="$type_schema != 'pg_catalog'"> 
      <xsl:choose>
	<xsl:when test="not(($handler_for/@schema = $type_schema) and
	                    ($handler_for/@name = $type_name))">
	  <!-- We depend on the type. -->
	  <dependency fqn="{concat('type.', ancestor::database/@name,
			   '.', $type_schema, '.', $type_name)}"/>
	</xsl:when>
	<xsl:otherwise>
	  <!-- We are a handler for the type, so depend on the shelltype. -->
	  <dependency fqn="{concat('shelltype.', ancestor::database/@name,
			   '.', $type_schema, '.', $type_name)}"/>
	  
	</xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <!-- Domains -->
  <xsl:template match="domain">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="fqn" 
		      select="concat('type.', $parent_core, '.', @name)"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="domain" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>

    <xsl:if test="(@basetype_schema != 'pg_toast') and
		   (@basetype_schema != 'pg_catalog') and
		   (@basetype_schema != 'information_schema')">
      <dependency fqn="{concat('type.', ancestor::database/@name, '.',
			@basetype_schema, '.', @basetype)}"/>
    </xsl:if>
  </xsl:template>


  <!-- Types -->
  <xsl:template match="type">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
    </xsl:apply-templates>

    <xsl:if test="@subtype='basetype'">
      <!-- Add shell-type dbobject.  This does nothing except provide
	   correct build ordering through the proper handling of
	   dependencies.  -->
      <dbobject type="shelltype" 
		fqn="{concat('shelltype.', $parent_core, '.', @name)}"
		qname="{skit:dbquote(@schema,@name)}"
		parent="{concat(name(..), '.', $parent_core)}"
		follow="{concat('type.', $parent_core, '.', @name)}">
	<context type="owner" value="{@owner}" 
		 default="{//cluster/@username}"/>
	<dependencies>
	  <dependency fqn="{concat(name(..), '.', $parent_core)}"/>
	  <dependency fqn="{concat('role.', @owner)}"/>
	</dependencies>
	<shelltype> 
	  <xsl:if test="@extension">
	    <xsl:attribute name="extension">
	      <xsl:value-of select="@extension"/>
	    </xsl:attribute>
	  </xsl:if>
	</shelltype>
      </dbobject>
    </xsl:if>
  </xsl:template>

  <xsl:template match="type" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('schema.', $parent_core)}"/>
    <xsl:if test="@extension">
      <dependency fqn="{concat('extension.', ancestor::database/@name,
			       '.',  @extension)}"/>
    </xsl:if>
    <xsl:if test="@collation_name and @collation_schema != 'pg_catalog'">
      <dependency fqn="{concat('collation.', ancestor::database/@name,
			       '.',  @collation_schema,
			       '.',  @collation_name)}"/>
    </xsl:if>

    <xsl:for-each select="handler-function">
      <dependency fqn="{concat('function.', ancestor::database/@name, 
			       '.', @signature)}"/>
    </xsl:for-each>

    <xsl:for-each select="column">
      <xsl:if test="(@type_schema != 'pg_toast') and
		    (@type_schema != 'pg_catalog') and
		    (@type_schema != 'information_schema')">
	<dependency fqn="{concat('type.', ancestor::database/@name, '.',
			  @type_schema, '.', @type)}"/>
      </xsl:if>
      <xsl:if test="@collation_name and @collation_schema != 'pg_catalog'">
	<dependency fqn="{concat('collation.', ancestor::database/@name,
			         '.',  @collation_schema,
			         '.',  @collation_name)}"/>
      </xsl:if>
    </xsl:for-each>
    <xsl:if test="@subtype='basetype'">
      <dependency fqn="{concat('shelltype.', $parent_core, '.', @name)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>


