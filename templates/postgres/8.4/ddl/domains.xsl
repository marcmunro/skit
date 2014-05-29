<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="domain" mode="build">
    <xsl:value-of 
	select="concat('&#x0A;create domain ',
		       skit:dbquote(@schema,@name), '&#x0A;  as ',
		       skit:dbquote(@basetype_schema,@basetype))"/>
    <xsl:for-each select="constraint">
      <xsl:value-of select="concat('&#x0A;  ', source/text())"/>
    </xsl:for-each>
    <xsl:if test="@nullable='no'">
      <xsl:text> not null</xsl:text>
    </xsl:if>
    <xsl:if test="@default">
      <xsl:value-of select="concat('&#x0A;  default ', @default)"/>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="domain" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop domain ', 
		       skit:dbquote(@schema,@name),';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="domain" mode="diffprep">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('&#x0A;alter domain ', ../@qname,
		         ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="domain" mode="diff">
    <xsl:if test="(../attribute[@name!='owner']) or
		  (../element[@type='constraint'])">
      <do-print/>
      <xsl:text>&#x0A;</xsl:text>
    </xsl:if>
    <xsl:for-each select="../attribute[@name!='owner']">
      <xsl:if test="@name='nullable'">
	<xsl:choose>
	  <xsl:when test="@new='yes'">
	    <xsl:value-of
		select="concat('alter domain ', ../@qname,
			       ' drop not null;&#x0A;')"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of
		select="concat('alter domain ', ../@qname,
			       ' set not null;&#x0A;')"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:if>
      <xsl:if test="@name='default'">
	<xsl:choose>
	  <xsl:when test="@status='gone'">
	    <xsl:value-of
		select="concat('alter domain ', ../@qname,
			       ' drop default;&#x0A;')"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:value-of
		select="concat('alter domain ', ../@qname,
			       ' set default ', @new,
			       ';&#x0A;')"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:if>
    </xsl:for-each>

    <xsl:for-each select="../element[@type='constraint']">
      <xsl:value-of
	  select="concat('alter domain ', ../@qname,
			 ' drop constraint ', 
			 skit:dbquote(constraint/@name),
			 ';&#x0A;')"/>
      <xsl:if test="@status!='gone'">
	<xsl:value-of
	    select="concat('alter domain ', ../@qname,
			   ' add constraint ', 
			   skit:dbquote(constraint/@name),
			   '&#x0A;  ', 
			   constraint/source/text(),
			   ';&#x0A;')"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>
</xsl:stylesheet>
