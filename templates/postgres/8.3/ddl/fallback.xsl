<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='fallback']">
    <xsl:if test="@action='fallback'">
      <print>
        <xsl:text>---- DBOBJECT FALLBACK</xsl:text> <!-- QQQ -->
	<xsl:choose>
	  <xsl:when test="@subtype='privilege'">
	    <xsl:value-of
		select="concat('&#x0A;&#x0A;alter user ', @role,
			' with superuser;&#x0A;')"/>
	  </xsl:when>
	  <xsl:when test="@subtype='grant'">
	    <xsl:value-of
		select="concat('&#x0A;&#x0A;grant ', @from,
			' to ', @to, ';&#x0A;')"/>
	  </xsl:when>
	</xsl:choose>
      </print>
    </xsl:if>

    <xsl:if test="@action='endfallback'">
      <print>
        <xsl:text>---- DBOBJECT END FALLBACK</xsl:text> <!-- QQQ -->
	<xsl:choose>
	  <xsl:when test="@subtype='privilege'">
	    <xsl:value-of
		select="concat('&#x0A;&#x0A;alter user ', fallback/@role,
			       ' with nosuperuser;&#x0A;')"/>
	  </xsl:when>
	  <xsl:when test="@subtype='grant'">
	    <xsl:value-of
		select="concat('&#x0A;&#x0A;revoke ', @from,
			' from ', @to, ';&#x0A;')"/>
	  </xsl:when>
	</xsl:choose>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

