<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skituls="http://www.bloodnok.com/xml/skituls"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  version="1.0">

  <xsl:template match="/">
    <skituls:stylesheet>
      <xsl:for-each select="//cluster">
	<xsl:apply-templates select="."/>
      </xsl:for-each>	
    </skituls:stylesheet>
  </xsl:template>

  <!-- OLD FILTER TO REMOVE DATABASE CONTENTS 
  <xsl:template match="database">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:for-each select="//*/db_visit">
	<xsl:apply-templates select="*"/>
      </xsl:for-each>	
    </xsl:copy>
  </xsl:template>	-->

  <!-- Eliminate db_visit objects which are artificially created
    by add_deps.xsl -->
  <xsl:template match="db_visit"/>

  <!-- Eliminate dependencies and their contents -->
  <xsl:template match="dependencies"/>

  <!-- Eliminate dependency convenience objects -->
  <xsl:template match="allroles"/>
  <xsl:template match="alltbs"/>

  <!-- Ignore dbobjects but not their contents -->
  <xsl:template match="dbobject">
    <xsl:for-each select="*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>
    
  <!-- Ignore column dbobjects as the column info appears at both the
       table and column levels. -->
  <xsl:template match="dbobject/column">
    <xsl:for-each select="*">
      <xsl:apply-templates select="."/>
    </xsl:for-each>	
  </xsl:template>
    
  <!-- Main template for database objects -->
  <xsl:template match="*">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
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
