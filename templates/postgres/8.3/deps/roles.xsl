<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Roles: roles are cluster level objects. -->
  <xsl:template match="role">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="role_name" select="concat($parent_core, '.', @name)"/>
    <dbobject type="role" name="{@name}" qname="{skit:dbquote(@name)}"
	      fqn="{concat('role.', $role_name)}">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="$role_name"/>
	</xsl:apply-templates>
	<xsl:for-each select="privilege">
	  <xsl:copy>
	    <xsl:copy-of select="@*"/>
	  </xsl:copy>
	</xsl:for-each>
      </xsl:copy>

    </dbobject>
  </xsl:template>

  <xsl:template match="privilege">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="priv_name" select="concat($parent_core, '.', @priv)"/>
    <dbobject type="privilege" name="{@priv}" qname="{skit:dbquote(@name)}"
	      fqn="{concat('privilege.', $priv_name)}">
      <!-- No need for any contents, these nodes exist only to record
           whether dependent privileges exist. -->
    </dbobject>
  </xsl:template>


</xsl:stylesheet>
