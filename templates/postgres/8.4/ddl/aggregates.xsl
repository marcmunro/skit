<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="aggregate_header">
    <xsl:value-of 
	select="concat('aggregate ', skit:dbquote(@schema,@name),
		       '(')"/>
    <xsl:choose>
      <xsl:when test="basetype/@name">
	<xsl:value-of 
	    select="skit:dbquote(basetype/@schema,basetype/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>*</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>)</xsl:text>
  </xsl:template>

  <xsl:template match="aggregate" mode="build">
    <xsl:text>&#x0A;create </xsl:text>
    <xsl:call-template name="aggregate_header"/>
    <xsl:value-of
	select="concat(' (&#x0A;  sfunc = ', 
		       skit:dbquote(transfunc/@schema,transfunc/@name),
		       ',&#x0A;  stype = ',
		       skit:dbquote(transtype/@schema,transtype/@name))"/>
    <xsl:if test="@initcond">
      <xsl:value-of
	  select="concat(',&#x0A;  initcond = ', @initcond)"/>
    </xsl:if>
    <xsl:if test="finalfunc">
      <xsl:value-of 
	  select="concat(',&#x0A;  finalfunc = ',
                         skit:dbquote(finalfunc/@schema,finalfunc/@name))"/>
    </xsl:if>
    <xsl:if test="sortop">
      <xsl:value-of 
	  select="concat(',&#x0A;  sortop = ',
		         skit:dbquote(sortop/@schema,sortop/@name))"/>
    </xsl:if>
    <xsl:text>);&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="aggregate" mode="drop">
    <xsl:text>&#x0A;drop </xsl:text>
    <xsl:call-template name="aggregate_header"/>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="aggregate" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:text>&#x0A;alter </xsl:text>
      <xsl:call-template name="aggregate_header"/>
      <xsl:value-of 
	  select="concat(' owner to ', @owner, ';&#x0A;')"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

