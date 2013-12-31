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
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
	<xsl:value-of 
	    select="concat('&#x0A;comment on operator family ',
		           ../@qname, ' using ', ../@method,
			   ' is&#x0A;', text(), ';&#x0A;&#x0A;')">
	</xsl:value-of>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


