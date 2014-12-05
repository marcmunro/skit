<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="fk_event">
    <xsl:param name="event"/>
    <xsl:param name="action_code"/>
    <xsl:if test="$action_code and ($action_code != 'a')">
      <xsl:value-of select="concat('&#x0A;  on ', $event)"/>
      <xsl:choose>
	<xsl:when test="$action_code = 'c'">
	  <xsl:text> cascade</xsl:text>
	</xsl:when>
	<xsl:when test="$action_code = 'd'">
	  <xsl:text> set default</xsl:text>
	</xsl:when>
	<xsl:when test="$action_code = 'n'">
	  <xsl:text> set null</xsl:text>
	</xsl:when>
	<xsl:when test="$action_code = 'r'">
	  <xsl:text> restrict</xsl:text>
	</xsl:when>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <xsl:template match="constraint" mode="build">
    <xsl:if test="@is_local='t'">
    <xsl:value-of 
	select="concat('&#x0A;alter table ', ../@table_qname,
		       '&#x0A;  add constraint ', skit:dbquote(@name))"/>

    <xsl:choose>
      <xsl:when test="@type='foreign key'">
	<xsl:text>&#x0A;  foreign key(</xsl:text>
	<xsl:for-each select="column">
	  <xsl:if test="position() != '1'">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="skit:dbquote(@name)"/>
	</xsl:for-each>
	<xsl:value-of 
	    select="concat(')&#x0A;  references ', 
			   skit:dbquote(reftable/@refschema, 
			                reftable/@reftable), '(')"/>
	<xsl:for-each select="reftable/column">
	  <xsl:if test="position() != '1'">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="skit:dbquote(@name)"/>
	</xsl:for-each>
	<xsl:text>)</xsl:text>
	<xsl:choose>
	  <xsl:when test="@confmatchtype = 'f'">
	    <xsl:text> match full</xsl:text>
	  </xsl:when>
	  <xsl:when test="@confmatchtype = 'p'">
	    <xsl:text> match partial</xsl:text>
	  </xsl:when>
	  <xsl:when test="@confmatchtype = 'f'">
	    <xsl:text> match simple</xsl:text>
	  </xsl:when>
	</xsl:choose>
	<xsl:call-template name="fk_event">
	  <xsl:with-param name="event" select="'update'"/>
	  <xsl:with-param name="action_code" select="@confupdtype"/>
	</xsl:call-template>
	<xsl:call-template name="fk_event">
	  <xsl:with-param name="event" select="'delete'"/>
	  <xsl:with-param name="action_code" select="@confdeltype"/>
	</xsl:call-template>
	<xsl:if test="@deferrable='t'">
	  <xsl:text>&#x0A;  deferrable</xsl:text>
	</xsl:if>
	<xsl:if test="@deferred='t'">
	  <xsl:text>&#x0A;  initially deferred</xsl:text>
	</xsl:if>
      </xsl:when>
      <xsl:when test="(@type = 'unique') or (@type = 'primary key')">
	<xsl:choose>
	  <xsl:when test="@type = 'unique'">
	    <xsl:text>&#x0A;  unique(</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>&#x0A;  primary key(</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>	
	<xsl:for-each select="column">
	  <xsl:if test="position() != '1'">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="skit:dbquote(@name)"/>
	</xsl:for-each>
	<xsl:text>)</xsl:text>
	<xsl:if test="@options">
	  <xsl:value-of select="concat('&#x0A;  with (', @options, ')')"/>
	</xsl:if>
	<xsl:if test="@tablespace">
	  <xsl:value-of 
	      select="concat('&#x0A;  using index tablespace ',
		             skit:dbquote(@tablespace))"/>
	</xsl:if>
      </xsl:when>
      <xsl:when test="@type='check'">
	<xsl:value-of select="concat('&#x0A;  ', @source)"/>
      </xsl:when>
      <xsl:when test="@type='exclusion'">
	<xsl:value-of 
	    select="concat('&#x0A;  exclude using ', @access_method, '(')"/>
	<xsl:for-each select="column">
	  <xsl:value-of select="concat(@name, ' with ', @operator)"/>
	  <xsl:if test="position()!=last()">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	</xsl:for-each>
	<xsl:text>)</xsl:text>
	<xsl:if test="@indexdef">
	  <xsl:value-of select="concat('&#x0A;  with ', @indexdef)"/>
	</xsl:if>
	<xsl:if test="@tablespace">
	  <xsl:value-of select="concat('&#x0A;  using index tablespace ', 
				       @tablespace)"/>
	</xsl:if>
	<xsl:if test="@predicate">
	  <xsl:value-of select="concat('&#x0A;  ', @predicate)"/>
	</xsl:if>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text> OTHER TYPE OF CONSTRAINT</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>;&#x0A;</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="constraint" mode="drop">
    <xsl:if test="@is_local='t'">
    <xsl:value-of 
	select="concat('&#x0A;alter table ', ../@table_qname,
		       '&#x0A;  drop constraint ', skit:dbquote(@name),
		       ';&#x0A;')"/>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>


