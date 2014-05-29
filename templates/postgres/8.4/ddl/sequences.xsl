<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="sequence" mode="build">
    <xsl:value-of 
	select="concat('&#x0A;create sequence ', ../@qname,
		       '&#x0A;  start with ', @start_with,
		       '&#x0A;  increment by ', @increment_by,
		       '&#x0A;  minvalue ', @min_value,
		       '&#x0A;  maxvalue ', @max_value,
		       '&#x0A;  cache ', @cache)"/>
    <xsl:if test="@cycled='t'">
      <xsl:text>&#x0A;  cycle</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="sequence" mode="drop">
    <xsl:value-of select="concat('&#x0A;drop sequence ', 
			         ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="sequence" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:for-each select="../attribute[@name='owner']">
	<xsl:value-of 
	    select="concat('&#x0A;alter sequence ', ../@qname,
			   ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="sequence" mode="diff">
    <xsl:if test="../attribute[@name!='owner'] or ../element">
      <do-print/>
      <xsl:text>&#x0A;</xsl:text>
      <xsl:if test="../attribute[(@name!='owner') and (@name!='start_with')]">
	<xsl:value-of select="concat('alter sequence ', ../@qname)"/>
	<xsl:for-each select="../attribute">
	  <xsl:choose>
	    <xsl:when test="@name='increment_by'">
	      <xsl:value-of select="concat('&#x0A;  increment by ', @new)"/>
	    </xsl:when>
	    <xsl:when test="@name='max_value'">
	      <xsl:value-of select="concat('&#x0A;  maxvalue ', @new)"/>
	    </xsl:when>
	    <xsl:when test="@name='min_value'">
	      <xsl:value-of select="concat('&#x0A;  minvalue ', @new)"/>
	    </xsl:when>
	    <xsl:when test="@name='cache'">
	      <xsl:value-of select="concat('&#x0A;  cache ', @new)"/>
	    </xsl:when>
	    <xsl:when test="@name='is_cycled'">
	      <xsl:choose>
		<xsl:when test="@new='t'">
		  <xsl:text>&#x0A;  cycle</xsl:text>
		</xsl:when>
		<xsl:otherwise>
		  <xsl:text>&#x0A;  nocycle</xsl:text>
		</xsl:otherwise>
	      </xsl:choose>
	    </xsl:when>
	      </xsl:choose>
	</xsl:for-each>
	<xsl:text>;&#x0A;</xsl:text>
      </xsl:if>
      <xsl:for-each select="../attribute[@name='start_with']">
	<xsl:value-of 
	    select="concat('select pg_catalog.setval(', $apos, 
		           ../sequence/@schema, '.', ../@name, $apos,
			   ', ', @new, ', true);&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>


