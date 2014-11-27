<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template name="process-option">
    <xsl:param name="option"/>
    <xsl:param name="comma"/>
    <xsl:if test="$comma">
      <xsl:text>,&#x0A;        </xsl:text>
    </xsl:if>
    <xsl:value-of select="substring-before($option, '=')"/>
    <xsl:text> &apos;</xsl:text>
    <xsl:value-of select="substring-after($option, '=')"/>
    <xsl:text>&apos;</xsl:text>
  </xsl:template>

  <xsl:template name="process-options">
    <xsl:param name="options"/>
    <xsl:param name="comma" select="''"/>
    <xsl:choose>
      <xsl:when test="substring($options,1,1)='&quot;'">
	<xsl:call-template name="process-option">
	  <xsl:with-param name="option">
	    <xsl:choose>
	      <xsl:when test="contains($options, '&quot;,')">
		<xsl:value-of 
		    select="substring-before(substring($options,2),
			                     '&quot;,')"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:value-of 
		    select="substring($options,2,
			              string-length($options) - 2)"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:with-param>
	  <xsl:with-param name="comma" select="$comma"/>
	</xsl:call-template>
	<xsl:if test="contains($options, '&quot;,')">
	  <xsl:call-template name="process-options">
	    <xsl:with-param name="options" 
			    select="substring-after($options, '&quot;,')"/>
	    <xsl:with-param name="comma" select="','"/>
	  </xsl:call-template>
	</xsl:if>
      </xsl:when>
      <xsl:otherwise>
	<xsl:call-template name="process-option">
	  <xsl:with-param name="option">
	    <xsl:choose>
	      <xsl:when test="contains($options, ',')">
		<xsl:value-of 
		    select="substring-before($options, ',')"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:value-of select="$options"/>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:with-param>
	  <xsl:with-param name="comma" select="$comma"/>
	</xsl:call-template>
	<xsl:if test="contains($options, ',')">
	  <xsl:call-template name="process-options">
	    <xsl:with-param name="options" 
			    select="substring-after($options, ',')"/>
	    <xsl:with-param name="comma" select="','"/>
	  </xsl:call-template>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="foreign_data_wrapper" mode="build">
    <xsl:value-of 
	select="concat('create foreign data wrapper ', ../@qname,
                       '&#x0A;    ')"/>
    <xsl:choose>
      <xsl:when test="@validator_proc">
	<xsl:value-of 
	    select="concat('validator ', 
		           skit:dbquote(@validator_schema,@validator_proc))"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>no validator</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="@options">
      <xsl:variable name="options" 
		    select="substring(@options, 2, 
			              string-length(@options) - 2)"/>
      <xsl:text>&#x0A;    options(</xsl:text>
      <xsl:call-template name="process-options">
	<xsl:with-param name="options" select="$options"/>
      </xsl:call-template>
      <xsl:text>)</xsl:text>
    </xsl:if>
     <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="foreign_data_wrapper" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop foreign data wrapper ', 
		       ../@qname, ';&#x0A;')"/>
  </xsl:template>

</xsl:stylesheet>

