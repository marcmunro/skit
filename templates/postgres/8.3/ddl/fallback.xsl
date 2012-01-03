<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='fallback']">
    <xsl:if test="@action='build'">
      <print>
        <xsl:text>&#x0A;&#x0A;GRANTGRANTGRANTGRANTGRANTGRANT&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="@action='drop'">
      <print>
        <xsl:text>&#x0A;&#x0A;UNGRANTUNGRANTUNGRANTUNGRANTUNGRANTUNGRANT&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

