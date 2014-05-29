<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="operator-signature">
    <xsl:value-of 
	select="concat('operator ',
		        skit:dbquote(@schema),
			'.', @name, '(')"/>

    <xsl:choose>
      <xsl:when test="arg[@position='left']">
	<xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
			                   arg[@position='left']/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>none</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    
    <xsl:text>, </xsl:text>
    <xsl:choose>
      <xsl:when test="arg[@position='right']">
	<xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
				           arg[@position='right']/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>none</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>)</xsl:text>
  </xsl:template>

  <xsl:template match="operator" mode="build">
    <xsl:value-of 
	select="concat('&#x0A;create operator ', skit:dbquote(@schema),
		       '.', @name, ' (&#x0A;  leftarg = ')"/>
    <xsl:choose>
      <xsl:when test="arg[@position='left']">
	<xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
			                   arg[@position='left']/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>none</xsl:text>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:text>,&#x0A;  rightarg = </xsl:text>
    <xsl:choose>
      <xsl:when test="arg[@position='right']">
	<xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
			                   arg[@position='right']/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>none</xsl:text>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:text>,&#x0A;  procedure = </xsl:text>
    <xsl:value-of 
	select="skit:dbquote(procedure/@schema,procedure/@name)"/>
    <xsl:if test="commutator">
      <xsl:text>,&#x0A;  commutator = operator(</xsl:text>
      <xsl:value-of 
	  select="concat(commutator/@schema, '.', commutator/@name, ')')"/>
    </xsl:if>
    <xsl:if test="negator">
      <xsl:text>,&#x0A;  negator = operator(</xsl:text>
      <xsl:value-of 
	  select="concat(negator/@schema, '.', negator/@name, ')')"/>
    </xsl:if>
    <xsl:if test="restrict">
      <xsl:text>,&#x0A;  restrict = </xsl:text>
      <xsl:value-of select="skit:dbquote(restrict/@schema,restrict/@name)"/>
    </xsl:if>
    <xsl:if test="join">
      <xsl:text>,&#x0A;  join = </xsl:text>
      <xsl:value-of select="skit:dbquote(join/@schema,join/@name)"/>
    </xsl:if>
    <xsl:if test="@hashes">
      <xsl:text>,&#x0A;  hashes</xsl:text>
    </xsl:if>
    <xsl:if test="@merges">
      <xsl:text>,&#x0A;  merges</xsl:text>
    </xsl:if>
    <xsl:text>&#x0A;);&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="operator" mode="drop">
    <xsl:text>drop </xsl:text>
    <xsl:call-template name="operator-signature"/>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="operator" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:variable name="sig">
	<xsl:call-template name="operator-signature"/>
      </xsl:variable>
      <xsl:for-each select="../attribute">
	<xsl:if test="@name='owner'">
	  <xsl:value-of 
	      select="concat('&#x0A;alter ', $sig, ' owner to ', 
			     skit:dbquote(@new), ';&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <!-- We do not use the default dbobject handling as the commentdiff
       template needs to take a parameter in this case. -->
  <xsl:template match="dbobject[@action='diff']/operator">
    <print conditional="yes">
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>
      <xsl:call-template name="commentdiff">
	<xsl:with-param name="sig">
	  <xsl:call-template name="operator-signature"/>
	</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="reset_owner"/>
    </print>
  </xsl:template>
</xsl:stylesheet>
