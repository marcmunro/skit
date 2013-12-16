<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='fallback']">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:if test="@action='fallback'">
	<print>
	  <xsl:choose>
	    <xsl:when test="@subtype='privilege'">
	      <xsl:value-of
		  select="concat('alter user ', @role,
			  ' with superuser;')"/>
	    </xsl:when>
	    <xsl:when test="@subtype='grant'">
	      <xsl:value-of
		  select="concat('grant ', @from,
			  ' to ', @to, ';')"/>
	    </xsl:when>
	  </xsl:choose>
	  <xsl:text>    -- temporary (fallback) privilege&#x0A;</xsl:text>
	</print>
      </xsl:if>

      <xsl:if test="@action='endfallback'">
	<print>
	  <xsl:choose>
	    <xsl:when test="@subtype='privilege'">
	      <xsl:value-of
		  select="concat('alter user ', fallback/@role,
			  ' with nosuperuser;')"/>
	    </xsl:when>
	    <xsl:when test="@subtype='grant'">
	      <xsl:value-of
		  select="concat('revoke ', @from,
			  ' from ', @to, ';')"/>
	    </xsl:when>
	  </xsl:choose>
	  <xsl:text>    -- end temporary (fallback) privilege&#x0A;</xsl:text>
	</print>
      </xsl:if>
    </xsl:copy>
  </xsl:template>
</xsl:stylesheet>

