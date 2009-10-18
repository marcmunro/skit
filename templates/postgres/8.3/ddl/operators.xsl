<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/operator">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="@owner != //cluster/@username">
          <xsl:text>set session authorization &apos;</xsl:text>
          <xsl:value-of select="@owner"/>
          <xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:text>create operator &quot;</xsl:text>
        <xsl:value-of select="@schema"/>
	<xsl:text>&quot;.</xsl:text>
        <xsl:value-of select="@name"/>
	<xsl:text>(&#x0A;  leftarg = </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='left']">
	    <xsl:text>&quot;</xsl:text>
            <xsl:value-of select="arg[@position='left']/@schema"/>
	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="arg[@position='left']/@name"/>
	    <xsl:text>&quot;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>,&#x0A;  rightarg = </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='right']">
	    <xsl:text>&quot;</xsl:text>
            <xsl:value-of select="arg[@position='right']/@schema"/>
	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="arg[@position='right']/@name"/>
	    <xsl:text>&quot;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>,&#x0A;  procedure = &quot;</xsl:text>
        <xsl:value-of select="procedure/@schema"/>
	<xsl:text>&quot;.&quot;</xsl:text>
        <xsl:value-of select="procedure/@name"/>
	<xsl:if test="commutator">
	  <xsl:text>&quot;,&#x0A;  commutator = &quot;</xsl:text>
          <xsl:value-of select="commutator/@schema"/>
	  <xsl:text>&quot;.&quot;</xsl:text>
          <xsl:value-of select="commutator/@name"/>
	</xsl:if>
	<xsl:if test="negator">
	  <xsl:text>&quot;,&#x0A;  negator = &quot;</xsl:text>
          <xsl:value-of select="negator/@schema"/>
	  <xsl:text>&quot;.&quot;</xsl:text>
          <xsl:value-of select="negator/@name"/>
	</xsl:if>
	<xsl:if test="restrict">
	  <xsl:text>&quot;,&#x0A;  restrict = &quot;</xsl:text>
          <xsl:value-of select="restrict/@schema"/>
	  <xsl:text>&quot;.&quot;</xsl:text>
          <xsl:value-of select="restrict/@name"/>
	</xsl:if>
	<xsl:if test="join">
	  <xsl:text>&quot;,&#x0A;  join = &quot;</xsl:text>
          <xsl:value-of select="join/@schema"/>
	  <xsl:text>&quot;.&quot;</xsl:text>
          <xsl:value-of select="join/@name"/>
	</xsl:if>
	<xsl:if test="@hashes">
	  <xsl:text>&quot;,&#x0A;  hashes</xsl:text>
	</xsl:if>
	<xsl:text>&quot;</xsl:text>
	<xsl:if test="@merges">
	  <xsl:text>,&#x0A;  merges</xsl:text>
	</xsl:if>
	<xsl:text>&#x0A;);&#x0A;</xsl:text>

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
	  
      	<xsl:text>&#x0A;drop operator &quot;</xsl:text>
        <xsl:value-of select="@schema"/>
	<xsl:text>&quot;.</xsl:text>
        <xsl:value-of select="@name"/>
      	<xsl:text>(</xsl:text>

	<xsl:choose>
	  <xsl:when test="arg[@position='left']">
	    <xsl:text>&quot;</xsl:text>
            <xsl:value-of select="arg[@position='left']/@schema"/>
	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="arg[@position='left']/@name"/>
	    <xsl:text>&quot;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>, </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='right']">
	    <xsl:text>&quot;</xsl:text>
            <xsl:value-of select="arg[@position='right']/@schema"/>
	    <xsl:text>&quot;.&quot;</xsl:text>
            <xsl:value-of select="arg[@position='right']/@name"/>
	    <xsl:text>&quot;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
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
