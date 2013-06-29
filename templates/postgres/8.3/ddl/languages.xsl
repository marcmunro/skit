<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/language">

    <xsl:if test="../@action='build'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 

	<!-- This direct generation of set session auth is required because
	     there is no other way of defining the owner using the create
	     language statement. -->
        <xsl:value-of 
	    select="concat('&#x0A;set session authorization ',
		           $apos, @owner, $apos, 
		           ';&#x0A;create language ', 
		           ../@qname, ';&#x0A;')"/>

	<xsl:apply-templates/>

	<xsl:text>reset session authorization;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
        <xsl:value-of 
	    select="concat('&#x0A;drop language ', ../@qname, ';&#x0A;')"/>
	<xsl:if test="../@name = 'plpgsql'">
	  <xsl:value-of 
	      select="concat('&#x0A;drop function plpgsql_validator(oid);',
		      '&#x0A;&#x0A;',
		      'drop function plpgsql_call_handler();&#x0A;')"/>
	</xsl:if>
      </print>
    </xsl:if>
    <xsl:apply-templates/>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
	<xsl:text>&#x0A;</xsl:text>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:value-of 
		select="concat('alter language ', ../@qname,
			       ' owner to ', skit:dbquote(@new),
			       ';&#x0A;')"/>
	  </xsl:if>
	  </xsl:for-each>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


