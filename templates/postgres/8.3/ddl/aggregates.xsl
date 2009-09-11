<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/aggregate">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@owner"/>
          <xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:text>create aggregate </xsl:text>
	<xsl:text>&quot;</xsl:text>
        <xsl:value-of select="@schema"/>
	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="@name"/>
	<xsl:text>&quot;(</xsl:text>
	<xsl:choose>
	  <xsl:when test="basetype/@name">
	    <xsl:text>&quot;</xsl:text>
            <xsl:value-of select="basetype/@schema"/>
	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="basetype/@name"/>
	    <xsl:text>&quot;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>*</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>) (&#x0A;  sfunc = &quot;</xsl:text>
        <xsl:value-of select="transfunc/@schema"/>
	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="transfunc/@name"/>
	<xsl:text>&quot;,&#x0A;  stype = &quot;</xsl:text>
        <xsl:value-of select="transtype/@schema"/>
	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="transtype/@name"/>
	<xsl:text>&quot;</xsl:text>
	<xsl:if test="@initcond">
	  <xsl:text>,&#x0A;  initcond = </xsl:text>
          <xsl:value-of select="@initcond"/>
	</xsl:if>
	<xsl:if test="finalfunc">
	  <xsl:text>,&#x0A;  finalfunc = &quot;</xsl:text>
          <xsl:value-of select="finalfunc/@schema"/>
	  <xsl:text>&quot;.&quot;</xsl:text>
          <xsl:value-of select="finalfunc/@name"/>
	  <xsl:text>&quot;</xsl:text>
	</xsl:if>
	<xsl:if test="sortop">
	  <xsl:text>,&#x0A;  sortop = &quot;</xsl:text>
          <xsl:value-of select="sortop/@schema"/>
	  <xsl:text>&quot;.&quot;</xsl:text>
          <xsl:value-of select="sortop/@name"/>
	  <xsl:text>&quot;</xsl:text>
	</xsl:if>
	<xsl:text>);&#x0A;</xsl:text>
	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
      	<xsl:text>&#x0A;</xsl:text>
      	<xsl:if test="@owner != //cluster/@username">
      	  <xsl:text>set session authorization &apos;</xsl:text>
      	  <xsl:value-of select="@owner"/>
      	  <xsl:text>&apos;;&#x0A;</xsl:text>
      	</xsl:if>
	  
      	<xsl:text>drop aggregate </xsl:text>
	<xsl:text>&quot;</xsl:text>
        <xsl:value-of select="@schema"/>
	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="@name"/>
	<xsl:text>&quot;(</xsl:text>
	<xsl:choose>
	  <xsl:when test="basetype/@name">
	    <xsl:text>&quot;</xsl:text>
            <xsl:value-of select="basetype/@schema"/>
	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="basetype/@name"/>
	    <xsl:text>&quot;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>*</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>);&#x0A;</xsl:text>
	  
      	<xsl:if test="@owner != //cluster/@username">
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
