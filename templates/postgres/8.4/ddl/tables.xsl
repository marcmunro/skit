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
		      string-length(skit:dbquote(@name))), ' ')"/>
    <xsl:call-template name="column-typedef"/>
    <xsl:if test="@default">
      <xsl:value-of 
	  select="concat('&#x0A;                                    default ', 
		         @default)"/>
    </xsl:if>
  </xsl:template>

  <!-- Produce the alter column part of an alter table statement for a
       stats target.   
       The context for this call is the comment element. -->
  <xsl:template name="column-stats">
    <xsl:value-of 
	select="concat(' alter column ', skit:dbquote(@name),
		       ' set statistics ', @stats_target)"/>
    <xsl:if test="not(@stats_target)">
      <xsl:text>-1</xsl:text>
    </xsl:if>
  </xsl:template>

  <!-- Produce the alter column part of an alter table statement for a
       storage policy.   
       The context for this call is the comment element. -->
  <xsl:template name="column-storage">
    <xsl:value-of 
	select="concat(' alter column ', skit:dbquote(@name), 
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
      <xsl:otherwise>
	<!-- This may not be correct in all cases but is the best I can
	     be bothered to do.  -->
	<xsl:value-of select="'main'"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- Produce the comment for a column.  
       The context for this call is the comment element. -->
  <xsl:template name="column-comment">
    <xsl:param name="tbl-qname" select="../../@qname"/>
    <xsl:if test="comment">
      <xsl:value-of 
	  select="concat('&#x0A;comment on column ', $tbl-qname, '.',
	                 skit:dbquote(@name), ' is&#x0A;', comment/text(),
			 ';&#x0A;')"/>
    </xsl:if>
  </xsl:template>

  <xsl:template name="alter-table-only-intro">
    <xsl:param name="tbl-qname" select="../@qname"/>
    <xsl:value-of select="concat('&#x0A;alter table only ', $tbl-qname)"/>
  </xsl:template>

  <xsl:template name="alter-table-intro">
    <xsl:param name="tbl-qname" select="../@qname"/>
    <xsl:value-of select="concat('&#x0A;alter table ', $tbl-qname)"/>
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
      <xsl:call-template name="alter-table-only-intro"/>
      <xsl:for-each select="column[@stats_target]">
	<xsl:if test="position() != '1'">
	  <xsl:value-of select="', '"/>
	</xsl:if>
	<xsl:text>&#x0A; </xsl:text>
	<xsl:call-template name="column-stats"/>
      </xsl:for-each>
      <xsl:value-of select="';&#x0A;'"/>
    </xsl:if>
    
    <xsl:if test="column[@storage_policy]">
      <xsl:call-template name="alter-table-only-intro"/>
      <xsl:for-each select="column[@storage_policy]">
	<xsl:if test="position() != '1'">
	  <xsl:value-of select="', '"/>
	</xsl:if>
	<xsl:text>&#x0A; </xsl:text>
	<xsl:call-template name="column-storage"/>
      </xsl:for-each>
      <xsl:text>;&#x0A;</xsl:text>
    </xsl:if>
    
    <xsl:for-each select="column">
      <xsl:call-template name="column-comment"/>
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
      <xsl:call-template name="alter-table-only-intro"/>
	<xsl:value-of 
	    select="concat(' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>

    <xsl:for-each select="../element[@type='inherits' and @status='gone']">
      <do-print/>
      <xsl:call-template name="alter-table-only-intro"/>
      <xsl:value-of 
	  select="concat(' no inherit ',
		         skit:dbquote(inherits/@schema), '.',
			 skit:dbquote(inherits/@name), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="table" mode="diff">
    <xsl:for-each select="../element[@type='inherits' and @status='new']">
      <do-print/>
      <xsl:call-template name="alter-table-only-intro"/>
      <xsl:value-of 
	  select="concat(' inherit ',
		         skit:dbquote(inherits/@schema), '.',
			 skit:dbquote(inherits/@name), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <!-- Context is the column element.  -->
  <xsl:template name="build-column-from-diff">
    <xsl:value-of select="concat('&#x0A;alter table ', 
			           ../@parent-qname, ' add column ')"/>
    <xsl:call-template name="column">
      <xsl:with-param name="position" select="'1'"/>
      <xsl:with-param name="spaces" select="' '"/>
      <xsl:with-param name="prefix" select="''"/>
    </xsl:call-template>
    <xsl:text>;</xsl:text>
  </xsl:template>

  <!-- Column drops for rebuilds. -->
  <xsl:template 
      match="dbobject[@action='drop' and @parent-type='table' and
                      @parent-diff!='gone']/column[@is_local='t']">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>
      <xsl:value-of select="concat('&#x0A;alter table ', 
			           ../@parent-qname, ' drop column ',
				   ../@qname, ';&#x0A;')"/>
      <xsl:call-template name="reset_owner"/>
    </print>
  </xsl:template>


  <!-- Column builds for rebuilds. -->
  <xsl:template 
      match="dbobject[@action='build' and @parent-type='table' and
                      @parent-diff!='new']/column[@is_local='t']">
    <print>
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>

      <xsl:call-template name="build-column-from-diff"/>

      <xsl:if test="@storage_policy">
	<xsl:call-template name="alter-table-only-intro">
	  <xsl:with-param name="tbl-qname" select="../@parent-qname"/>
	</xsl:call-template>
	<xsl:call-template name="column-storage"/>
	<xsl:text>;</xsl:text>
      </xsl:if>

      <xsl:if test="@stats_target">
	<xsl:call-template name="alter-table-only-intro">
	  <xsl:with-param name="tbl-qname" select="../@parent-qname"/>
	</xsl:call-template>
	<xsl:call-template name="column-stats"/>
	<xsl:text>;</xsl:text>
      </xsl:if>

      <xsl:call-template name="column-comment">
	<xsl:with-param name="tbl-qname" select="../@parent-qname"/>
      </xsl:call-template>
      <xsl:call-template name="reset_owner"/>
    </print>
  </xsl:template>


  <xsl:template name="alter-table-col">
    <xsl:call-template name="alter-table-intro">
      <xsl:with-param name="tbl-qname" select="../@parent-qname"/>
    </xsl:call-template>
    <xsl:value-of select="concat(' alter column ', ../@qname, ' ')"/>
  </xsl:template>


  <!-- Column diffs (prep) -->
  <xsl:template 
      match="dbobject[@action='diffprep' and @parent-type='table']/column">
    <print conditional="yes">
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>

      <!-- If a column has become inherited...  -->
      <xsl:if test="../attribute[@name='is_local' and @new='f']">
	<!-- If the column will be automatically added when the
	     inherited column is created, we must drop our local
	     copy. -->
	<xsl:if test="../inherits-from[not(@eninherit-table)]">
	  <do-print/>
	  <xsl:value-of select="concat('&#x0A;alter table ', 
			               ../@parent-qname, ' drop column ',
				       ../@qname, ';&#x0A;')"/>
	</xsl:if>
      </xsl:if>
      
      <xsl:call-template name="reset_owner"/>
    </print>
  </xsl:template>

  <!-- Column diffs -->
  <xsl:template 
      match="dbobject[@action='diff' and @parent-type='table']/column">
    <print conditional="yes">
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>

      <!-- If a column has become disinherited by means of being
	   dropped, we must recreate it.   -->
      <xsl:if test="../attribute[@name='is_local' and @new='t']">
	<xsl:variable name="col-name" select="@name"/>
	<xsl:if test="../inherits-from[@column=$col-name 
		                       and @col-diff='gone']">

	  <do-print/>
	  <xsl:call-template name="build-column-from-diff"/>
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
      </xsl:if>

      <!-- If nullability is being allowed, deal with that first. -->
      <xsl:for-each 
	  select="../attribute[@status='diff' and @name='nullable' and
		               @new='yes']">
	<do-print/>
	<xsl:call-template name="alter-table-col"/>
	<xsl:text>drop not null;</xsl:text>
      </xsl:for-each>

      <xsl:if test="@is_local='t'">
	<xsl:for-each 
	    select="../attribute[@status='diff' and 
		                 (@name='size' or @name='precision')]">
	  <do-print/>
	  <xsl:call-template name="alter-table-col"/>
	  <xsl:text>type </xsl:text>
	  <xsl:for-each select="../column">
	    <xsl:call-template name="column-typedef">
	      <xsl:with-param name="with-null" select="'no'"/>
	    </xsl:call-template>
	  </xsl:for-each>
	  <xsl:text>;</xsl:text>
	</xsl:for-each>
      </xsl:if>

      <xsl:for-each 
	  select="../attribute[@status!='gone' and @name='default']">
	<do-print/>
	<xsl:call-template name="alter-table-col"/>
	<xsl:value-of select="concat('set default ', @new, ';')"/>
      </xsl:for-each>

      <xsl:for-each 
	  select="../attribute[@status='gone' and @name='default']">
	<do-print/>
	<xsl:call-template name="alter-table-col"/>
	<xsl:text>drop default;</xsl:text>
      </xsl:for-each>

      <xsl:for-each 
	  select="../attribute[@status='diff' and @name='nullable' and 
		               @new='no']">
	<do-print/>
	<xsl:call-template name="alter-table-col"/>
	<xsl:text>set not null;</xsl:text>
      </xsl:for-each>
      
      <xsl:if test="../attribute[@name='storage_policy']">
	<do-print/>
	<xsl:call-template name="alter-table-only-intro">
	  <xsl:with-param name="tbl-qname" select="../@parent-qname"/>
	</xsl:call-template>
	<xsl:call-template name="column-storage"/>
	<xsl:text>;</xsl:text>
      </xsl:if>

      <xsl:if test="../attribute[@name='stats_target']">
	<do-print/>
	<xsl:call-template name="alter-table-only-intro">
	  <xsl:with-param name="tbl-qname" select="../@parent-qname"/>
	</xsl:call-template>
	<xsl:call-template name="column-stats"/>
	<xsl:text>;</xsl:text>
      </xsl:if>

      <xsl:call-template name="commentdiff">
	<xsl:with-param name="sig">
	  <xsl:value-of 
	      select="concat('column ', ../@parent-qname, '.', ../@qname)"/>
	</xsl:with-param>
      </xsl:call-template>

      <xsl:call-template name="reset_owner"/>
    </print>
  </xsl:template>

</xsl:stylesheet>


