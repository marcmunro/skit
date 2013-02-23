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

        <xsl:text>grant </xsl:text>
        <xsl:value-of select="skit:dbquote(@priv)"/>
        <xsl:text> to </xsl:text>
        <xsl:value-of select="skit:dbquote(@to)"/>
	<xsl:if test="@with_admin = 'yes'">
          <xsl:text> with admin option</xsl:text>
	</xsl:if>
        <xsl:text>;&#x0A;</xsl:text>

	<xsl:call-template name="reset_owner_from"/>
      </print>
    </xsl:if>
  
    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

        <xsl:text>revoke </xsl:text>
        <xsl:value-of select="skit:dbquote(@priv)"/>
        <xsl:text> from </xsl:text>
        <xsl:value-of select="skit:dbquote(@to)"/>
        <xsl:text>;&#x0A;</xsl:text>

	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>
    
  </xsl:template>

  <!-- Object-level grants -->
  <xsl:template match="dbobject[@subtype!='role']/grant">
    <xsl:if test="../@action='build'">
      <xsl:if test="not(@default='yes')">
      	<print>
	  <xsl:call-template name="set_owner_from"/>

      	  <xsl:text>grant </xsl:text>
      	  <xsl:value-of select="@priv"/>
      	  <xsl:text> on </xsl:text>
	  <xsl:choose>
	    <xsl:when test="../@subtype = 'view'">
      	      <xsl:text>table</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
      	      <xsl:value-of select="../@subtype"/>
	    </xsl:otherwise>
	  </xsl:choose>
      	  <xsl:text> </xsl:text>
      	  <xsl:value-of select="../@on"/>
      	  <xsl:text> to </xsl:text>
      	  <xsl:value-of select="skit:dbquote(@to)"/>
      	  <xsl:if test="@with_grant = 'yes'">
      	    <xsl:text> with grant option</xsl:text>
      	  </xsl:if>
      	  <xsl:text>;&#x0A;</xsl:text>

	  <xsl:call-template name="reset_owner_from"/>
      	</print>
      </xsl:if>
    </xsl:if>
    
  
    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner_from"/>

	<xsl:text>revoke </xsl:text>
	<xsl:value-of select="@priv"/>
	<xsl:text> on </xsl:text>
	<xsl:choose>
	  <xsl:when test="../@subtype = 'view'">
      	    <xsl:text>table</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
      	    <xsl:value-of select="../@subtype"/>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text> </xsl:text>
	<xsl:value-of select="../@on"/>
	<xsl:text> from </xsl:text>
	<xsl:value-of select="skit:dbquote(@to)"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:call-template name="reset_owner_from"/>
      </print>
    </xsl:if>
    
  </xsl:template>


</xsl:stylesheet>

