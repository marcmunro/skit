<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xi="http://www.w3.org/2003/XInclude"
    xmlns:skit="http://www.bloodnok.com/xml/skit"
    version="1.0">

  <xsl:template name="alter-table-only-intro">
    <xsl:param name="tbl-qname" select="../@qname"/>
    <xsl:text>&#x0A;alter </xsl:text>
    <xsl:if test="@foreign='t'">
      <xsl:text>foreign </xsl:text>
    </xsl:if>
    <xsl:value-of select="concat('table only ', $tbl-qname)"/>
  </xsl:template>

  <xsl:template name="alter-table-intro">
    <xsl:param name="tbl-qname" select="../@qname"/>
    <xsl:text>&#x0A;alter </xsl:text>
    <xsl:if test="@foreign='t'">
      <xsl:text>foreign </xsl:text>
    </xsl:if>
    <xsl:value-of select="concat('table ', $tbl-qname)"/>
  </xsl:template>


  <xsl:template match="table" mode="build">
    <xsl:text>create </xsl:text>
    <xsl:if test="@is_foreign='t'">
      <xsl:text>foreign </xsl:text>
    </xsl:if>
    <xsl:value-of select="concat('table ', ../@qname, ' (')"/>
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
    <xsl:if test ="@tablespace_is_default!='t'">
      <xsl:value-of select="concat('&#x0A;tablespace ', @tablespace)"/>
    </xsl:if>
    <xsl:if test="@is_foreign='t'">
      <xsl:value-of select="concat('&#x0A;server ', 
	                           skit:dbquote(@foreign_server_name))"/>
      <xsl:if test="@foreign_table_options">
	<xsl:value-of select="concat('&#x0A;options (', 
			             @foreign_table_options, ')')"/>
      </xsl:if>
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
    <xsl:text>&#x0A;drop </xsl:text>
    <xsl:if test="@is_foreign='t'">
      <xsl:text>foreign </xsl:text>
    </xsl:if>
    <xsl:value-of 
	    select="concat('table ', ../@qname, ';&#x0A;')"/>
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

    <xsl:if test="../attribute[@name='tablespace']">
      <do-print/>
      <xsl:call-template name="alter-table-only-intro"/>
      <xsl:for-each select="../attribute[@name='tablespace']">
	<xsl:value-of 
	    select="concat(' set tablespace ', skit:dbquote(@new), ';&#x0A;')"/>
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
    <xsl:for-each select="../element/element[@type='inherited-column' and 
			                     @status='new']">
      <xsl:for-each select="inherited-column[@storage_policy]">
	<do-print/>
	<xsl:variable name="tblqname"
		      select="skit:dbquote(../../../table/@schema,
			                   ../../../table/@name)"/>
	<xsl:value-of select="concat('alter table ', $tblqname)"/>
	<xsl:call-template name="column-storage"/>
	<xsl:text>;&#x0A;</xsl:text>
      </xsl:for-each>
    </xsl:for-each>
  </xsl:template>

</xsl:stylesheet>


