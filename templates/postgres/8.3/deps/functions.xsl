<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Functions -->
  <xsl:template match="function">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="function_fqn" 
		  select="concat('function.', 
			  ancestor::database/@name, '.', @signature)"/>
    <dbobject type="function" fqn="{$function_fqn}"
	      name="{@name}" qname="{@qname}">
      <dependencies>

	<xsl:if test="(@language != 'c') and (@language != 'internal')
	  and (@language != 'sql')">
	  <dependency fqn="{concat('language.', 
			   ancestor::database/@name, '.', @language)}"/>
	</xsl:if>

	<xsl:for-each select="result">
	  <xsl:call-template name="TypeDep">
	    <xsl:with-param name="ignore" select="../handler_for"/>
	  </xsl:call-template>
	</xsl:for-each>

	<xsl:for-each select="handler_for[@following]">
	  <xsl:call-template name="FuncDep">
	    <xsl:with-param name="fqn" select="@following"/>
	    <xsl:with-param name="schema" select="@following_schema"/>
	  </xsl:call-template>
	</xsl:for-each>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @signature)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
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
