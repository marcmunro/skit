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
	<xsl:call-template name="set_owner"/>

	<xsl:text>create operator </xsl:text>
        <xsl:value-of select="skit:dbquote(@schema)"/>
        <xsl:text>.</xsl:text>
        <xsl:value-of select="@name"/>
	<xsl:text> (&#x0A;  leftarg = </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='left']">
            <xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
				               arg[@position='left']/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>,&#x0A;  rightarg = </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='right']">
            <xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
				               arg[@position='right']/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>,&#x0A;  procedure = </xsl:text>
        <xsl:value-of 
	   select="skit:dbquote(procedure/@schema,procedure/@name)"/>
	<xsl:if test="commutator">
	  <xsl:text>,&#x0A;  commutator = operator(</xsl:text>
          <xsl:value-of 
	     select="concat(commutator/@schema, '.', commutator/@name, ')')"/>
	</xsl:if>
	<xsl:if test="negator">
	  <xsl:text>,&#x0A;  negator = operator(</xsl:text>
          <xsl:value-of 
	     select="concat(negator/@schema, '.', negator/@name, ')')"/>
	</xsl:if>
	<xsl:if test="restrict">
	  <xsl:text>,&#x0A;  restrict = </xsl:text>
          <xsl:value-of select="skit:dbquote(restrict/@schema,restrict/@name)"/>
	</xsl:if>
	<xsl:if test="join">
	  <xsl:text>,&#x0A;  join = </xsl:text>
          <xsl:value-of select="skit:dbquote(join/@schema,join/@name)"/>
	</xsl:if>
	<xsl:if test="@hashes">
	  <xsl:text>,&#x0A;  hashes</xsl:text>
	</xsl:if>
	<xsl:if test="@merges">
	  <xsl:text>,&#x0A;  merges</xsl:text>
	</xsl:if>
	<xsl:text>&#x0A;);&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
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
	  
      	<xsl:text>&#x0A;drop operator </xsl:text>
        <xsl:value-of select="skit:dbquote(@schema)"/>
        <xsl:text>.</xsl:text>
        <xsl:value-of select="@name"/>
      	<xsl:text>(</xsl:text>

	<xsl:choose>
	  <xsl:when test="arg[@position='left']">
            <xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
				               arg[@position='left']/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>, </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='right']">
            <xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
				               arg[@position='right']/@name)"/>
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
