<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

    <!-- Cluster and database:
       Handle the database and the cluster objects.  The cluster creates
       an interesting problem: the database can only be created from
       within a connection to a different database within the cluster.
       So, to create a database we must visit the cluster.  To create a
       database object, we must visit the database.  But in visiting the
       database, we do not want to visit the cluster.  To solve this, we
       create two distinct database objects, one within the cluster for
       db creation purposes, and one not within the cluster which
       depends on the first.  The first object we will call dbincluster,
       the second will be database.  -->

  <xsl:template match="cluster">
    <dbobject type="cluster" visit="true"
	      name="cluster" fqn="cluster">
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="'cluster'"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>
  </xsl:template>

  <!-- The dbincluster object (responsible for actual database creation) -->
  <xsl:template match="database">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="fqn" select="concat('dbincluster.', 
				     $parent_core, '.', @name)"/>
    <dbobject type="dbincluster" name="{@name}" 
	      qname="{skit:dbquote(@name)}" fqn="{$fqn}">
      <dependencies>
	<xsl:if test="@tablespace != 'pg_default'">
	  <dependency fqn="{concat('tablespace.', $parent_core, 
			   '.', @tablespace)}"/>
	</xsl:if>
	<xsl:if test="@owner != 'public'">
	  <dependency fqn="{concat('role.', $parent_core, 
			   '.', @owner)}"/>
	</xsl:if>
      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:copy-of select="comment"/>
      </xsl:copy>
    </dbobject>
  </xsl:template>


  <!-- The database object, from which other database objects may be
       manipulated.  This is the only template called with a mode of
       "database" -->
  <xsl:template match="database" mode="database">
    <dbobject type="database" visit="true" name="{@name}" 
	      qname="{skit:dbquote(@name)}" 
	      fqn="{concat('database.cluster.', @name)}">
      <dependencies>
	<dependency fqn="{concat('dbincluster.cluster.', @name)}"/>
      </dependencies>
      <database name="{@name}">
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" select="@name"/>
	</xsl:apply-templates>
      </database>
    </dbobject>
  </xsl:template>

  <xsl:include href="skitfile:deps/roles.xsl"/>
  <xsl:include href="skitfile:deps/tablespaces.xsl"/>
  <xsl:include href="skitfile:deps/grants.xsl"/>
  <xsl:include href="skitfile:deps/languages.xsl"/>
  <xsl:include href="skitfile:deps/schemata.xsl"/>
  <xsl:include href="skitfile:deps/types.xsl"/>
  <xsl:include href="skitfile:deps/functions.xsl"/>
  <xsl:include href="skitfile:deps/aggregates.xsl"/>
  <xsl:include href="skitfile:deps/casts.xsl"/>
  <xsl:include href="skitfile:deps/operators.xsl"/>
  <xsl:include href="skitfile:deps/operator_classes.xsl"/>
  <xsl:include href="skitfile:deps/operator_families.xsl"/>

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
