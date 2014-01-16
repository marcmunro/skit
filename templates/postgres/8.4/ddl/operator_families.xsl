<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Cannot use the default dbobject mechanism as creation of operator
       families is conditional (on @auto-generated) -->
  <xsl:template match="dbobject[@action='build']/operator_family">
    <xsl:if test="@auto_generated!='t'">
      <print>
	<xsl:call-template name="feedback"/>
	<xsl:call-template name="set_owner"/>

        <xsl:value-of 
	    select="concat('&#x0A;create operator family ', ../@qname,
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

	<!-- If context value is not the same as owner, we must be using 
	     a superuser context and we will have created the operator 
	     family under that user.  Now we must alter the ownership. -->
	<xsl:if test="@owner != ../context[@name='owner']/@value">
	  <xsl:value-of 
	      select="concat('&#x0A;alter operator family ', ../@qname,
			     ' using ', @method,
			     ' owner to ', skit:dbquote(@owner), ';&#x0A;')"/>
	</xsl:if>

	<xsl:apply-templates/>  <!-- Deal with comments -->
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>
  </xsl:template>

  <xsl:template match="operator_family" mode="drop">
    <xsl:value-of 
	select="concat('&#x0A;drop operator family ', ../@qname,
		       ' using ', @method, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template match="operator_family" mode="diffprep">
    <xsl:if test="../attribute[@name='owner']">
      <do-print/>
      <xsl:variable name="method" select="@method"/>
      <xsl:call-template name="feedback"/>
      <xsl:for-each select="../attribute">
	<xsl:if test="@name='owner'">
	  <xsl:value-of 
	      select="concat('&#x0A;alter operator family ', ../@qname,
			     ' using ', $method,
			     ' owner to ', skit:dbquote(@new), ';&#x0A;')"/>
	</xsl:if>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <xsl:template match="operator_family" mode="diffcomplete">
    <xsl:if test="../element">
      <do-print/>
      <xsl:text>&#x0A;</xsl:text>
      <xsl:for-each 
	  select="../element[(@type='opfamily_operator' or
		              @type='opfamily_function') and
			      (@status='gone' or @status='diff')]">

	<!-- DEBUG NOTE: The code below may not work for dropping the
	     original version of a diff operator.  We will deal with
	     that as and when it becomes a problem.  At that point we
	     will have to reconstruct the original operator definition -->
	<xsl:value-of 
	    select="concat('alter operator family ', ../@qname,
		           ' using ', ../operator_family/@method, 
			   ' drop&#x0A;  ')"/>
	<xsl:choose>
	  <xsl:when test="@type='opfamily_operator'">
	    <xsl:for-each 
		select="opfamily_operator">
	      <xsl:value-of select="concat('operator ', @strategy, ' ')"/>
	      <xsl:call-template name="operator_params_defn"/>
	    </xsl:for-each>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:for-each 
		select="opfamily_function">
	      <xsl:value-of select="concat('function ', @proc_num, ' ')"/>
	      <xsl:call-template name="function_params_defn"/>
	    </xsl:for-each>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>;&#x0A;</xsl:text>
      </xsl:for-each>

      <xsl:for-each 
	  select="../element[(@type='opfamily_operator' or
		              @type='opfamily_function') and
			      (@status='new' or @status='diff')]">
	<xsl:value-of 
	    select="concat('alter operator family ', ../@qname,
		           ' using ', ../operator_family/@method, 
			   ' add&#x0A;  ')"/>
	<xsl:choose>
	  <xsl:when test="@type='opfamily_operator'">
	    <xsl:for-each 
		select="opfamily_operator">
	      <xsl:call-template name="operator_defn"/>
	    </xsl:for-each>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:for-each select="opfamily_function">
	      <xsl:call-template name="function_defn"/>
	    </xsl:for-each>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>;&#x0A;</xsl:text>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
