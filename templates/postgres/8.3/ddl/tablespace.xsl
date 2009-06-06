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
        <xsl:text> owner &quot;</xsl:text>
        <xsl:value-of select="@owner"/>
        <xsl:text>&quot;</xsl:text>
        <xsl:text>&#x0A;  location &apos;</xsl:text>
        <xsl:value-of select="@location"/>
        <xsl:text>&apos;;&#x0A;</xsl:text>
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
