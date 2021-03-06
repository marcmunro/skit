<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Tablespaces -->
  <xsl:template match="tablespace">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <!-- Figure out whether we should consider the database or the
         cluster to be our parent.  It will be the database if this is
	 not the database's default tablespace.  -->
    <xsl:variable name="name">
      <xsl:value-of select="@name"/>
    </xsl:variable>

   <xsl:variable name="parent">
      <xsl:choose>
	<xsl:when test="../database[@tablespace=$name]">
	  <xsl:value-of select="'cluster'"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="concat('database.', ../database/@name)"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="this_core" select="@name"/>
      <xsl:with-param name="parent" select="$parent"/>
      <xsl:with-param name="fqn" select="concat('tablespace.', @name)"/>
      <xsl:with-param name="qname" select="skit:dbquote(@name)"/>
      <xsl:with-param name="do_schema_grant" select="'no'"/>
      <xsl:with-param name="do_context" select="'no'"/>

    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="tablespace" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:param name="parent"/>

    <xsl:choose>
      <xsl:when test="$parent='cluster'">
	<!-- If the parent is the cluster, this must be the database's
   	     default tablespace.  It might still be possible to
	     manipulate it within the database if we are not doing a
	     build or drop, so we allow a conditional dependency on the
	     database. -->
	<dependency-set>
	  <dependency fqn="{concat('database.', ../../database/@name)}"/>
	  <dependency fqn="cluster"/>
	</dependency-set>
	<!-- Tablespace owner must be a superuser. -->
	<dependency-set
	    fallback="{concat('privilege.role.', @owner, '.superuser')}"
	    parent="ancestor::dbobject[cluster]">
	  <dependency fqn="{concat('privilege.role.', @owner, '.superuser')}"/>
	</dependency-set>

      </xsl:when>
      <xsl:otherwise>
	<dependency fqn="{$parent}"/>
	<!-- Tablespace owner must be a superuser. -->
	<dependency-set
	    fallback="{concat('privilege.role.', @owner, '.superuser')}"
	    parent="//dbobject[database]">
	  <dependency fqn="{concat('privilege.role.', @owner, '.superuser')}"/>
	</dependency-set>
      </xsl:otherwise>
    </xsl:choose>

  </xsl:template>
</xsl:stylesheet>
