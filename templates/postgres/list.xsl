<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  version="1.0">

  <xsl:template match="/*">
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<printable/>  <!-- Simple tag to show whether the output doc is
			   printable as other than an xml document -->
	<xsl:apply-templates>
	  <xsl:with-param name="depth" select="1"/>
	</xsl:apply-templates>
      </xsl:copy>
  </xsl:template>

  <xsl:template match="dbobject">
    <xsl:param name="depth"/>
    <xsl:choose>
      <xsl:when test="/*/params[@grants='true']">
	<xsl:call-template name="printObject">
	  <xsl:with-param name="depth" select="$depth"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
	<!-- Do not list grants -->
	<xsl:choose>
	  <xsl:when test="self::*[@type='grant']"/>
	  <xsl:otherwise>
	    <xsl:call-template name="printObject">
	      <xsl:with-param name="depth" select="$depth"/>
	    </xsl:call-template>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="printObject">
    <xsl:param name="depth"/>
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <print>
	<xsl:attribute name="text">
	  <xsl:if test="@type!='cluster' and @type != 'database'">
	    <xsl:value-of 
	       select="substring('                            ', 1, $depth*2)"/>
	  </xsl:if>

	  <xsl:value-of select="@type"/>
	  <xsl:text>=</xsl:text>
	  <xsl:value-of select="@name"/>
	  <xsl:text>|</xsl:text>
	  <xsl:value-of select="@fqn"/>
	  <xsl:if test="@action">
	    <xsl:text>|</xsl:text>
	    <xsl:value-of select="@action"/>
	  </xsl:if>
	  <xsl:text>&#xA;</xsl:text>
	</xsl:attribute>
      </print>
      <xsl:choose>
	<xsl:when test="@type='cluster'">
	  <xsl:apply-templates>
	    <xsl:with-param name="depth" select="$depth"/>
	  </xsl:apply-templates>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates>
	    <xsl:with-param name="depth" select="$depth+1"/>
	  </xsl:apply-templates>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
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
