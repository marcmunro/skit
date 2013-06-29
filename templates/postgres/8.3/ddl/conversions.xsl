<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/conversion">
    <xsl:if test="../@action='build'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;&#x0A;')"/> 
	<xsl:call-template name="set_owner"/>

	<xsl:text>create </xsl:text>
	<xsl:if test="@is_default='t'">
	  <xsl:text>default </xsl:text>
	</xsl:if>
        <xsl:value-of 
	    select="concat('conversion ', ../@qname,
	                   '&#x0A;  for ', $apos, @source, $apos,
			   ' to ', $apos, @destination, $apos,
			   '&#x0A;  from ',
			   skit:dbquote(@function_schema,@function_name),
			   ';&#x0A;')"/>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;&#x0A;')"/> 
	<xsl:call-template name="set_owner"/>
        <xsl:value-of 
	    select="concat('&#x0A;drop conversion ', ../@qname, ';&#x0A;')"/>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;&#x0A;')"/> 
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:value-of 
		select="concat('&#x0A;alter conversion ', ../@qname,
			       ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
	  </xsl:if>
	  </xsl:for-each>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

