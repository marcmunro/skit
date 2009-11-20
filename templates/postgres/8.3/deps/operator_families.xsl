<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!-- Operator classes -->
  <xsl:template match="operator_family">
    <xsl:param name="parent_core" select="'NOT SUPPLIED'"/>
    <xsl:variable name="operator_family_fqn" 
		  select="concat('operator_family.', 
			  ancestor::database/@name, '.', 
			  @schema, '.', @name, 
			  '(', @method, ')')"/>
    <dbobject type="operator_family" fqn="{$operator_family_fqn}"
	      name="{@name}" qname="{skit:dbquote(@schema,@name)}">
      <dependencies>

	<!-- operators -->
	<!-- The following is copied from oerator_classes.xsl
	     TODO: Make this a template call to eliminate redundant code -->
	<xsl:for-each select="opclass_operator">
	  <xsl:if test="@schema != 'pg_catalog'">
	    <dependency fqn="{concat('operator.', 
			     ancestor::database/@name, '.', 
			     @schema, '.', @name, '(',
			     arg[@position='left']/@schema, '.',
			     arg[@position='left']/@name, ',',
			     arg[@position='right']/@schema, '.',
			     arg[@position='right']/@name, ')')}"/>
	    
	  </xsl:if>
	</xsl:for-each>

	<!-- functions -->
	<!-- The following is copied from oerator_classes.xsl
	     TODO: Make this a template call to eliminate redundant code -->
	<xsl:for-each select="opclass_function">
	  <xsl:if test="@schema != 'pg_catalog'">
	    <dependency fqn="{concat('function.', 
			     ancestor::database/@name, '.', 
			     @schema, '.', @name, '(',
			     params/param[@position='1']/@schema, '.',
			     params/param[@position='1']/@type, ',',
			     params/param[@position='2']/@schema, '.',
			     params/param[@position='2']/@type, ')')}"/>
	    
	  </xsl:if>
	</xsl:for-each>

      </dependencies>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="parent_core" 
			  select="concat($parent_core, '.', @signature)"/>
	</xsl:apply-templates>
      </xsl:copy>
    </dbobject>

    <xsl:if test="@auto_generated='t'">
      <xsl:if test="comment">
	<!-- If the operator family was auto-created, we must create the
	     comment only after the operator class has been created. -->
	<xsl:variable name="comment_fqn" 
		      select="concat('comment.', 
			             ancestor::database/@name, '.', 
				     @schema, '.', @name, 
				     '(', @method, ')')"/>
	<dbobject type="comment" fqn="{$comment_fqn}"
	          name="{@name}" qname="{skit:dbquote(@schema,@name)}"
		  nolist="true" method="{@method}">
	  <dependencies>
	    <dependency fqn="{concat('operator_class.', $parent_core,
			      '.', @name)}"/>
	  </dependencies>
	  <xsl:for-each select="comment">
	    <xsl:copy select=".">
	      <xsl:copy-of select="text()"/>
	    </xsl:copy>
	  </xsl:for-each>
	</dbobject>
      </xsl:if>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

<!-- Keep this comment at the end of the file
Local variables:
mode: xml
sgml-omittag:nil
sgml-shorttag:nil
sgml-namecase-general:nil
sgml-general-insert-case:lower
sgml-minimize-attributes:nil
sgml-always-quote-attributes:t
sgml-indent-step:2
sgml-indent-data:t
sgml-parent-document:nil
sgml-exposed-tags:nil
sgml-local-catalogs:nil
sgml-local-ecat-files:nil
End:
-->
