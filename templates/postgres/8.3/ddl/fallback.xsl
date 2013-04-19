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
	<xsl:value-of
	    select="concat('&#x0A;&#x0A;alter user ', @to,
		           ' with superuser;&#x0A;')"/>
      </print>
    </xsl:if>

    <xsl:if test="@action='endfallback'">
      <print>
        <xsl:text>---- DBOBJECT END FALLBACK</xsl:text> <!-- QQQ -->
	<xsl:value-of
	    select="concat('&#x0A;&#x0A;alter user ', @to,
		           ' with nosuperuser;&#x0A;')"/>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

