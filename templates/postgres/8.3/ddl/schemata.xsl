<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/schema">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:choose>
	  <xsl:when test="../@name='public'">
            <xsl:text>&#x0A;alter schema </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> owner to &quot;</xsl:text>
            <xsl:value-of select="@owner"/>
	  </xsl:when>
	  <xsl:otherwise>
            <xsl:text>&#x0A;create schema </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> authorization &quot;</xsl:text>
            <xsl:value-of select="@owner"/>
	  </xsl:otherwise>
	</xsl:choose>
        <xsl:text>&quot;;&#x0A;</xsl:text>
	<xsl:apply-templates/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;drop schema</xsl:text>
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
