<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/constraint[@type='foreign key']">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:text>&#x0A;alter table only </xsl:text>
        <xsl:value-of select="../@table_qname"/>
	<xsl:text>&#x0A;  add constraint </xsl:text>
	<xsl:value-of select="skit:dbquote(@name)"/>
	<xsl:text>&#x0A;  foreign key(</xsl:text>
	<xsl:for-each select="column">
	  <xsl:if test="position() != '1'">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="skit:dbquote(@name)"/>
	</xsl:for-each>
	<xsl:text>)&#x0A;  references </xsl:text>
	<xsl:value-of select="skit:dbquote(reftable/@refschema, 
			                   reftable/@reftable)"/>
	<xsl:text>(</xsl:text>
	<xsl:for-each select="reftable/column">
	  <xsl:if test="position() != '1'">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="skit:dbquote(@name)"/>
	</xsl:for-each>
	<xsl:text>);&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="set_owner"/>
	<xsl:text>&#x0A;alter table only </xsl:text>
        <xsl:value-of select="../@table_qname"/>
	<xsl:text>&#x0A;  drop constraint </xsl:text>
	<xsl:value-of select="skit:dbquote(@name)"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
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
