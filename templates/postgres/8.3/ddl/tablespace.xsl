<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/tablespace">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;create tablespace </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text> owner </xsl:text>
        <xsl:value-of select="skit:dbquote(@owner)"/>
        <xsl:text>&#x0A;  location &apos;</xsl:text>
        <xsl:value-of select="@location"/>
        <xsl:text>&apos;;&#x0A;</xsl:text>
	<xsl:apply-templates/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;\echo Not dropping tablespace </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> as it may</xsl:text>
        <xsl:text> contain objects in other dbs;&#x0A;</xsl:text>
        <xsl:text>\echo To perform the drop uncomment the</xsl:text>
	<xsl:text>  following line:&#x0A;</xsl:text>
        <xsl:text>-- drop tablespace </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

