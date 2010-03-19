<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <xsl:template match="/">
    <scatter>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates/>
      </xsl:copy>
    </scatter>
  </xsl:template>

  <!-- Templates for copying the contents of dbobjects -->
  <xsl:template match="dependencies" mode="copy_dbobject"/>

  <xsl:template match="dbobject" mode="copy_dbobject">
    <xsl:apply-templates select="."/>
  </xsl:template>

  <xsl:template match="*" mode="copy_dbobject">
    <scatter>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates mode="copy_dbobject"/>
      </xsl:copy>
    </scatter>
  </xsl:template>

  <xsl:template match="dbobject">
    <xsl:if test="@type!='dbincluster'">
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<contents>
	  <xsl:attribute name="path">
	    <xsl:choose>
	      <xsl:when test="@type='cluster'">
		<xsl:value-of select="'cluster/'"/>
	      </xsl:when>	
	      <xsl:when test="@type='database'">
		<xsl:value-of select="'cluster/database/'"/>
	      </xsl:when>	
	      <xsl:when test="@type='grant'">
		<xsl:value-of select="'cluster/grants/'"/>
	      </xsl:when>	
	      <xsl:when test="@type='role'">
		<xsl:value-of select="'cluster/roles/'"/>
	      </xsl:when>	
	      <xsl:when test="@type='tablespace'">
		<xsl:value-of select="'cluster/tablespaces/'"/>
	      </xsl:when>	
	      <xsl:when test="@type='schema'">
		<xsl:value-of select="concat('cluster/database/',
				      ../../@name,
				      '/schemata/')"/>
	      </xsl:when>	
	      <xsl:otherwise>
		<xsl:value-of select="'cluster/UNHANDLED/'"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:attribute>
	  <xsl:attribute name="name">
	    <xsl:choose>
	      <xsl:when test="@type='cluster'">
		<xsl:value-of select="'cluster.skt'"/>
	      </xsl:when>	
	      <xsl:otherwise>
		<xsl:value-of select="concat(@name, '.skt')"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:attribute>
	  <xsl:apply-templates mode="copy_dbobject"/>
	</contents>
      </xsl:copy>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

