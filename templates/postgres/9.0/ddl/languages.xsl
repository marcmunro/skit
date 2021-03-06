<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template match="dbobject[@action='build']/language">
    <xsl:choose>
      <xsl:when test="(@name!='plpgsql') or @diff">
	<!-- If this is plpgsql, it should only be created if we
	     processing a diff.  -->
	<print>
	  <xsl:call-template name="feedback"/>
	  <xsl:text>&#x0A;create </xsl:text>
	  <xsl:if test="@trusted='yes'">
	    <xsl:text>trusted </xsl:text>
	  </xsl:if>
	  <xsl:if test="@trusted='yes'">
	    <xsl:text>trusted </xsl:text>
	  </xsl:if>
	  <xsl:value-of 
	      select="concat('language ', ../@qname, ';&#x0A;')"/>

	  <xsl:apply-templates/>
	</print>
      </xsl:when>
      <xsl:otherwise>
	<!-- This is plpgsql in the context of a database build.  There
	     may be a comment we need to deal with. -->
	<xsl:if test="comment">
	  <print>
	    <xsl:apply-templates/>
	  </print>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
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
		                 ' owner to ', skit:dbquote(@new),
		             ';&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>


