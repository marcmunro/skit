<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">

  <!-- This stylesheet adds dependency definitions to dbobjects unless
       they appear to already exist. -->

  <xsl:template match="/">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:choose>
	<xsl:when test="//dbobject/dependencies">
	  <xsl:apply-templates mode="copy"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:attribute name="x">y</xsl:attribute>
	  <xsl:apply-templates/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
  </xsl:template>

  <!-- This template handles copy-only mode.  This is used when we
       discover that a document already has dependencies defined -->
  <xsl:template match="*" mode="copy">
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy"/>
    </xsl:copy>
  </xsl:template>

  <!-- Anything not matched explicitly will match this and be copied 
       This handles db objects, dependencies, etc -->
  <xsl:template match="*">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates>
	<xsl:with-param name="parent_core" select="$parent_core"/>
      </xsl:apply-templates>
    </xsl:copy>
  </xsl:template>








  <!-- Handle the database and the cluster objects.  The cluster creates
  an interesting problem: the database can only be created from within a
  connection to a different database within the cluster.  So, to create
  a database we must visit the cluster.  To create a database object, we
  must visit the database.  But in visiting the database, we do not want
  to visit the cluster.  To solve this, we create two distinct database
  objects, one within the cluster for db creation purposes, and one not
  within the cluster which depends on the first.  The first object we
  will call dbincluster. -->

  <xsl:template match="cluster">
    <dbobject type="cluster" root="true" visit="true"
	      name="{@name}" fqn="{concat('cluster.', @name)}"
	      qname='"{@name}"'>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
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
