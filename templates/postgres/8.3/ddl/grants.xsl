<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@subtype='role']/grant">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="set_owner_from"/>

        <xsl:value-of 
	    select="concat('grant ', skit:dbquote(@priv),
		           ' to ', skit:dbquote(@to))"/>
	<xsl:if test="@with_admin = 'yes'">
          <xsl:text> with admin option</xsl:text>
	</xsl:if>
        <xsl:text>;&#x0A;</xsl:text>

	<xsl:call-template name="reset_owner_from"/>
      </print>
    </xsl:if>
  
    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of 
	    select="concat('revoke ', skit:dbquote(@priv),
		           ' from ', skit:dbquote(@to), ';&#x0A;')"/>

	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>
    
  </xsl:template>

  <!-- Object-level grants -->
  <xsl:template match="dbobject[@subtype!='role']/grant">
    <xsl:if test="../@action='build'">
      <!-- If this is an automatic grant and we are not doing a diff, 
           we can avoid making the grant.  -->
      <xsl:if test="../@diff or not (@automatic='yes')">
	<print>
	  <xsl:call-template name="set_owner_from"/>
	
	  <xsl:value-of 
	      select="concat('grant ', @priv, ' on ')"/>
	  <xsl:choose>
	    <xsl:when test="../@subtype = 'view'">
	      <xsl:text>table</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="../@subtype"/>
	    </xsl:otherwise>
	  </xsl:choose>
	  <xsl:value-of 
	      select="concat(' ', ../@on, ' to ',
		             skit:dbquote(@to))"/>
	  <xsl:if test="@with_grant = 'yes'">
	    <xsl:text> with grant option</xsl:text>
	  </xsl:if>
	  <xsl:text>;&#x0A;</xsl:text>
	
	  <xsl:call-template name="reset_owner_from"/>
	</print>
      </xsl:if>
    </xsl:if>
    
  
    <xsl:if test="../@action='drop'">
      <!-- If this is an automatic grant we can avoid doing the revoke.  -->
      <xsl:if test="../@diff or not (@automatic='yes')">
	<print>
	  <xsl:call-template name="set_owner_from"/>

	  <xsl:value-of 
	      select="concat('revoke ', @priv, ' on ')"/>
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

	  <xsl:call-template name="reset_owner_from"/>
        </print>
      </xsl:if>
    </xsl:if>
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

