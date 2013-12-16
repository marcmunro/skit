<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <xsl:template name="feedback-dbobject">
    <xsl:param name="prefix" select="'----'"/>
    <xsl:param name="trailing" select="'&#x0A;'"/>
    <xsl:param name="echo" select="'\echo'"/>
    <xsl:value-of 
	select="concat('&#x0A;&#x0A;', $prefix, ' DBOBJECT ', @fqn, 
		' (', @action, ')&#x0A;')"/> 
    <xsl:if test="skit:eval('echoes') = 't'">
      <xsl:value-of 
	  select="concat($echo, ' ', ../@action, ' ', @type, ' ', 
		          @qname, '...&#x0A;')"/>
    </xsl:if>
    <xsl:value-of select="$trailing"/>
  </xsl:template>

  <xsl:template name="feedback">
    <xsl:param name="prefix" select="'----'"/>
    <xsl:param name="trailing" select="'&#x0A;'"/>
    <xsl:param name="echo" select="'\echo'"/>
    <xsl:value-of 
	select="concat('&#x0A;&#x0A;', $prefix, ' DBOBJECT ', ../@fqn, 
		' (', ../@action, ')&#x0A;')"/> 
    <xsl:if test="skit:eval('echoes') = 't'">
      <xsl:value-of 
	  select="concat($echo, ' ', ../@action, ' ', ../@type, ' ', 
		          ../@qname, '...&#x0A;')"/>
    </xsl:if>
    <xsl:value-of select="$trailing"/>
  </xsl:template>

  <xsl:template name="shell-feedback">
    <xsl:call-template name="feedback">
      <xsl:with-param name="prefix" select="'####'"/>
      <xsl:with-param name="trailing" select="''"/>
      <xsl:with-param name="echo" select="'echo'"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="shell-feedback-dbobject">
    <xsl:call-template name="feedback-dbobject">
      <xsl:with-param name="prefix" select="'####'"/>
      <xsl:with-param name="trailing" select="''"/>
      <xsl:with-param name="echo" select="'echo'"/>
    </xsl:call-template>
  </xsl:template>
</xsl:stylesheet>