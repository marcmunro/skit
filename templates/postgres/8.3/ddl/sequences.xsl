<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/sequence">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@owner"/>
          <xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
	</xsl:if>

        <xsl:text>create sequence </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text>&#x0A;  start with </xsl:text>
        <xsl:value-of select="@start_with"/>
	<xsl:text>&#x0A;  increment by </xsl:text>
        <xsl:value-of select="@increment_by"/>
	<xsl:text>&#x0A;  minvalue </xsl:text>
        <xsl:value-of select="@min_value"/>
	<xsl:text>&#x0A;  maxvalue </xsl:text>
        <xsl:value-of select="@max_value"/>
	<xsl:text>&#x0A;  cache </xsl:text>
        <xsl:value-of select="@cache"/>
	<xsl:if test="@cycled='t'">
	  <xsl:text>&#x0A;  cycle</xsl:text>
	</xsl:if>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;drop sequence </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
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
