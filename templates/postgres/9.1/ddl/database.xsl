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
		         ' with&#x0A;  owner ',
			 skit:dbquote(@owner), '&#x0A;  encoding ',
			 $apos, @encoding, $apos)"/>
      <xsl:if test="@lc_collate">
	<xsl:value-of 
	    select="concat('&#x0A;  lc_collate ', $apos, @lc_collate, $apos)"/>
      </xsl:if>
      <xsl:if test="@lc_type">
	<xsl:value-of 
	    select="concat('&#x0A; lc_ctype ', $apos, @lc_ctype, $apos)"/>
      </xsl:if>
      <xsl:value-of  
	  select="concat('&#x0A;  tablespace ',
			 skit:dbquote(@tablespace), 
			 '&#x0A;  connection limit = ',
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
			 'set escape_string_warning = off;&#x0A;')"/>
      <xsl:apply-templates/>
    </print>
  </xsl:template>

  <xsl:template match="dbobject[@type='dbincluster' and
		                @action='diffprep']/database">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>
      <xsl:for-each select="../attribute[@name='tablespace']">
	<xsl:if test="@old != 'pg_default'">
	  <do-print/>
	  <xsl:value-of 
	      select="concat('alter database ', ../@qname,
		      ' set tablespace pg_default;&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </print>
  </xsl:template>

  <xsl:template match="dbobject[@type='dbincluster' and
		                @action='diff']/database">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>
      <xsl:for-each select="../attribute[@name='tablespace']">
	<xsl:if test="@new != 'pg_default'">
	  <do-print/>
	  <xsl:value-of 
	      select="concat('alter database ', ../@qname,
		             ' set tablespace ', skit:dbquote(@new),
			     ';&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </print>
  </xsl:template>


  <xsl:template match="database" mode="diffprep">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('alter database ', ../@qname,
		         ' owner to ', skit:dbquote($username), 
			 ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="database" mode="diff">
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
		         ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
    </xsl:for-each>

    <xsl:for-each select="../attribute[@name='encoding']">
      <do-print/>
      <xsl:value-of 
	  select="concat('\echo WARNING: database character encoding', 
			 ' changes from &quot;', @old,
			 '&quot; to &quot;', @new, '&quot;&#x0A;&#x0A;')"/>
    </xsl:for-each>
    <xsl:for-each select="../attribute[@name='lc_collate']">
      <do-print/>
      <xsl:value-of 
	  select="concat('\echo WARNING: database collation order', 
			 ' changes from &quot;', @old,
			 '&quot; to &quot;', @new, '&quot;&#x0A;&#x0A;')"/>
    </xsl:for-each>
    <xsl:for-each select="../attribute[@name='lc_ctype']">
      <do-print/>
      <xsl:value-of 
	  select="concat('\echo WARNING: database character classification', 
			 ' changes from &quot;', @old,
			 '&quot; to &quot;', @new, '&quot;&#x0A;&#x0A;')"/>
    </xsl:for-each>
    <xsl:for-each select="../element[@type='setting']/
			  attribute[@name='setting']">
      <do-print/>
      <xsl:value-of select="concat('\echo WARNING: setting ', ../setting/@name,
			           ' changes from ')"/>
      <xsl:if test="(@status='diff') or (@status='gone')">
	<xsl:value-of select="concat('&quot;', @old, '&quot; to ')"/>
      </xsl:if>
      <xsl:if test="@status='new'">
	<xsl:text>default to </xsl:text>
      </xsl:if>
      <xsl:if test="(@status='diff') or (@status='new')">
	<xsl:value-of select="concat('&quot;', @new, '&quot;')"/>
      </xsl:if>
      <xsl:if test="@status='gone'">
	<xsl:text>default</xsl:text>
      </xsl:if>
      <xsl:if test="../setting/@sourcefile">
	<xsl:value-of select="concat('...&#x0A;\echo ...at ', 
			             ../setting/@sourcefile,
	                             ':', ../setting/@sourceline)"/>
      </xsl:if>
      <xsl:text>&#x0A;&#x0A;</xsl:text>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="dbobject[@type='database' and 
		                @action='drop']/database">
    <xsl:value-of 
	select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
    <xsl:text>&#x0A;DBEOF&#x0A;&#x0A;</xsl:text>
  </xsl:template>

</xsl:stylesheet>

