<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <xsl:include href="skitfile:ddl/feedback.xsl"/>

  <xsl:variable name="apos">&apos;</xsl:variable>

  <!-- Doing this explicitly seems to put the xmlns, etc definitions
       into this, the root node, rather than at many other nodes.  -->
  <xsl:template match="dump">	
    <dump>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </dump>
  </xsl:template>
      
  <!-- If this dbobject has a print object, we just copy it, otherwise
       we process the dbobject to add navigation code. -->
  <xsl:template match="dbobject">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:choose>
	<xsl:when test="print">
	  <print>
	    <xsl:apply-templates mode="print"/>
	    <!--
	    <xsl:if test="@type!='grant' and @type!='fallback'">
	      <xsl:text>&#x0A;</xsl:text>
	    </xsl:if> -->
	  </print>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates mode="add-nav" select="."/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
  </xsl:template>

  <!-- Don't do anything with text nodes matched by calls to
       apply-templates.  Adding this template seems to result in
       slightly better output formatting.  -->
  <xsl:template match="text()"/>

  <!-- Anything not matched explicitly will match this and be copied -->
  <xsl:template match="*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <!-- This template is called for the dbobjects that require navigation
       processing.  No dbobject should match this: all should be handled
       explicitly by type below.   IE, this is a catch-all for debugging
       purposes. -->
  <xsl:template match="*" mode="add-nav">
    <THIS-DBOBJECT-TYPE-NEEDS-EXPLICIT-HANDLING-IN-NAVIGATION-DOT-XSL>
      <xsl:copy>
	<xsl:copy-of select="@*"/>
      </xsl:copy>
    </THIS-DBOBJECT-TYPE-NEEDS-EXPLICIT-HANDLING-IN-NAVIGATION-DOT-XSL>
    <xsl:apply-templates mode="add-nav"/>
  </xsl:template>

  <xsl:template match="dbobject[@type='context']" mode="add-nav">
    <print>
      <xsl:if test="@action='arrive'">
	<xsl:value-of 
	    select="concat('set session authorization ', $apos, 
		            @name, $apos, ';&#x0A;')"/>
      </xsl:if>	

      <xsl:if test="@action='depart'">
	<print>
	  <xsl:text>reset session authorization;&#x0A;</xsl:text>
	</print>
      </xsl:if>	
    </print>
  </xsl:template>

  <xsl:template match="dbobject[@type='cluster']" mode="add-nav">
    <print>
      <xsl:if test="@action='arrive'">
	<xsl:call-template name="shell-feedback-dbobject"/>

	<xsl:text>psql -d postgres &lt;&lt;&apos;CLUSTEREOF&apos;&#x0A;</xsl:text>
	<xsl:text>set standard_conforming_strings = off;&#x0A;</xsl:text>
	<xsl:text>set escape_string_warning = off;&#x0A;</xsl:text>
      </xsl:if>	

      <xsl:if test="@action='depart'">
	<xsl:text>&#x0A;CLUSTEREOF&#x0A;</xsl:text>
      </xsl:if>	
    </print>
  </xsl:template>

  <xsl:template match="dbobject[@type='database']" mode="add-nav">
    <print>
      <xsl:if test="@action='arrive'">
	<xsl:call-template name="shell-feedback-dbobject"/>
        <xsl:value-of 
	    select="concat('psql -d ', @name,
		           ' &lt;&lt;', $apos, 'DBEOF', $apos, '&#x0A;',
			   'set standard_conforming_strings = off;&#x0A;',
			   'set escape_string_warning = off;&#x0A;')"/>
      </xsl:if>	

      <xsl:if test="@action='depart'">
	<print>
	  <xsl:value-of select="'&#x0A;DBEOF&#x0A;'"/>
	</print>
      </xsl:if>	
    </print>
  </xsl:template>

  <!-- dbobjects that have no required navigation -->
  <xsl:template match="dbobject[@type='schema' or @type='table' or
		                @type='view' or @type='role' or
				@type='tablespace' or @type='language' or
				@type='function']" 
		mode="add-nav"/>

</xsl:stylesheet>