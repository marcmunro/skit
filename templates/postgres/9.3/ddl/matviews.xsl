<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="materialized_view" mode="build">
    <xsl:variable name="options">
      <xsl:if test="option">
	<xsl:text>&#x0A;  with (</xsl:text>
	<xsl:for-each select="option">
	  <xsl:if test="position()!=1">
	    <xsl:text>,&#x0A;         </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="concat(@name, ' = ', @value)"/>
	</xsl:for-each>
	<xsl:text>)</xsl:text>
      </xsl:if>
    </xsl:variable>
    <xsl:variable name="tablespace">
      <xsl:if test="@tablespace">
	<xsl:value-of select="concat('&#x0A;  tablespace ', @tablespace)"/>
      </xsl:if>
    </xsl:variable>

    <xsl:value-of 
	select="concat('&#x0A;create materialized view ', ../@qname,
		       $options, $tablespace,
		       ' as&#x0A;  ', source/text())"/>
    <xsl:if test="@is_populated!='t'">
      <xsl:text>&#x0A;  with no data</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="materialized_view" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop materialized view ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="materialized_view" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:for-each select="../attribute[@name='owner']">
	<xsl:value-of 
	    select="concat('&#x0A;alter materialized view ', ../@qname,
		           ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="materialized_view" mode="diff">
    <xsl:if test="../attribute[@name='is_populated']">
      <do-print/>
      <xsl:choose>
	<xsl:when test="@is_populated='t'">
	  <xsl:value-of select="concat('refresh materialized view ',
	                               ../@qname, ';&#x0A;')"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="concat('refresh materialized view ',
	                               ../@qname, 
				       ' with no data;&#x0A;')"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>


