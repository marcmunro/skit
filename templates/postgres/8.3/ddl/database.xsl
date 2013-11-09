<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='dbincluster']/database">
    <xsl:if test="../@action='build'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
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
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
	<xsl:value-of 
	    select="concat('&#x0A;-- drop database ', ../@qname, ';&#x0A;')"/>
      </print>
    </xsl:if>
  </xsl:template>

  <xsl:template match="dbobject[@type='database']/database">

    <xsl:if test="../@action='build'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('#### DBOBJECT ', ../@fqn, '&#x0A;')"/> 
        <xsl:value-of 
	    select="concat('&#x0A;psql -d ', ../@name,
		           ' &lt;&lt;', $apos, 'DBEOF', $apos, '&#x0A;',
			   'set standard_conforming_strings = off;&#x0A;',
			   'set escape_string_warning = off;&#x0A;')"/>
	<xsl:apply-templates/>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('#### DBOBJECT ', ../@fqn, '&#x0A;')"/> 

        <xsl:value-of 
	    select="concat('&#x0A;psql -d ', ../@name,
		           ' &lt;&lt;', $apos, 'DBEOF', $apos, '&#x0A;',
			   'set standard_conforming_strings = off;&#x0A;',
			   'set escape_string_warning = off;&#x0A;')"/>

	<xsl:for-each select="../attribute[@name='connections']">
	  <xsl:value-of 
	      select="concat('alter database ', ../@qname,
		             ' connection limit ', @new, ';&#x0A;')"/>
	</xsl:for-each>

	<xsl:for-each select="../attribute[@name='owner']">
	  <xsl:value-of 
	      select="concat('alter database ', ../@qname,
		             ' owner to ', @new, ';&#x0A;')"/>
	</xsl:for-each>

	<xsl:for-each select="../attribute[@name='tablespace']">
	  <xsl:value-of 
	      select="concat('\echo WARNING: database default tablespace', 
		             ' changes from &quot;', @old,
			     '&quot; to &quot;', @new, '&quot;&#x0A;')"/>
	</xsl:for-each>

	<xsl:for-each select="../attribute[@name='encoding']">
	    <xsl:value-of 
		select="concat('\echo WARNING: database character encoding', 
			       ' changes from &quot;', @old,
			       '&quot; to &quot;', @new, '&quot;&#x0A;')"/>
	</xsl:for-each>

	<xsl:for-each select="../element[@type='comment']">
	  <xsl:value-of 
	      select="concat('&#x0A;comment on database ', ../@qname,
		             ' is ')"/>
	  <xsl:choose>
	    <xsl:when test="@status='gone'">
	      <xsl:text>null;&#x0A;</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of 
		  select="concat('&#x0A;', comment, ';&#x0A;')"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:for-each>
	<xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>	

    <xsl:if test="../@action='drop'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 
        <xsl:text>DBEOF&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>	
  </xsl:template>
</xsl:stylesheet>

