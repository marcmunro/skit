<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='context']">
     <xsl:if test="@action='arrive'">
      <print>
	<xsl:value-of 
	    select="concat('&#x0A;set session authorization ', $apos, 
		            @name, $apos, ';&#x0A;')"/>
      </print>
    </xsl:if>	

    <xsl:if test="@action='depart'">
      <print>
	<xsl:text>reset session authorization;&#x0A;</xsl:text>
	</print>
    </xsl:if>	

  </xsl:template>

</xsl:stylesheet>

