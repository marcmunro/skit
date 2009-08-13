<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/type">
    <xsl:if test="(../@action='build') and (@is_defined = 't')">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@owner"/>
          <xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>create type </xsl:text>

        <xsl:value-of select="../@qname"/>
	<xsl:choose>
	  <xsl:when test="@subtype='basetype'">
            <xsl:text> (&#x0A;  input = &quot;</xsl:text>
            <xsl:value-of select="*[@type='input']/@schema"/>
            <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="*[@type='input']/@name"/>
            <xsl:text>&quot;,&#x0A;  output = &quot;</xsl:text>
            <xsl:value-of select="*[@type='output']/@schema"/>
            <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="*[@type='output']/@name"/>
            <xsl:text>&quot;</xsl:text>
	    <xsl:if test="*[@type='send']">
              <xsl:text>,&#x0A;  send = &quot;</xsl:text>
              <xsl:value-of select="*[@type='send']/@schema"/>
              <xsl:text>&quot;.&quot;</xsl:text>
              <xsl:value-of select="*[@type='send']/@name"/>
              <xsl:text>&quot;</xsl:text>
	    </xsl:if>
	    <xsl:if test="*[@type='receive']">
              <xsl:text>,&#x0A;  send = &quot;</xsl:text>
              <xsl:value-of select="*[@type='receive']/@schema"/>
              <xsl:text>&quot;.&quot;</xsl:text>
              <xsl:value-of select="*[@type='receive']/@name"/>
              <xsl:text>&quot;</xsl:text>
	    </xsl:if>
	    <xsl:if test="*[@type='analyze']">
              <xsl:text>,&#x0A;  send = &quot;</xsl:text>
              <xsl:value-of select="*[@type='analyze']/@schema"/>
              <xsl:text>&quot;.&quot;</xsl:text>
              <xsl:value-of select="*[@type='analyze']/@name"/>
              <xsl:text>&quot;</xsl:text>
	    </xsl:if>
	    <xsl:choose>
	      <xsl:when test="@passbyval='yes'">
		<xsl:text>,&#x0A;  passedbyvalue</xsl:text>
		<xsl:text>,&#x0A;  internallength = </xsl:text>
		<xsl:value-of select="@typelen"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:text>,&#x0A;  internallength = </xsl:text>
		<xsl:choose>
		  <xsl:when test="@typelen &lt; 0">
		    <xsl:text>variable</xsl:text>
		  </xsl:when>
		  <xsl:otherwise>
		    <xsl:value-of select="@typelen"/>
		  </xsl:otherwise>
		</xsl:choose>
	      </xsl:otherwise>
	    </xsl:choose>
	    <xsl:if test="@alignment">
              <xsl:text>,&#x0A;  alignment = </xsl:text>
              <xsl:value-of select="@alignment"/>
	    </xsl:if>
	    <xsl:if test="@storage">
              <xsl:text>,&#x0A;  storage = </xsl:text>
              <xsl:value-of select="@storage"/>
	    </xsl:if>
	    <xsl:if test="@element">
              <xsl:text>,&#x0A;  element = </xsl:text>
              <xsl:value-of select="'TODO'"/>
	    </xsl:if>
	    <xsl:if test="@delimiter">
              <xsl:text>,&#x0A;  delimiter = &apos;</xsl:text>
              <xsl:value-of select="@delimiter"/>
              <xsl:text>&apos;</xsl:text>
	    </xsl:if>
	  </xsl:when>
	</xsl:choose>
	<xsl:text>);&#x0A;</xsl:text>
	<xsl:apply-templates/>

	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
	<xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;drop type </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:if test="@subtype='basetype'">
	  <!-- Basetypes must be dropped using cascade to ensure that
	       the input, output, etc functions are also dropped -->
          <xsl:text> cascade</xsl:text>
	</xsl:if>
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
