<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- User Mapping -->
  <xsl:template match="user_mapping">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <xsl:apply-templates select="." mode="dbobject">
      <xsl:with-param name="parent_core" select="$parent_core"/>
      <xsl:with-param 
	  name="qname" 
	  select="concat(skit:dbquote(@server), ':', skit:dbquote(@user))"/>
      <xsl:with-param 
	  name="fqn" 
	  select="concat('user_mapping.', $parent_core, '.',
	                 skit:dbquote(@server), ':', skit:dbquote(@user))"/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="user_mapping" mode="dependencies">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>

    <dependency fqn="{concat('database.', $parent_core)}"/>
    <dependency fqn="{concat('foreign_server.', 
		              ancestor::database/@name, '.', 
			      @server)}"/>
    <xsl:if test="@user">
      <dependency fqn="{concat('role.', @user)}"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

