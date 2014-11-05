<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="function_header">
    <xsl:value-of select="concat(skit:dbquote(@schema,@name), '(')"/>
    
    <xsl:for-each select="params/param">
      <xsl:if test="position() != 1">
	<xsl:text>,</xsl:text>
      </xsl:if>
      <xsl:text>&#x0A;    </xsl:text>
      <xsl:if test="@name">
	<xsl:value-of select="@name"/>
	<xsl:text> </xsl:text>
      </xsl:if>
      <xsl:if test="@mode">
	<xsl:choose>
	  <xsl:when test="@mode='i'">
	    <xsl:text>in </xsl:text>
	  </xsl:when>
	  <xsl:when test="@mode='o'">
	    <xsl:text>out </xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>inout </xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:if>
      <xsl:value-of select="skit:dbquote(@schema,@type)"/>
      <xsl:if test="@default">
	<xsl:value-of select="concat(' default ', @default)"/>
      </xsl:if>
    </xsl:for-each>
    <xsl:text>)</xsl:text>
  </xsl:template>

  <xsl:template name="function">
    <xsl:text>&#x0A;create or replace&#x0A;function </xsl:text>
    <xsl:call-template name="function_header"/>
    <xsl:text>&#x0A;  returns </xsl:text>
    <xsl:if test="@returns_set">
      <xsl:text>setof </xsl:text>
    </xsl:if>
    
    <xsl:value-of 
	select="concat(skit:dbquote(result/@schema,result/@type), '&#x0A;as')"/>
    
    <xsl:choose>
      <xsl:when test="@language='internal'">
	<xsl:text> &apos;</xsl:text>
	<xsl:value-of select="concat(source/text(), $apos, '&#x0A;')"/>
      </xsl:when>
      <xsl:when test="@language='c'">
	<xsl:value-of 
	    select="concat(' ', $apos, @bin, $apos, ', ', $apos,
		    source/text(), $apos, '&#x0A;')"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of 
	    select="concat('&#x0A;$_$', source/text(), '$_$&#x0A;')"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:value-of 
	select="concat('language ', @language, ' ', @volatility)"/>
    <xsl:if test="@is_strict='yes'">
      <xsl:text> strict</xsl:text>
    </xsl:if>
    <xsl:if test="@security_definer='yes'">
      <xsl:text> security definer</xsl:text>
    </xsl:if>
    <xsl:if test="@rows">
      <xsl:value-of select="concat(' rows ', @rows)"/>
    </xsl:if>
    <xsl:value-of select="concat(' cost ', @cost)"/>
    <xsl:for-each select="config_setting">
      <xsl:value-of select="concat('&#x0A;set ', @name, ' = ', @setting)"/>
    </xsl:for-each>
    <xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="function" mode="build">
    <xsl:call-template name="function"/>
  </xsl:template>

  <xsl:template match="dbobject[@action = 'drop']/function">
    <xsl:if test="not(handler-for-type)">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
	  
	<xsl:value-of 
	    select="concat('&#x0A;drop function ', 
		           ../@qname, ';&#x0A;&#x0A;')"/>
	  
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>
  </xsl:template>

  <xsl:template match="function" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:text>&#x0A;alter function </xsl:text>
      <xsl:call-template name="function_header"/>
      <xsl:for-each select="../attribute[@name='owner']">
	<xsl:value-of select="concat(' owner to ', skit:dbquote($username), 
			             ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="function" mode="diff">
    <do-print/>
    <xsl:if test="../attribute[@name='owner']">
      <xsl:text>&#x0A;alter function </xsl:text>
      <xsl:call-template name="function_header"/>
      <xsl:for-each select="../attribute[@name='owner']">
	<xsl:value-of select="concat(' owner to ', skit:dbquote(@new), 
			             ';&#x0A;')"/>
      </xsl:for-each>
    </xsl:if>
    <xsl:variable name="action">
      <xsl:choose>
	<xsl:when 
	    test="../element[@status='diff']/element[@status='diff']/attribute[@name='name']">
	  <!-- One of the parameters, names has changed. -->
	  <xsl:value-of select="'Rebuild'"/>
	</xsl:when>
	<xsl:when 
	    test="../element[@status='diff' and @type='source']">
	  <!-- The function's code has changed. -->
	  <xsl:value-of select="'Rebuild'"/>
	</xsl:when>
	<xsl:when 
	    test="../attribute[@status='diff' and @name='bin']">
	  <!-- The library for the function has changed. -->
	  <xsl:value-of select="'Rebuild'"/>
	</xsl:when>
	<xsl:when 
	    test="../attribute[@status='diff' and @name='language']">
	  <!-- The language of the function has changed, which seems
	       unlikely.  -->
	  <xsl:value-of select="'Rebuild'"/>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:value-of select="'None'"/>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="$action='Rebuild'">
	<xsl:call-template name="function"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:if test="../attribute or ../element[@type='config_setting']">
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
	<xsl:if test="../attribute[@name='owner']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:call-template name="function_header"/>
	  <xsl:value-of select="concat(' owner to ', @owner, ';&#x0A;')"/>
	</xsl:if>
	<xsl:if test="../attribute[@name='volatility']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:call-template name="function_header"/>
	  <xsl:value-of select="concat(' ', @volatility, ';&#x0A;')"/>
	</xsl:if>
	<xsl:if test="../attribute[@name='is_strict']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:call-template name="function_header"/>
	  <xsl:choose>
	    <xsl:when test="@is_strict='yes'">
	      <xsl:text> strict;&#x0A;</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:text> called on null input;&#x0A;</xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:if>
	<xsl:if test="../attribute[@name='security_definer']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:call-template name="function_header"/>
	  <xsl:choose>
	    <xsl:when test="@security_definer='yes'">
	      <xsl:text> security definer;&#x0A;</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:text> security invoker;&#x0A;</xsl:text>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:if>
	<xsl:if test="../attribute[@name='cost']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:call-template name="function_header"/>
	  <xsl:value-of select="concat(' cost ', @cost, ';&#x0A;')"/>
	</xsl:if>
	<xsl:if test="../attribute[@name='rows']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:call-template name="function_header"/>
	  <xsl:value-of select="concat(' rows ', @rows, ';&#x0A;')"/>
	</xsl:if>
	<xsl:for-each select="../element[@type='config_setting']">
	  <xsl:text>alter function </xsl:text>
	  <xsl:for-each select="../function">
	    <xsl:call-template name="function_header"/>
	  </xsl:for-each>
	  <xsl:choose>
	    <xsl:when test="@status='gone'">
	      <xsl:value-of 
		  select="concat('&#x0A;reset ', config_setting/@name, 
			         ';&#x0A;')"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of 
		  select="concat('&#x0A;set ', config_setting/@name, ' = ', 
			         config_setting/@setting,';&#x0A;')"/>
	    </xsl:otherwise>
	  </xsl:choose>		
	</xsl:for-each>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>

