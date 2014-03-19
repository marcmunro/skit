<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- This cannot be handled by the default dbobject template as the 
       dbobject type does not match the element name. -->
  <xsl:template match="dbobject[@type='dbincluster' and
		                @action='build']/database">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:value-of 
	  select="concat('&#x0A;create database ', ../@qname,
		         ' with&#x0A; owner ',
			 skit:dbquote(@owner), '&#x0A; encoding ',
			 $apos, @encoding, $apos, 
			 '&#x0A; tablespace ',
			 skit:dbquote(@tablespace), 
			 '&#x0A; connection limit = ',
			 @connections, ';&#x0A;')"/>
    </print>
  </xsl:template>

  <!-- This cannot be handled by the default dbobject template as the 
       dbobject type does not match the element name. -->
  <xsl:template match="dbobject[@type='dbincluster' and
		                @action='drop']/database">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:value-of 
	  select="concat('&#x0A;-- drop database ', ../@qname, ';&#x0A;')"/>
    </print>
  </xsl:template>

  <!-- This cannot be handled by the default dbobject template as 
       the feedback mechanism must be of the shell variety.  -->
  <xsl:template match="dbobject[@type='database' and
		                @action='build']/database">
    <print>
      <xsl:call-template name="shell-feedback"/>
      <xsl:value-of 
	  select="concat('&#x0A;psql -d ', ../@name,
		         ' &lt;&lt;', $apos, 'DBEOF', $apos, '&#x0A;',
			 'set standard_conforming_strings = off;&#x0A;',
			 'set escape_string_warning = off;&#x0A;')"/>
      <xsl:apply-templates/>
    </print>
  </xsl:template>

  <xsl:template match="database" mode="diffcomplete">
    <xsl:if test="../attribute">
      <xsl:text>&#x0A;</xsl:text>
    </xsl:if>
    <xsl:for-each select="../attribute[@name='connections']">
      <do-print/>
      <xsl:value-of 
	  select="concat('alter database ', ../@qname,
		         ' connection limit ', @new, ';&#x0A;')"/>
    </xsl:for-each>

    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('alter database ', ../@qname,
		         ' owner to ', @new, ';&#x0A;')"/>
    </xsl:for-each>

    <xsl:for-each select="../attribute[@name='tablespace']">
      <do-print/>
      <xsl:value-of 
	  select="concat('\echo WARNING: database default tablespace', 
		         ' changes from &quot;', @old,
			 '&quot; to &quot;', @new, '&quot;&#x0A;')"/>
    </xsl:for-each>

    <xsl:for-each select="../attribute[@name='encoding']">
      <do-print/>
      <xsl:value-of 
	  select="concat('\echo WARNING: database character encoding', 
			 ' changes from &quot;', @old,
			 '&quot; to &quot;', @new, '&quot;&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="dbobject[@type='database' and 
		                @action='drop']/database">
    <xsl:value-of 
	select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
    <xsl:text>&#x0A;DBEOF&#x0A;&#x0A;</xsl:text>
  </xsl:template>

</xsl:stylesheet>

