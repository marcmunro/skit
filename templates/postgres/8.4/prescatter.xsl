<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <!-- This runs as a precursor to scatter.xsl.  It combines various
       dbobjects into single dbobject listings in order to reduce the
       number of distinct objects and group related things together.

       Specifically grants are combined with the objects that they
       provide access to.
    -->

  <xsl:template match="/*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <!-- We may as well remove the dependency elements now.  We won't be
       needing them.
  -->
  <xsl:template match="dbobject/dependencies"/>

  <!-- ditto for context elements.  -->
  <xsl:template match="dbobject/context"/>

  <!-- and extra-schema-priv-elements.  -->
  <xsl:template match="dbobject/extra-schema-privs"/>

  <xsl:template match="dbobject[@type='grant']">
    <!-- We remove the dbobject for grants. -->
    <xsl:apply-templates/>
  </xsl:template>

  <!-- We remove this entire dbobject -->
  <xsl:template match="dbobject[@type='dbincluster']"/>

  <xsl:template match="*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>

