<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/schema">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:choose>
	  <xsl:when test="../@name='public'">
	    <xsl:value-of 
		select="concat('alter schema ', ../@qname, ' owner to ')"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of 
		select="concat('create schema ', ../@qname, 
			       ' authorization ')"/>
	  </xsl:otherwise>
	</xsl:choose>
        <xsl:value-of select="concat(skit:dbquote(@owner), ';&#x0A;')"/>
	<xsl:apply-templates/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
        <xsl:value-of select="concat('drop schema ', ../@qname, ';&#x0A;')"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <xsl:if test="../attribute[@name='owner']">
	<print>
	  <xsl:call-template name="feedback"/>
	  <xsl:for-each select="../attribute">
	    <xsl:if test="@name='owner'">
	      <xsl:value-of 
		  select="concat('alter schema ', ../@qname, ' owner to ',
			         skit:dbquote(@new), ';&#x0A;')"/>
	    </xsl:if>
	  </xsl:for-each>
	</print>
      </xsl:if>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <xsl:if test="../element[@type='comment']">
	<print>
	  <xsl:call-template name="feedback"/>
	  <xsl:call-template name="commentdiff"/>
	</print>
      </xsl:if>
    </xsl:if>


  </xsl:template>
</xsl:stylesheet>

