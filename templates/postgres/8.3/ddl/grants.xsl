<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@subtype='role']/grant">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:if test="@from != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@from"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:if>

        <xsl:text>grant &quot;</xsl:text>
        <xsl:value-of select="@priv"/>
        <xsl:text>&quot; to &quot;</xsl:text>
        <xsl:value-of select="@to"/>
        <xsl:text>&quot;</xsl:text>
	<xsl:if test="@with_admin = 'yes'">
          <xsl:text> with admin option</xsl:text>
	</xsl:if>
        <xsl:text>;&#x0A;</xsl:text>

	<xsl:if test="@from != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
  
    <xsl:if test="../@action='drop'">
      <print>
	<xsl:if test="@from != //@user">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@from"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:if>

        <xsl:text>revoke &quot;</xsl:text>
        <xsl:value-of select="@priv"/>
        <xsl:text>&quot; from &quot;</xsl:text>
        <xsl:value-of select="@to"/>
        <xsl:text>&quot;;&#x0A;</xsl:text>

	<xsl:if test="@from != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
    
  </xsl:template>

  <!-- Object-level grants -->
  <xsl:template match="dbobject[@subtype!='role']/grant">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:if test="@from != //cluster/@username">
          <xsl:text>set session authorization </xsl:text>
	  <xsl:value-of select="@from"/>
	  <xsl:text>;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:text>grant </xsl:text>
	<xsl:value-of select="@priv"/>
	<xsl:text> on </xsl:text>
	<xsl:value-of select="../@subtype"/>
	<xsl:text> </xsl:text>
	<xsl:value-of select="../@on"/>
	<xsl:text> to &quot;</xsl:text>
	<xsl:value-of select="@to"/>
	<xsl:text>&quot;</xsl:text>
	<xsl:if test="@with_grant = 'yes'">
	  <xsl:text> with grant option</xsl:text>
	</xsl:if>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:if test="@from != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
    
  
    <xsl:if test="../@action='drop'">
      <print>
	<xsl:if test="@from != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@from"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:text>revoke </xsl:text>
	<xsl:value-of select="@priv"/>
	<xsl:text> on </xsl:text>
	<xsl:value-of select="../@subtype"/>
	<xsl:text> </xsl:text>
	<xsl:value-of select="../@on"/>
	<xsl:text> from &quot;</xsl:text>
	<xsl:value-of select="@to"/>
	<xsl:text>&quot;;&#x0A;</xsl:text>
	<xsl:if test="@from != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>
    
  </xsl:template>

</xsl:stylesheet>

<!-- Keep this comment at the end of the file
Local variables:
mode: xml
sgml-omittag:nil
sgml-shorttag:nil
sgml-namecase-general:nil
sgml-general-insert-case:lower
sgml-minimize-attributes:nil
sgml-always-quote-attributes:t
sgml-indent-step:2
sgml-indent-data:t
sgml-parent-document:nil
sgml-exposed-tags:nil
sgml-local-catalogs:nil
sgml-local-ecat-files:nil
End:
-->
