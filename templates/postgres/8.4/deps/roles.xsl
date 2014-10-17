<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Roles: roles are cluster level objects.   As privileges are handled
       specially in roles we do not use the default dbobject template.  -->
  <xsl:template match="role">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <!-- Figure out whether we should consider the database or the
         cluster to be our parent.  It will be the database if this is
	 not the database's owner.  -->
    <xsl:variable name="name">
      <xsl:value-of select="@name"/>
    </xsl:variable>

    <xsl:variable name="parent">
      <xsl:choose>
	<xsl:when test="../database[@owner=$name]">
	  <xsl:value-of select="'cluster'"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="concat('database.', ../database/@name)"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <dbobject type="role" name="{@name}" qname="{skit:dbquote(@name)}"
	      fqn="{concat('role.', @name)}"
	      parent="{$parent}">

      <xsl:choose>
	<xsl:when test="$parent='cluster'">
	  <!-- If the parent is the cluster, this must be the database's 
	       owner.  It might still be possible to manipulate it
	       within the database if we are not doing a build or drop,
	       so we allow a conditional dependency on the database. -->
	<dependency-set>
	  <dependency fqn="{concat('database.', ../database/@name)}"/>
	  <dependency fqn="cluster"/>
	</dependency-set>
	</xsl:when>
	<xsl:otherwise>
	  <dependency fqn="{$parent}"/>
	</xsl:otherwise>
      </xsl:choose>

      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
	<!-- We copy the privileges directly as it is within the role
	     deinition that we assign them.  Note that in order to
	     create a complete dbobject, the privilege element is also
	     copied within the deboject creation in the template below. -->
	<xsl:for-each select="privilege">
	  <xsl:copy>
	    <xsl:copy-of select="@*"/>
	  </xsl:copy>
	</xsl:for-each>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <xsl:template match="role/privilege">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param name="name" select="@priv"/>
      <xsl:with-param name="fqn" select="concat('privilege.role.', 
					        $parent_core, '.', @priv)"/>
      <xsl:with-param name="qname" select="skit:dbquote(@priv)"/>
      <xsl:with-param name="others">
	<param name="role_qname" value="{skit:dbquote(../@name)}"/>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="role/privilege" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('role.', $parent_core)}"/>
  </xsl:template>
</xsl:stylesheet>
