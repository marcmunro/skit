<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="operator_defn">
    <xsl:value-of 
	select="concat('operator ', @strategy, ' ',
		       skit:dbquote(@schema), '.', @name, '(',
                       skit:dbquote(arg[@position='left']/@schema,
                                    arg[@position='left']/@name), ',',
                       skit:dbquote(arg[@position='right']/@schema,
                                    arg[@position='right']/@name), ')')"/> 
  </xsl:template>

  <xsl:template name="function_defn">
    <xsl:value-of 
	select="concat('function ', @proc_num, ' ',
		       skit:dbquote(@schema,@name), '(',
	               skit:dbquote(params/param[@position='1']/@schema,
		                    params/param[@position='1']/@type), ',',
		       skit:dbquote(params/param[@position='2']/@schema,
		                    params/param[@position='2']/@type), ')')"/>
  </xsl:template>

  <xsl:template match="dbobject/operator_class">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of 
	    select="concat('create operator class ', ../@qname, '&#x0A;  ')"/>
	<xsl:if test="@is_default = 't'">
	  <xsl:text>default </xsl:text>
	</xsl:if>
        <xsl:value-of 
	    select="concat('for type ',
		           skit:dbquote(@intype_schema,@intype_name),
			   ' using ', @method)"/>
	<xsl:if test="(@name != @family) or (@schema != @family_schema)">
          <xsl:value-of select="concat(' family ', 
				       skit:dbquote(@family_schema,@family))"/>
	</xsl:if >
	<xsl:text> as</xsl:text>
	<xsl:for-each select="opclass_operator">
	  <xsl:sort select="@strategy"/>
	  <xsl:if test="position() != 1">
	    <xsl:text>,</xsl:text>
	  </xsl:if>
	  <xsl:text>&#x0A;  </xsl:text>
	  <xsl:call-template name="operator_defn"/>
	</xsl:for-each>

	<xsl:for-each select="opclass_function">
	  <xsl:sort select="@proc_num"/>
	  <xsl:text>,&#x0A;  </xsl:text>
	  <xsl:call-template name="function_defn"/>
	</xsl:for-each>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
	  
        <xsl:value-of 
	    select="concat('drop operator class ', ../@qname,
		           ' using ', @method, ';&#x0A;')"/>

	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <xsl:if test="../attribute[@name='owner']">
	<print>
	  <xsl:variable name="method" select="@method"/>
	  <xsl:call-template name="feedback"/>
	  <xsl:for-each select="../attribute">
	    <xsl:if test="@name='owner'">
	      <xsl:value-of 
		  select="concat('alter operator class ', ../@qname,
			         ' using ', $method,
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


