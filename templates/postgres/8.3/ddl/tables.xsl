<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="column">
    <xsl:if test="position() != 1">
      <xsl:text>,</xsl:text>
    </xsl:if>
    <xsl:text>&#x0A;  </xsl:text>
    <xsl:value-of 
       select="concat(skit:dbquote(@name),
	              substring('                              ',
		      string-length(skit:dbquote(@name))))"/>
    <xsl:text> </xsl:text>
    <xsl:value-of select="skit:dbquote(@type_schema, @type)"/>
    <xsl:if test="@size">
      <xsl:text>(</xsl:text>
      <xsl:value-of select="@size"/>
      <xsl:if test="@precision">
        <xsl:text>,</xsl:text>
	<xsl:value-of select="@precision"/>
      </xsl:if>
      <xsl:text>)</xsl:text>
    </xsl:if>
    <xsl:value-of select="@dimensions"/>
    <xsl:if test="@nullable='no'">
      <xsl:text> not null</xsl:text>
    </xsl:if>
    <xsl:if test="@default">
      <xsl:text>&#x0A;                                    default </xsl:text>
      <xsl:value-of select="@default"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="dbobject/table">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

        <xsl:text>create table </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text> (</xsl:text>
	<xsl:for-each select="column[@is_local='t']">
	  <xsl:sort select="@colnum"/>
	  <xsl:call-template name="column"/>
	</xsl:for-each>
	<xsl:text>&#x0A;)</xsl:text>
	<xsl:if test ="inherits">
	  <xsl:text> inherits (</xsl:text>
	  <xsl:for-each select="inherits">
	    <xsl:sort select="@inherit_order"/>
	    <xsl:if test="position() != 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="skit:dbquote(@schema,@name)"/>
	  </xsl:for-each>
	  <xsl:text>)</xsl:text>
	</xsl:if>
	<xsl:if test ="@tablespace">
	  <xsl:text>&#x0A;tablespace </xsl:text>
	  <xsl:value-of select="@tablespace"/>
	</xsl:if>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:if test="column[@stats_target]">
	  <xsl:text>&#x0A;alter table </xsl:text>
          <xsl:value-of select="../@qname"/>
	  <xsl:for-each select="column[@stats_target]">
	    <xsl:if test="position() != '1'">
	      <xsl:text>,</xsl:text>
	    </xsl:if>
	    <xsl:text>&#x0A;  alter column </xsl:text>
            <xsl:value-of select="skit:dbquote(@name)"/>
	    <xsl:text> set statistics </xsl:text>
            <xsl:value-of select="@stats_target"/>
	  </xsl:for-each>
	  <xsl:text>;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:if test="column[@storage_policy]">
	  <xsl:text>&#x0A;alter table </xsl:text>
          <xsl:value-of select="../@qname"/>
	  <xsl:for-each select="column[@storage_policy]">
	    <xsl:if test="position() != '1'">
	      <xsl:text>,</xsl:text>
	    </xsl:if>
	    <xsl:text>&#x0A;  alter column </xsl:text>
            <xsl:value-of select="skit:dbquote(@name)"/>
	    <xsl:text> set storage </xsl:text>
	    <xsl:choose>
	      <xsl:when test="@storage_policy = 'p'">
		<xsl:text>main</xsl:text>
	      </xsl:when>
	      <xsl:when test="@storage_policy = 'e'">
		<xsl:text>external</xsl:text>
	      </xsl:when>
	      <xsl:when test="@storage_policy = 'm'">
		<xsl:text>main</xsl:text>
	      </xsl:when>
	      <xsl:when test="@storage_policy = 'x'">
		<xsl:text>extended</xsl:text>
	      </xsl:when>
	    </xsl:choose>
	  </xsl:for-each>
	  <xsl:text>;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:for-each select="column[@is_local='t']/comment">
	  <xsl:text>&#x0A;comment on column </xsl:text>
          <xsl:value-of select="concat(../../../@qname, '.',
	                        skit:dbquote(../@name))"/>
	  <xsl:text> is&#x0A;</xsl:text>
	  <xsl:value-of select="text()"/>
	  <xsl:text>;&#x0A;</xsl:text>
	</xsl:for-each>

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="set_owner"/>
        <xsl:text>&#x0A;drop table </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


