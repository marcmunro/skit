<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/operator_family">
    <xsl:if test="(../@action='build') and (@auto_generated!='t')">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of 
	    select="concat('create operator family ', ../@qname,
		           ' using ', @method, ';&#x0A;')"/>

	<xsl:if test="opfamily_operator or opfamily_function">
	  <xsl:value-of 
	      select="concat('alter operator family ', ../@qname,
		             ' using ', @method, ' add')"/>
	  <xsl:for-each select="opfamily_operator">
	    <xsl:sort select="arg[@position='left']/@name"/>
	    <xsl:sort select="arg[@position='right']/@name"/>
	    <xsl:sort select="@strategy"/>

	    <xsl:if test="position() != 1">
	      <xsl:text>,</xsl:text>
	    </xsl:if>

	    <xsl:text>&#x0A;  </xsl:text>
	    <xsl:call-template name="operator_defn"/>
	  </xsl:for-each>

	  <xsl:for-each select="opfamily_function">
	    <xsl:sort select="params/param[@position='1']/@type"/>
	    <xsl:sort select="params/param[@position='2']/@type"/>
	    <xsl:sort select="@proc_num"/>

	    <xsl:if test="../opfamily_operator or (position() != 1) ">
	      <xsl:text>,</xsl:text>
	    </xsl:if>

	    <xsl:text>&#x0A;  </xsl:text>
	    <xsl:call-template name="function_defn"/>
	  </xsl:for-each>
	  <xsl:text>;&#x0A;</xsl:text>
	</xsl:if>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>
	  
	<xsl:text></xsl:text>
        <xsl:value-of 
	    select="concat('drop operator family ', ../@qname,
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
		  select="concat('alter operator family ', ../@qname,
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


