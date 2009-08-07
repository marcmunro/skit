<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Roles: roles are cluster level objects. -->
  <xsl:template match="role">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="role_name" select="concat($parent_core, '.', @name)"/>
    <dbobject type="role" name="{@name}" qname='"{@name}"'
	      fqn="{concat('role.', $role_name)}">
      <dependencies>
	<dependency fqn="cluster"/>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="$role_name"/>
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
