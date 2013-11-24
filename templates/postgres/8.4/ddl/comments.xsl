<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- This handles comments which are defined as dbobjects.  For now
       this just means comments for automatically generated operator
       families.  These can't be created in the normal way as operator
       classes must be dependent on their operator families so that
       drops are correctly ordered, and the operator family comment
       cannot be generated as part of the operator family generation as
       operator families must be created before the operator class they
       depend on.  -->
  <xsl:template match="dbobject/comment">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
      	  <xsl:text>&#x0A;-- Comment for automatically generated</xsl:text>
	  <xsl:text> operator family</xsl:text>
      	  <xsl:text>&#x0A;comment on operator family </xsl:text>
	  <xsl:value-of select="../@qname"/>
      	  <xsl:text> using </xsl:text>
	  <xsl:value-of select="../@method"/>
      	  <xsl:text> is&#x0A;</xsl:text>
	  <xsl:value-of select="text()"/>
          <xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


