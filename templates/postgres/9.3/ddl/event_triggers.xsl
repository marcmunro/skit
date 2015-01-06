<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">


  <xsl:template name="alter_event_trigger">
    <xsl:value-of 
	select="concat('alter event trigger ', ../@qname)"/>
    <xsl:call-template name="alter_enablement"/>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="event_trigger" mode="build">
    <xsl:value-of 
	select="concat('&#x0A;create event trigger ', ../@qname,
		       '&#x0A;  on ', @event_name)"/>
    <xsl:if test="@event_tags">
      <xsl:value-of 
	  select="concat('&#x0A;  when tag in (', @event_tags, ')')"/>
    </xsl:if>
    <xsl:value-of 
	select="concat('&#x0A;  execute procedure ', @function)"/>
    <xsl:text>;&#x0A;</xsl:text>

    <xsl:if test="@enabled!='O'">
      <xsl:call-template name="alter_event_trigger"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="event_trigger" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop event trigger ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="event_trigger" mode="diffprep">
    <xsl:if test="../attribute[@name='enabled']">
      <do-print/>
      <xsl:call-template name="alter_event_trigger"/>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>


