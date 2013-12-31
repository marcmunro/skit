<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/type">
    <xsl:if test="(../@action='build') and (@is_defined = 't')">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of select="concat('create type ', ../@qname)"/>
	<xsl:choose>
	  <xsl:when test="@subtype='enum'">
	    <xsl:text> as enum (&#x0A;  </xsl:text>
	    <xsl:for-each select="label">
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
	    <xsl:if test="@element">
              <xsl:value-of 
		  select="concat(',&#x0A;  element = ', 'TODO')"/>
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

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
        <xsl:value-of select="concat('drop type ', ../@qname)"/>
	<xsl:if test="@subtype='basetype'">
	  <!-- Basetypes must be dropped using cascade to ensure that
	       the input, output, etc functions are also dropped -->
          <xsl:text> cascade</xsl:text>
	</xsl:if>
        <xsl:text>;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <xsl:if test="../attribute[@name='owner']">
	<print>
	  <xsl:call-template name="feedback"/>
	  <xsl:for-each select="../attribute">
	    <xsl:if test="@name='owner'">
	      <xsl:value-of 
		  select="concat('alter type ', ../@qname,
			         ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
	    </xsl:if>
	  </xsl:for-each>
	</print>
      </xsl:if>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

