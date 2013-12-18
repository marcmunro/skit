<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="operator-signature">
    <xsl:value-of 
	select="concat('operator ',
		        skit:dbquote(@schema),
			'.', @name, '(')"/>

    <xsl:choose>
      <xsl:when test="arg[@position='left']">
	<xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
			                   arg[@position='left']/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>none</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    
    <xsl:text>, </xsl:text>
    <xsl:choose>
      <xsl:when test="arg[@position='right']">
	<xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
				           arg[@position='right']/@name)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text>none</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>)</xsl:text>
  </xsl:template>


  <xsl:template match="dbobject/operator">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of 
	    select="concat('create operator ',
		           skit:dbquote(@schema),
			   '.', @name,
			   ' (&#x0A;  leftarg = ')"/>
	<xsl:choose>
	  <xsl:when test="arg[@position='left']">
            <xsl:value-of select="skit:dbquote(arg[@position='left']/@schema,
				               arg[@position='left']/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>,&#x0A;  rightarg = </xsl:text>
	<xsl:choose>
	  <xsl:when test="arg[@position='right']">
            <xsl:value-of select="skit:dbquote(arg[@position='right']/@schema,
				               arg[@position='right']/@name)"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>none</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:text>,&#x0A;  procedure = </xsl:text>
        <xsl:value-of 
	   select="skit:dbquote(procedure/@schema,procedure/@name)"/>
	<xsl:if test="commutator">
	  <xsl:text>,&#x0A;  commutator = operator(</xsl:text>
          <xsl:value-of 
	     select="concat(commutator/@schema, '.', commutator/@name, ')')"/>
	</xsl:if>
	<xsl:if test="negator">
	  <xsl:text>,&#x0A;  negator = operator(</xsl:text>
          <xsl:value-of 
	     select="concat(negator/@schema, '.', negator/@name, ')')"/>
	</xsl:if>
	<xsl:if test="restrict">
	  <xsl:text>,&#x0A;  restrict = </xsl:text>
          <xsl:value-of select="skit:dbquote(restrict/@schema,restrict/@name)"/>
	</xsl:if>
	<xsl:if test="join">
	  <xsl:text>,&#x0A;  join = </xsl:text>
          <xsl:value-of select="skit:dbquote(join/@schema,join/@name)"/>
	</xsl:if>
	<xsl:if test="@hashes">
	  <xsl:text>,&#x0A;  hashes</xsl:text>
	</xsl:if>
	<xsl:if test="@merges">
	  <xsl:text>,&#x0A;  merges</xsl:text>
	</xsl:if>
	<xsl:text>&#x0A;);&#x0A;</xsl:text>

	<!-- Comment handling is explicit due to the need to use a custom 
	     object signature for operators. -->
	<xsl:for-each select="comment">
	  <xsl:call-template name="comment">
	    <xsl:with-param name="objnode" select=".."/>
	    <xsl:with-param name="text" select="./text()"/>
	    <xsl:with-param name="sig">
	      <xsl:for-each select="..">
		<xsl:call-template name="operator-signature"/>
	      </xsl:for-each>
	    </xsl:with-param>
	  </xsl:call-template>
	</xsl:for-each>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

	<xsl:text>drop </xsl:text>
	<xsl:call-template name="operator-signature"/>
      	<xsl:text>;&#x0A;</xsl:text>
	  
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <xsl:if test="../attribute[@name='owner']">
	<print>
	  <xsl:call-template name="feedback"/>
	  <xsl:variable name="sig">
	    <xsl:call-template name="operator-signature"/>
	  </xsl:variable>
	  <xsl:for-each select="../attribute">
	    <xsl:if test="@name='owner'">
	      <xsl:value-of 
		  select="concat('alter ', $sig, ' owner to ', 
			         skit:dbquote(@new), ';&#x0A;')"/>
	    </xsl:if>
	  </xsl:for-each>
	</print>
      </xsl:if>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="commentdiff">
	  <xsl:with-param name="sig">
	    <xsl:call-template name="operator-signature"/>
	  </xsl:with-param>
	</xsl:call-template>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

