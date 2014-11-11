<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">

  <!-- This stylesheet performs post-processing of diffs.  It is needed
       because during ddl operations the object hierarchy will have
       been lost, and sometimes objects need to know about their
       parents.  This is particularly true for columns within tables.  -->
  <xsl:template match="*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="table/dbobject[column]">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:attribute name="parent-type">
	<xsl:value-of select="'table'"/>
      </xsl:attribute>
      <xsl:attribute name="parent-qname">
	<xsl:value-of select="../../@qname"/>
      </xsl:attribute>
      <xsl:attribute name="parent-diff">
	<xsl:value-of select="../../@diff"/>
      </xsl:attribute>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>

