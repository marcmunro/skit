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

  <xsl:template match="privilege">
    <!-- WHY IS A PRIVILEGE CONSIDERED TO BE A DBOBJECT?  
         I'm sure I has a good reason but I can't recall it.  If anyone
	 ever figures it out, please comment the reason here.  MM -->
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="priv_name" select="concat($parent_core, '.', @priv)"/>
    <dbobject type="privilege" name="{@priv}" qname="{skit:dbquote(@name)}"
	      fqn="{concat('privilege.', $priv_name)}">
      <xsl:copy>
	<xsl:copy-of select="@*"/>
      </xsl:copy>
    </dbobject>
  </xsl:template>


</xsl:stylesheet>
