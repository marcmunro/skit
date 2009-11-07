<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/operator_class">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create operator class </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text>&#x0A;  </xsl:text>
	<xsl:if test="@is_default = 't'">
	  <xsl:text>default </xsl:text>
	</xsl:if>
	<xsl:text>for type </xsl:text>
        <xsl:value-of select="skit:dbquote(@intype_schema,@intype_name)"/>
	<xsl:text> using </xsl:text>
        <xsl:value-of select="@method"/>
	<xsl:if test="(@name != @family) or (@schema != @family_schema)">
	  <xsl:text> family </xsl:text>
          <xsl:value-of select="skit:dbquote(@family_schema,@family)"/>
	</xsl:if >
	<xsl:text> as</xsl:text>
	<xsl:for-each select="opclass_operator">
	  <xsl:sort select="@strategy"/>
	  <xsl:if test="position() != 1">
	    <xsl:text>,</xsl:text>
	  </xsl:if>
	  <xsl:text>&#x0A;  operator </xsl:text>
	  <xsl:value-of select="@strategy"/>
	  <xsl:text> </xsl:text>
	  <xsl:value-of select="skit:dbquote(@schema)"/>
	  <xsl:text>.</xsl:text>
	  <xsl:value-of select="@name"/>
	  <xsl:if test="(arg[@position='left']/@name != 
                             arg[@position='right']/@name) or
			(arg[@position='left']/@schema != 
                             arg[@position='right']/@schema)">
	    <xsl:text>(</xsl:text>
	    <xsl:value-of
	       select="skit:dbquote(arg[@position='left']/@schema,
		                    arg[@position='left']/@name)"/>
	    <xsl:text>,</xsl:text>
	    <xsl:value-of
	       select="skit:dbquote(arg[@position='right']/@schema,
		                    arg[@position='right']/@name)"/>
	    <xsl:text>)</xsl:text>
	  </xsl:if>
	</xsl:for-each>
	<xsl:for-each select="opclass_function">
	  <xsl:sort select="@proc_num"/>
	  <xsl:text>,&#x0A;  function </xsl:text>
	  <xsl:value-of select="@proc_num"/>
	  <xsl:text> </xsl:text>
	  <xsl:value-of select="skit:dbquote(@schema,@name)"/>
	  <xsl:text>(</xsl:text>
	  <xsl:value-of 
	     select="skit:dbquote(params/param[@position='1']/@schema,
		                  params/param[@position='1']/@type)"/>
	  <xsl:text>,</xsl:text>
	  <xsl:value-of 
	     select="skit:dbquote(params/param[@position='2']/@schema,
		                  params/param[@position='2']/@type)"/>
	  <xsl:text>)</xsl:text>
	</xsl:for-each>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>
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
	  
	<xsl:text>drop operator class </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:text> using </xsl:text>
        <xsl:value-of select="@method"/>
	<xsl:text>;&#x0A;</xsl:text>

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
