<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="column-typedef">
    <xsl:param name="with-null" select="'yes'"/>
    <xsl:value-of select="skit:dbquote(@type_schema, @type)"/>
    <xsl:if test="@size">
      <xsl:value-of select="concat('(', @size)"/>
      <xsl:if test="@precision">
	<xsl:value-of select="concat(',', @precision)"/>
      </xsl:if>
      <xsl:value-of select="')'"/>
    </xsl:if>
    <xsl:value-of select="@dimensions"/>
    <xsl:if test="$with-null='yes' and @nullable='no'">
      <xsl:value-of select="' not null'"/>
    </xsl:if>
  </xsl:template>

  <xsl:template name="column">
    <xsl:param name="position" select="position()"/>
    <xsl:param name="spaces" select="'                              '"/>
    <xsl:param name="prefix" select="'&#x0A;  '"/>
    <xsl:if test="$position != 1">
      <xsl:value-of select="','"/>
    </xsl:if>
    <xsl:value-of select="$prefix"/>
    <xsl:value-of 
       select="concat(skit:dbquote(@name),
	              substring($spaces,
		      string-length(skit:dbquote(@name))), '  ')"/>
    <xsl:call-template name="column-typedef"/>
    <xsl:if test="@default">
      <xsl:value-of 
	  select="concat('&#x0A;                                    default ', 
		         @default)"/>
    </xsl:if>
  </xsl:template>

  <xsl:template match="table" mode="build">
    <xsl:value-of select="concat('create table ', ../@qname, ' (')"/>
    <xsl:for-each select="column[@is_local='t']">
      <xsl:sort select="@colnum"/>
      <xsl:call-template name="column"/>
    </xsl:for-each>
    <xsl:value-of select="'&#x0A;)'"/>
    <xsl:if test ="inherits">
      <xsl:value-of select="' inherits ('"/>
      <xsl:for-each select="inherits">
	<xsl:sort select="@inherit_order"/>
	<xsl:if test="position() != 1">
	  <xsl:value-of select="', '"/>
	</xsl:if>
	<xsl:value-of select="skit:dbquote(@schema,@name)"/>
      </xsl:for-each>
      <xsl:value-of select="')'"/>
    </xsl:if>
    <xsl:if test ="@tablespace">
	  <xsl:value-of select="concat('&#x0A;tablespace ', @tablespace)"/>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>

    <xsl:if test="column[@stats_target]">
      <xsl:value-of select="concat('&#x0A;alter table ', ../@qname)"/>
      <xsl:for-each select="column[@stats_target]">
	<xsl:if test="position() != '1'">
	  <xsl:value-of select="', '"/>
	</xsl:if>
	<xsl:value-of 
	    select="concat('&#x0A;  alter column ', skit:dbquote(@name),
		           ' set statistics ', @stats_target)"/>
      </xsl:for-each>
      <xsl:value-of select="';&#x0A;'"/>
    </xsl:if>
    
    <xsl:if test="column[@storage_policy]">
      <xsl:value-of select="concat('&#x0A;alter table ', ../@qname)"/>
      <xsl:for-each select="column[@storage_policy]">
	<xsl:if test="position() != '1'">
	  <xsl:value-of select="', '"/>
	</xsl:if>
	<xsl:value-of 
	    select="concat('&#x0A;  alter column ', skit:dbquote(@name),
		           ' set storage ')"/>
	<xsl:choose>
	  <xsl:when test="@storage_policy = 'p'">
	    <xsl:value-of select="'main'"/>
	  </xsl:when>
	  <xsl:when test="@storage_policy = 'e'">
	    <xsl:value-of select="'external'"/>
	  </xsl:when>
	  <xsl:when test="@storage_policy = 'm'">
	    <xsl:value-of select="'main'"/>
	  </xsl:when>
	  <xsl:when test="@storage_policy = 'x'">
	    <xsl:value-of select="'extended'"/>
	  </xsl:when>
	</xsl:choose>
      </xsl:for-each>
      <xsl:value-of select="';&#x0A;'"/>
    </xsl:if>
    
    <xsl:for-each select="column[@is_local='t']/comment">
      <xsl:value-of 
	  select="concat('&#x0A;comment on column ', ../../../@qname, '.',
	                 skit:dbquote(../@name), ' is&#x0A;', text(),
			 ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="table" mode="drop">
    <xsl:value-of 
	    select="concat('&#x0A;drop table ', ../@qname, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="table" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:for-each select="../attribute[@name='owner']">
	<xsl:value-of 
	    select="concat('&#x0A;alter table ', ../@qname,
			   ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template 
      match="dbobject[@action='build' and @parent-type='table' and
                      @parent-diff!='new']/column">
    <print>
      <xsl:value-of select="concat('&#x0A;alter table ', 
			           ../@parent-qname, ' add ')"/>
      <xsl:call-template name="column">
	<xsl:with-param name="position" select="'1'"/>
	<xsl:with-param name="spaces" select="' '"/>
	<xsl:with-param name="prefix" select="''"/>
      </xsl:call-template>
      <xsl:value-of select="';&#x0A;'"/>
    </print>
  </xsl:template>

  <xsl:template 
      match="dbobject[@action='drop' and @parent-type='table' and
                      @parent-diff!='gone']/column">
    <print>
      <xsl:value-of select="concat('&#x0A;alter table ', 
			           ../@parent-qname, ' drop column ',
				   ../@qname, ';&#x0A;')"/>
    </print>
  </xsl:template>

  <xsl:template 
      match="dbobject[@action='diff' and @parent-type='table']/column">
    <print>
      <xsl:for-each select="../attribute[@status='diff']">
	<xsl:value-of select="concat('&#x0A;alter table ', ../@parent-qname,
			             ' alter column ', ../@qname)"/>
	<xsl:choose>
	  <xsl:when test="@name='nullable'">
	    <xsl:choose>
	      <xsl:when test="@new='yes'">
		<xsl:text> drop not null</xsl:text>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:text> set not null</xsl:text>
	      </xsl:otherwise>
	    </xsl:choose>
	  </xsl:when>
	  <xsl:when test="@name='size'">
	    <xsl:text> type </xsl:text>
	    <xsl:for-each select="../column">
	      <xsl:call-template name="column-typedef">
		<xsl:with-param name="with-null" select="'no'"/>
		</xsl:call-template>
	    </xsl:for-each>
	  </xsl:when>
	</xsl:choose>
	<xsl:text>;</xsl:text>
      </xsl:for-each>
      <xsl:text>&#x0A;</xsl:text>
    </print>
  </xsl:template>

</xsl:stylesheet>


