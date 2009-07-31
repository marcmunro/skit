<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- TODO: comments on domains! -->
  <xsl:template match="dbobject/domain">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@from"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:if>
	
        <xsl:text>&#x0A;create domain &quot;</xsl:text>
        <xsl:value-of select="@schema"/>
        <xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&quot;&#x0A;  as &quot;</xsl:text>
        <xsl:value-of select="@basetype_schema"/>
        <xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="@basetype"/>
        <xsl:text>&quot;</xsl:text>
	<xsl:for-each select="constraint">
          <xsl:text>&#x0A;  </xsl:text>
          <xsl:value-of select="@source"/>
	</xsl:for-each>
	<xsl:if test="@default">
          <xsl:text>&#x0A;  default </xsl:text>
          <xsl:value-of select="@default"/>
	</xsl:if>

        <xsl:text>;&#x0A;</xsl:text>
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;drop domain &quot;</xsl:text>
        <xsl:value-of select="@schema"/>
        <xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>&quot;&#x0A;&#x0A;</xsl:text>
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
