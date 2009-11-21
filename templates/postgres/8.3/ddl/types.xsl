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
	<xsl:call-template name="set_owner"/>

        <xsl:text>create type </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:choose>
	  <xsl:when test="@subtype='enum'">
	    <xsl:text> as enum (&#x0A;  </xsl:text>
	    <xsl:for-each select="label">
	      <xsl:if test="position() != 1">
		<xsl:text>,&#x0A;  </xsl:text>
	      </xsl:if>
              <xsl:value-of select="@label"/>
	    </xsl:for-each>
            <xsl:text>);&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:when test="@subtype='basetype'">
            <xsl:text> (&#x0A;  input = </xsl:text>
            <xsl:value-of select="skit:dbquote(*[@type='input']/@schema,
			                       *[@type='input']/@name)"/>
            <xsl:text>,&#x0A;  output = </xsl:text>
            <xsl:value-of select="skit:dbquote(*[@type='output']/@schema,
				               *[@type='output']/@name)"/>
	    <xsl:if test="*[@type='send']">
              <xsl:text>,&#x0A;  send = </xsl:text>
              <xsl:value-of select="skit:dbquote(*[@type='send']/@schema,
				                 *[@type='send']/@name)"/>
	    </xsl:if>
	    <xsl:if test="*[@type='receive']">
              <xsl:text>,&#x0A;  receive = </xsl:text>
              <xsl:value-of select="skit:dbquote(*[@type='receive']/@schema,
				                 *[@type='receive']/@name)"/>
	    </xsl:if>
	    <xsl:if test="*[@type='analyze']">
              <xsl:text>,&#x0A;  analyze = </xsl:text>
              <xsl:value-of select="skit:dbquote(*[@type='analyze']/@schema,
				                 *[@type='analyze']/@name)"/>
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
	    <xsl:text>);&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:when test="@subtype='comptype'">
            <xsl:text> as (</xsl:text>
	    <xsl:for-each select="column">
	      <xsl:call-template name="column"/>
	    </xsl:for-each>
	    <xsl:text>);&#x0A;</xsl:text>
	    <xsl:for-each select="column/comment">
	      <xsl:text>&#x0A;comment on column </xsl:text>
	      <xsl:value-of select="../../../@qname"/>
	      <xsl:text>.</xsl:text>
	      <xsl:value-of select="skit:dbquote(../@name)"/>
	      <xsl:text> is&#x0A;</xsl:text>
	      <xsl:value-of select="text()"/>
	      <xsl:text>;&#x0A;</xsl:text>
	    </xsl:for-each>
	  </xsl:when>
	</xsl:choose>

	<xsl:apply-templates/>
	<xsl:call-template name="reset_owner"/>
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
