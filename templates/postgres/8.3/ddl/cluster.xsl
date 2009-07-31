<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Coding standard for postgres DDL generated from skit:
       Each object action is preceded by a blank line and
       followed by a blank line.  This means that there will be 2 lines
       between the creation of each different object.
    -->

  <xsl:template match="dbobject/cluster">
    <xsl:if test="../@action='build'">
      <print>psql -d postgres &lt;&lt;&apos;CLUSTEREOF&apos;&#x0A;</print>
    </xsl:if>	

    <xsl:if test="../@action='arrive'">
      <print>psql -d postgres &lt;&lt;&apos;CLUSTEREOF&apos;&#x0A;</print>
    </xsl:if>	

    <xsl:if test="../@action='depart'">
      <print>&#x0A;CLUSTEREOF&#x0A;&#x0A;</print>
    </xsl:if>	

    <xsl:if test="../@action='drop'">
      <print>&#x0A;CLUSTEREOF&#x0A;&#x0A;</print>
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
