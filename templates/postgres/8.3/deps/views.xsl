<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- views -->
  <xsl:template match="view">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="view_fqn" 
		  select="concat('view.', $parent_core, '.', @name)"/>
    <dbobject type="view" fqn="{$view_fqn}" name="{@name}"
	      qname="{skit:dbquote(@schema,@name)}">

      <dependencies>
	<!-- Add explicitly identified dependencies -->
	<xsl:for-each select="depends[@function]">
	  <xsl:choose>
	    <xsl:when test="@cast">
	      <dependency fqn="{concat('cast.', 
			               ancestor::database/@name, 
				       '.', @cast)}"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <dependency fqn="{concat('function.', 
			                ancestor::database/@name, 
					'.', @function)}"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:for-each>
	<xsl:for-each select="depends[@table]">
	  <dependency fqn="{concat('table.', ancestor::database/@name, 
			   '.', @schema, '.', @table)}"/>
	</xsl:for-each>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.cluster.', @owner)}"/>
	</xsl:if>
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
