<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='dbincluster']/database">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:text>&#x0A;create database </xsl:text>
	<xsl:value-of select="../@qname"/>
	<xsl:text> with&#x0A;</xsl:text>
	<xsl:text> owner &quot;</xsl:text>
	<xsl:value-of select="@owner"/>
	<xsl:text>&quot;&#x0A;</xsl:text>
	<xsl:text> encoding &apos;</xsl:text>
	<xsl:value-of select="@encoding"/>
	<xsl:text>&apos;&#x0A;</xsl:text>
	<xsl:text> tablespace &quot;</xsl:text>
	<xsl:value-of select="@tablespace"/>
	<xsl:text>&quot;&#x0A;</xsl:text>
	<xsl:text> connection limit = </xsl:text>
	<xsl:value-of select="@connections"/>
	<xsl:text>;&#x0A;</xsl:text>
	<xsl:apply-templates/>
	<xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:text>&#x0A;-- drop database </xsl:text>
	<xsl:value-of select="../@qname"/>
	<xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="dbobject[@type='database']/database">

    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;psql -d </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> &lt;&lt;&apos;DBEOF&apos;&#x0A;</xsl:text>
	<xsl:text>set standard_conforming_strings = off;&#x0A;</xsl:text>
        <xsl:text>set escape_string_warning = off;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>DBEOF&#x0A;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='arrive'">
      <print>
        <xsl:text>psql -d </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> &lt;&lt;&apos;DBEOF&apos;&#x0A;</xsl:text>
	<xsl:text>set standard_conforming_strings = off;&#x0A;</xsl:text>
        <xsl:text>set escape_string_warning = off;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='depart'">
      <print>
        <xsl:text>DBEOF&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:apply-templates/>
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