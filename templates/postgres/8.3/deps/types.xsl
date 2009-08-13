<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Domains -->
  <xsl:template match="domain">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="tbs_fqn" select="concat('language.', 
					  $parent_core, '.', @name)"/>
    <dbobject type="domain" name="{@name}" qname='"{@name}"'
	      fqn="{$tbs_fqn}">
      <xsl:if test="(@basetype_schema != 'pg_toast') and
		    (@basetype_schema != 'pg_catalog') and
		    (@basetype_schema != 'information_schema')">
	<dependencies>
          <dependency fqn="{concat('type.', 
			   ancestor::database/@name, '.',
			   @basetype_schema, '.', @basetype)}"/>
	</dependencies>
      </xsl:if>

      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core"
			  select="concat($parent_core, '.', @name)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- Generate a dependency for a given type, ignoring built-ins -->
  <xsl:template name="TypeDep">
    <xsl:param name="ignore" select="NONE"/>
    <xsl:param name="type_name" select="@type"/>
    <xsl:param name="type_schema" select="@schema"/>
    <xsl:if test="$type_schema != 'pg_catalog'"> 
      <!-- Ignore builtin types TODO: Make the test below use xsl:if -->
      <xsl:choose>
	<xsl:when test="($ignore/@schema = $type_schema) and
	  ($ignore/@name = $type_name)"/>
	<xsl:otherwise>
	  <dependency fqn="{concat('type.', ancestor::database/@name,
			   '.', $type_schema, '.', $type_name)}"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <!-- Types -->
  <xsl:template match="type">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="type_fqn" select="concat('type.', 
					    $parent_core, '.', @name)"/>
    <dbobject type="type" name="{@name}"
	      fqn="{$type_fqn}" qname='"{@name}"'>
      <dependencies>
	<xsl:for-each select="handler-function">
	  <dependency fqn="{concat('function.', ancestor::database/@name, 
			           '.', @signature)}"/>
	</xsl:for-each>

	<xsl:for-each select="column">
          <xsl:if test="(@type_schema != 'pg_toast') and
			(@type_schema != 'pg_catalog') and
			(@type_schema != 'information_schema')">
            <dependency fqn="{concat('type.', 
			     ancestor::database/@name, '.',
			     @type_schema, '.', @type)}"/>
          </xsl:if>
	</xsl:for-each>
      </dependencies>

      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @name)"/>
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
