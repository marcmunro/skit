<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/cast">
    <xsl:if test="../@action='build'">
      <print>
      	<xsl:text>create cast(&quot;</xsl:text>
        <xsl:value-of select="source/@schema"/>
      	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="source/@type"/>
      	<xsl:text>&quot; as &quot;</xsl:text>
        <xsl:value-of select="target/@schema"/>
      	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="target/@type"/>
      	<xsl:text>&quot;)&#x0A;  </xsl:text>
	<xsl:choose>
	  <xsl:when test="handler-function">
      	    <xsl:text>with function &quot;</xsl:text>
            <xsl:value-of select="handler-function/@schema"/>
      	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="handler-function/@name"/>
      	    <xsl:text>&quot;(&quot;</xsl:text>
            <xsl:value-of select="source/@schema"/>
      	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="source/@type"/>
      	    <xsl:text>&quot;)&#x0A;  </xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
      	    <xsl:text>without function</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="@context='a'">
      	  <xsl:text>as assignment</xsl:text>
	</xsl:if>
	<xsl:if test="@context='i'">
      	  <xsl:text>as implicit</xsl:text>
	</xsl:if>
      	<xsl:text>;&#x0A;</xsl:text>

      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
      	<xsl:text>&#x0A;</xsl:text>
      	<xsl:text>drop cast(&quot;</xsl:text>
        <xsl:value-of select="source/@schema"/>
      	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="source/@type"/>
      	<xsl:text>&quot; as &quot;</xsl:text>
        <xsl:value-of select="target/@schema"/>
      	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="target/@type"/>
      	<xsl:text>&quot;);&#x0A;  </xsl:text>
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
