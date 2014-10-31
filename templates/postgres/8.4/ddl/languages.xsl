<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@action='build']/language">
    <print>
      <xsl:call-template name="feedback"/>
      <!-- This direct generation of set session auth is required because
	   there is no other way of defining the owner using the create
	   language statement. -->
      <xsl:value-of 
	  select="concat('&#x0A;set session authorization ', 
		             $apos, @owner, $apos, 
		         ';&#x0A;create language ', ../@qname, 
			 ';&#x0A;')"/>

      <xsl:apply-templates/>

      <xsl:text>reset session authorization;&#x0A;</xsl:text>
    </print>
  </xsl:template>

  <xsl:template match="language" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop language ', ../@qname, 
		       ';&#x0A;')"/>
    <xsl:if test="../@name = 'plpgsql'">
      <xsl:value-of 
	  select="concat('&#x0A;drop function plpgsql_validator(oid);',
		         '&#x0A;drop function plpgsql_call_handler();',
			 '&#x0A;')"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="language" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:for-each select="../attribute">
	<xsl:if test="@name='owner'">
	  <xsl:value-of 
	      select="concat('&#x0A;alter language ', ../@qname,
		                 ' owner to ', skit:dbquote($username),
		             ';&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="language" mode="diff">
    <xsl:for-each select="../attribute">
      <do-print/>
      <xsl:if test="@name='owner'">
	<xsl:value-of 
	    select="concat('&#x0A;alter language ', ../@qname,
		               ' owner to ', skit:dbquote(@new),
		           ';&#x0A;')"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>


