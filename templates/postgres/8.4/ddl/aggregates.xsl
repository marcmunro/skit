<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="aggregate_header">
    <xsl:value-of 
	select="concat('aggregate ', skit:dbquote(@schema,@name),
		       '(')"/>
    <xsl:choose>
      <xsl:when test="basetype/@name">
	<xsl:value-of 
	    select="skit:dbquote(basetype/@schema,basetype/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>*</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>)</xsl:text>
  </xsl:template>

  <xsl:template match="dbobject/aggregate">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

	<xsl:text>create </xsl:text>
	<xsl:call-template name="aggregate_header"/>
	<xsl:value-of
	    select="concat(' (&#x0A;  sfunc = ', 
		           skit:dbquote(transfunc/@schema,transfunc/@name),
			   ',&#x0A;  stype = ',
			   skit:dbquote(transtype/@schema,transtype/@name))"/>
	<xsl:if test="@initcond">
	  <xsl:value-of
	      select="concat(',&#x0A;  initcond = ', @initcond)"/>
	</xsl:if>
	<xsl:if test="finalfunc">
          <xsl:value-of 
	     select="concat(',&#x0A;  finalfunc = ',
                            skit:dbquote(finalfunc/@schema,finalfunc/@name))"/>
	</xsl:if>
	<xsl:if test="sortop">
          <xsl:value-of 
	     select="concat(',&#x0A;  sortop = ',
		            skit:dbquote(sortop/@schema,sortop/@name))"/>
	</xsl:if>
	<xsl:text>);&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
	  
      	<xsl:text>drop </xsl:text>
	<xsl:call-template name="aggregate_header"/>
	<xsl:text>;&#x0A;</xsl:text>
	  
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <xsl:if test="../attribute[@name='owner']">
	<print>
	  <xsl:call-template name="feedback"/>

	  <xsl:text>alter </xsl:text>
	  <xsl:call-template name="aggregate_header"/>
	  <xsl:value-of 
	      select="concat(' owner to ', @owner, ';&#x0A;')"/>
	</print>
      </xsl:if>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

