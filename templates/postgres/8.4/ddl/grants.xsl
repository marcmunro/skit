<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="build_rolegrant">
    <xsl:value-of 
	select="concat('grant ', skit:dbquote(@priv), 
		       ' to ', skit:dbquote(@to))"/>
    <xsl:if test="@with_admin = 'yes'">
      <xsl:text> with admin option</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template name="build_objectgrant">
    <xsl:value-of select="concat('grant ', @priv, ' on ')"/>
    <xsl:choose>
      <xsl:when test="../@subtype = 'view'">
	<xsl:text>table</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="../@subtype"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:value-of select="concat(' ', ../@on, ' to ', skit:dbquote(@to))"/>
    <xsl:if test="@with_grant = 'yes'">
      <xsl:text> with grant option</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="grant" mode="build">
    <xsl:choose>
      <xsl:when test="../@subtype='role'">
	<xsl:call-template name="build_rolegrant"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:if test="../@diff or not (@automatic='yes')">
	  <!-- In the other case we should do no ddl.  Not sure how best
	       to handle this, maybe a <noprint> element would be
	       useful. --> 
	  <xsl:call-template name="build_objectgrant"/>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="grant" mode="drop">
    <xsl:choose>
      <xsl:when test="../@subtype='role'">
	<xsl:value-of 
	    select="concat('&#x0A;revoke ', skit:dbquote(@priv),
		           ' from ', skit:dbquote(@to), ';&#x0A;')"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:if test="../@diff or not (@automatic='yes')">
	  <!-- In the other case we should do no ddl.  Not sure how best
	       to handle this, maybe a <noprint> element would be
	       useful. --> 

	  <xsl:value-of 
	      select="concat('&#x0A;revoke ', @priv, ' on ')"/>
	  <xsl:choose>
	    <xsl:when test="../@subtype = 'view'">
	      <xsl:text>table</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="../@subtype"/>
	    </xsl:otherwise>
	  </xsl:choose>
	  <xsl:value-of 
	      select="concat(' ', ../@on, ' from ',
		      skit:dbquote(@to), ';&#x0A;')"/>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="revoke">
    <xsl:call-template name="set_owner_from"/>
    <xsl:value-of 
	select="concat('revoke ', @priv, ' on ')"/>
    <xsl:choose>
      <xsl:when test="name(..) = 'view'">
	<xsl:text>table</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="name(..)"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:value-of 
	select="concat(' ', ../../@qname, ' from ', @to, ';&#x0A;')"/>
    <xsl:call-template name="reset_owner_from"/>
  </xsl:template>
</xsl:stylesheet>

