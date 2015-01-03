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

  <xsl:template name="revoke_rolegrant">
    <xsl:param name="admin-only" select="'no'"/>
    <xsl:variable name="adminoption-text">
      <xsl:if test="$admin-only='yes'">
	<xsl:value-of select="'admin option for '"/>
      </xsl:if>
    </xsl:variable>
    <xsl:value-of 
	select="concat('revoke ', $adminoption-text, skit:dbquote(@priv), 
		       ' from ', skit:dbquote(@to), ';&#x0A;')"/>
  </xsl:template>

  <xsl:template name="build_objectgrant">
    <xsl:choose>
      <xsl:when test="../@subtype = 'column'">
	<xsl:value-of select="concat('grant ', @priv, ' (', 
			             ../@on, ') on table ',
				     skit:dbquote(../@schema, ../@table),
				     ' to ', skit:dbquote(@to))"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="concat('grant ', @priv, ' on ')"/>
	<xsl:choose>
	<xsl:when test="../@subtype = 'view' or
			../@subtype = 'materialized_view'">
	    <xsl:text>table</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:call-template name="dbobject-typename">
	      <xsl:with-param name="typename" select="../@subtype"/>
	    </xsl:call-template>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:value-of select="concat(' ', ../@on, 
			             ' to ', skit:dbquote(@to))"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="@with_grant = 'yes'">
      <xsl:text> with grant option</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template name="revoke_objectgrant">
    <xsl:param name="grant-only" select="'no'"/>
    <xsl:variable name="grantoption-text">
      <xsl:if test="$grant-only='yes'">
	<xsl:value-of select="'grant option for '"/>
      </xsl:if>
    </xsl:variable>
    <xsl:variable name="objecttype">
      <xsl:choose>
	<xsl:when test="../@subtype = 'view' or
			../@subtype = 'materialized_view' or
			../@subtype = 'column'">
	  <xsl:text>table</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:call-template name="dbobject-typename">
	    <xsl:with-param name="typename" select="../@subtype"/>
	  </xsl:call-template>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="on">
      <xsl:choose>
	<xsl:when test="../@subtype = 'column'">
	  <xsl:value-of select="skit:dbquote(../@schema, ../@table)"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="../@on"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="colname">
      <xsl:if test="../@subtype = 'column'">
	<xsl:value-of select="concat('(', ../@on, ') ')"/>
      </xsl:if>
    </xsl:variable>
    <xsl:value-of select="concat('revoke ', $grantoption-text, @priv, 
			  ' on ', $objecttype, ' ', 
			  $on, ' from ', 
			  skit:dbquote(@to), ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="grant" mode="build">
    <xsl:choose>
      <xsl:when test="../@subtype='role'">
	<xsl:call-template name="build_rolegrant"/>
      </xsl:when>
      <xsl:otherwise>
	<do-print/>
	<xsl:choose>
	  <xsl:when test="@automatic='revoke'">

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
		select="concat(' ', ../@on, ' from ', @to, ';&#x0A;')"/>

	  </xsl:when>
	  <xsl:when test="not (@automatic='yes')">
	    <xsl:call-template name="build_objectgrant"/>
	  </xsl:when>
	  <!-- We will get here only when @automatic = 'yes'. -->
	  <xsl:when test="../@diff='rebuild'">  
	    <!-- No need for any action here.  This is just like the
		 build case for non-diffs (ie do nothing).  -->
	  </xsl:when>
	  <xsl:when test="../@diff">
	    <!-- If we are generating for a diff we should perform the
	         grant unless we know that the owner has changed, in
		 which case it is unnecessary.  -->
	    <xsl:call-template name="build_objectgrant"/>
	  </xsl:when>
	</xsl:choose>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="grant" mode="diff">
    <xsl:choose>
      <xsl:when test="../@subtype='role'">
	<do-print/>
	<xsl:choose>
	  <xsl:when test="../attribute[@name='with_admin' and @new='yes']">
	    <xsl:call-template name="build_rolegrant"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:call-template name="revoke_rolegrant">
	      <xsl:with-param name="admin-only" select="'yes'"/>
	    </xsl:call-template>

	  </xsl:otherwise>
	</xsl:choose>
      </xsl:when>
      <xsl:otherwise>
	<do-print/>
	<xsl:choose>
	  <xsl:when test="../attribute[@name='automatic' and
                                       (@old='revoke' or @new='revoke')]">
	    <!-- We are going to perform or undo a revocation of what would be
	         an automatic grant. -->
	    <xsl:if test="../attribute[@new='revoke']">
	      <xsl:call-template name="revoke_objectgrant"/>
	    </xsl:if>
	    <xsl:if test="../attribute[@old='revoke']">
	      <xsl:call-template name="build_objectgrant"/>
	    </xsl:if>
	  </xsl:when>

	  <xsl:when test="../attribute[@name='with_grant']">
	    <do-print/>
	    <xsl:choose>
	      <xsl:when test="../attribute[@name='with_grant' and @new='yes']">
		<xsl:call-template name="build_objectgrant"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:call-template name="revoke_objectgrant">
		  <xsl:with-param name="grant-only" select="'yes'"/>
		</xsl:call-template>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:when>
	  <xsl:when test="../attribute[@name='automatic']">
	    <!-- Nothing very important has changed here, just whether
	         the grant was automatic or not.  If it has changed from
		 being automatic to not, we can explicitly perform the
		 grant and all will be well.  If the change is the
		 other way, there is nothing we can do - not that there
		 will be any functional difference in the way the
		 database behaves.  The only reason for doing anything
		 at all here is to try to keep the catalog acl fields
		 of the databases in step.  -->

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
	<xsl:call-template name="revoke_rolegrant"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:if test="../@diff or not (@automatic='yes')">
	  <!-- Don't explicitly revoke privs that were automatically
	       generated. --> 

	  <xsl:call-template name="revoke_objectgrant"/>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>

