<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/conversion">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

	<xsl:text>&#x0A;create </xsl:text>
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
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
        <xsl:value-of 
	    select="concat('&#x0A;drop conversion ', ../@qname, ';&#x0A;')"/>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:value-of 
		select="concat('&#x0A;alter conversion ', ../@qname,
			       ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
	  </xsl:if>
	</xsl:for-each>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

