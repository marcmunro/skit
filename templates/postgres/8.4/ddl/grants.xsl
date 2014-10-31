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
	<xsl:if test="not (@automatic='yes')">
	  <!-- Don't explicitly grant privs that are automatically
	       generated. --> 
	  <xsl:call-template name="build_objectgrant"/>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="grant" mode="diff">
    <xsl:choose>
      <xsl:when test="../@subtype='role'">
	<do-print/>
	<xsl:text>NOT IMPLEMENTED - CODE IN DDL/GRANTS.XSL</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:choose>
	  <xsl:when test="../attribute[@name='with_grant']">
	    <do-print/>
	    <xsl:choose>
	      <xsl:when test="../attribute[@name='with_grant' and @new='yes']">
		<xsl:call-template name="build_objectgrant"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:text>NOT IMPLEMENTED - CODE2 IN DDL/GRANTS.XSL</xsl:text>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:when>
	  <xsl:when test="../attribute[@name='automatic']">
	    <!-- Nothing very important has changed here, just whether
	         the grant was automatic or not.  If it has changed from
		 being automatic to not, we can explicitly preform the
		 grant and all will be well.  If the change is the
		 other way, there is nothing we can do - not that there
		 will be any functional difference in the way the
		 database behaves.  The only reason for doing anything
		 at all here, is to try to keep the catalogs of the
		 different databases in step (ie neither database will
		 now show an empty acl field).  -->

	    <xsl:if test="../attribute[@name='automatic' and @old='yes']">
	      <do-print/>
	      <xsl:call-template name="build_objectgrant"/>
	    </xsl:if>
	  </xsl:when>
	</xsl:choose>
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
	<xsl:if test="not (@automatic='yes')">
	  <!-- Don't explicitly revoke privs that were automatically
	       generated. --> 

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

