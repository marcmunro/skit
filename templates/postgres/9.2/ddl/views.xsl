<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="build_viewbase">
    <xsl:value-of 
	select="concat('&#x0A;create or replace view ', ../@qname,
		       ' as select &#x0A;')"/>
    <xsl:for-each select="column">
      <xsl:value-of 
	  select="concat('    null::', skit:dbquote(@type_schema, @type),
		         ' as ', @name)"/>
      <xsl:if test="position() != last()">
	<xsl:text>,&#x0A;</xsl:text>
      </xsl:if>
    </xsl:for-each>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="view" mode="build">
    <xsl:choose>
      <xsl:when test="../@type='viewbase'">
	<xsl:call-template name="build_viewbase"/>
      </xsl:when>
      <xsl:otherwise>
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
	<xsl:value-of 
	    select="concat('&#x0A;create or replace view ', ../@qname,
		           $options, ' as&#x0A;  ', source/text(), '&#x0A;')"/>

	<xsl:for-each select="column[@default]">
	  <xsl:value-of select="concat('alter view ', ../../@qname,
				       ' alter column ', skit:dbquote(@name),
				       ' set default ', @default, 
				       ';&#x0A;')"/>
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="view" mode="drop">
    <xsl:choose>
      <xsl:when test="../@type='viewbase'">
	<xsl:call-template name="build_viewbase"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of 
	    select="concat('&#x0A;drop view ', ../@qname, ';&#x0A;')"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="view" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:for-each select="../attribute[@name='owner']">
	<xsl:value-of 
	    select="concat('&#x0A;alter view ', ../@qname,
		           ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="view" mode="diff">
    <xsl:for-each select="../element[@type='column']/
                          attribute[@name='default']">
      <do-print/>
      <xsl:value-of select="concat('&#x0A;alter view ', ../../@qname,
				   ' alter column ', 
				   skit:dbquote(../column/@name),
				   ' set default ', @new, 
				   ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>


