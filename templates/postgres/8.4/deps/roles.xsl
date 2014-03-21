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
    <dbobject type="role" name="{@name}" qname="{skit:dbquote(@name)}"
	      fqn="{concat('role.', @name)}"
	      parent="cluster">
      <dependencies>
	<dependency fqn="cluster"/>
      </dependencies>
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
