<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Cannot rely on standard dbobject template as this build action is
       conditional.  -->
  <xsl:template match="dbobject[@action='build']/type">
    <xsl:if test="not(@extension) and @is_defined = 't'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of select="concat('&#x0A;create type ', ../@qname)"/>
	<xsl:choose>
	  <xsl:when test="@subtype='range'">
	    <xsl:value-of 
		select="concat(' as range (&#x0A;  subtype = ',
			       skit:dbquote(@subtype_schema, @subtype_name))"/>
	    <xsl:if test="@collation_name">
	      <xsl:value-of 
		  select="concat(',&#x0A;  collation = ',
			         skit:dbquote(@collation_schema, 
			                      @collation_name))"/>
	    </xsl:if>
	    <xsl:if test="@opclass_name">
	      <xsl:value-of 
		  select="concat(',&#x0A;  subtype_opclass = ',
			         skit:dbquote(@opclass_schema, 
			                      @opclass_name))"/>
	    </xsl:if>
	    <xsl:if test="@canonical_name">
	      <xsl:value-of 
		  select="concat(',&#x0A;  canonical = ',
			         skit:dbquote(@canonical_schema, 
			                      @canonical_name))"/>
	    </xsl:if>
	    <xsl:if test="@subdiff_name">
	      <xsl:value-of 
		  select="concat(',&#x0A;  subtype_diff = ',
			         skit:dbquote(@subdiff_schema, 
			                      @subdiff_name))"/>
	    </xsl:if>
            <xsl:text>);&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:when test="@subtype='enum'">
	    <xsl:text> as enum (&#x0A;  </xsl:text>
	    <xsl:for-each select="label">
	      <xsl:sort select="@seq_no" data-type="number"/>
	      <xsl:if test="position() != 1">
		<xsl:text>,&#x0A;  </xsl:text>
	      </xsl:if>
              <xsl:value-of select="@label"/>
	    </xsl:for-each>
            <xsl:text>);&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:when test="@subtype='basetype'">
            <xsl:value-of 
		select="concat(' (&#x0A;  input = ',
			       skit:dbquote(*[@type='input']/@schema,
			                    *[@type='input']/@name),
			       ',&#x0A;  output = ',
			       skit:dbquote(*[@type='output']/@schema,
				            *[@type='output']/@name))"/>
	    <xsl:if test="*[@type='send']">
              <xsl:value-of 
		  select="concat(',&#x0A;  send = ',
			         skit:dbquote(*[@type='send']/@schema,
				              *[@type='send']/@name))"/>
	    </xsl:if>
	    <xsl:if test="*[@type='receive']">
              <xsl:value-of 
		  select="concat(',&#x0A;  receive = ',
			         skit:dbquote(*[@type='receive']/@schema,
				              *[@type='receive']/@name))"/>
	    </xsl:if>
	    <xsl:if test="*[@type='analyze']">
              <xsl:value-of 
		  select="concat(',&#x0A;  analyze = ',
			         skit:dbquote(*[@type='analyze']/@schema,
				              *[@type='analyze']/@name))"/>
	    </xsl:if>
	    <xsl:choose>
	      <xsl:when test="@passbyval='yes'">
		<xsl:value-of 
		  select="concat(',&#x0A;  passedbyvalue',
			         ',&#x0A;  internallength = ', @typelen)"/>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:text>,&#x0A;  internallength = </xsl:text>
		<xsl:choose>
		  <xsl:when test="@typelen &lt; 0">
		    <xsl:text>variable</xsl:text>
		  </xsl:when>
		  <xsl:otherwise>
		    <xsl:value-of select="@typelen"/>
		  </xsl:otherwise>
		</xsl:choose>
	      </xsl:otherwise>
	    </xsl:choose>
	    <xsl:if test="@alignment">
              <xsl:value-of 
		  select="concat(',&#x0A;  alignment = ', @alignment)"/>
	    </xsl:if>
	    <xsl:if test="@storage">
              <xsl:value-of 
		  select="concat(',&#x0A;  storage = ', @storage)"/>
	    </xsl:if>
	    <xsl:if test="@type_category">
              <xsl:value-of 
		  select='concat(",&#x0A;  category = &apos;", 
			         @type_category, "&apos;")'/>
	    </xsl:if>
	    <xsl:if test="@is_preferred">
              <xsl:text>,&#x0A;  preferred = </xsl:text>
	      <xsl:choose>
		<xsl:when test="@is_preferred='t'">
		  <xsl:text>true</xsl:text>
		</xsl:when>
		<xsl:otherwise>
		  <xsl:text>false</xsl:text>
		</xsl:otherwise>
	      </xsl:choose>
	    </xsl:if>
	    <xsl:if test="@is_collatable">
              <xsl:text>,&#x0A;  collatable = true</xsl:text>
	    </xsl:if>
	    <xsl:if test="@delimiter">
              <xsl:value-of 
		  select="concat(',&#x0A;  delimiter = ', 
			         $apos, @delimiter, $apos)"/>
	    </xsl:if>
	    <xsl:text>);&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:when test="@subtype='comptype'">
            <xsl:text> as (</xsl:text>
	    <xsl:for-each select="column">
	      <xsl:call-template name="column"/>
	    </xsl:for-each>
	    <xsl:text>);&#x0A;</xsl:text>
	    <xsl:for-each select="column/comment">
	      <xsl:value-of
		  select="concat('&#x0A;comment on column ', 
		                 ../../../@qname, '.',
				 skit:dbquote(../@name),
				 ' is&#x0A;', text(),
				 ';&#x0A;')"/>
	    </xsl:for-each>
	  </xsl:when>
	</xsl:choose>

	<xsl:apply-templates/>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>
  </xsl:template>

  <xsl:template match="type" mode="drop">
    <xsl:value-of select="concat('&#x0A;drop type ', ../@qname)"/>
    <xsl:if test="@subtype='basetype' or @subtype='range'">
      <!-- Basetypes must be dropped using cascade to ensure that
	   the input, output, etc functions are also dropped -->
      <xsl:text> cascade</xsl:text>
    </xsl:if>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="type" mode="diffprep">
    <xsl:for-each select="../attribute[@name='owner']">
      <do-print/>
      <xsl:value-of 
	  select="concat('&#x0A;alter type ', ../@qname,
		         ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="type" mode="diff">
    <xsl:for-each select="../element/element[@type='comment']">
      <do-print/>
      <xsl:value-of select="concat('&#x0A;comment on column ', 
			           ../../@qname, '.',
				   skit:dbquote(../column/@name), ' is')"/>
      <xsl:choose>
	<xsl:when test="@status='gone'">
	  <xsl:text> null;&#x0A;</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="concat('&#x0A;', comment/text(), ';&#x0A;')"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:for-each>
  </xsl:template>

  <!-- Shell types exist as dbobjects mostly to provide a complete
       dependency graph.  This is the only ddl that is necessary for
       such objects.  -->
  <xsl:template match="shelltype" mode="build">
    <xsl:if test="not(@extension)">
      <xsl:value-of select="concat('&#x0A;create type ', 
			    ../@qname, ';&#x0A;')"/>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>
