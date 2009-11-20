<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/table">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

        <xsl:text>create table </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text> (</xsl:text>
	<xsl:for-each select="column[@is_local='t']">
	  <xsl:sort select="@colnum"/>
	  <xsl:if test="position() != 1">
            <xsl:text>,</xsl:text>
	  </xsl:if>
          <xsl:text>&#x0A;  </xsl:text>
	  <xsl:value-of 
	     select="concat(skit:dbquote(@name),
		            substring('                              ',
		                      string-length(skit:dbquote(@name))))"/>
          <xsl:text> </xsl:text>
	  <xsl:value-of select="skit:dbquote(@type_schema, @type)"/>
	  <xsl:if test="@size">
            <xsl:text>(</xsl:text>
	    <xsl:value-of select="@size"/>
	    <xsl:if test="@precision">
              <xsl:text>,</xsl:text>
	      <xsl:value-of select="@precision"/>
	    </xsl:if>
            <xsl:text>)</xsl:text>
	  </xsl:if>
	  <xsl:if test="@nullable='no'">
            <xsl:text> not null</xsl:text>
	  </xsl:if>
	  <xsl:if test="@default">
	    <xsl:text>&#x0A;                                    default </xsl:text>
	    <xsl:value-of select="@default"/>
	  </xsl:if>
	</xsl:for-each>
	<xsl:text>&#x0A;)</xsl:text>
	<xsl:if test ="inherits">
	  <xsl:text> inherits (</xsl:text>
	  <xsl:for-each select="inherits">
	    <xsl:sort select="@inherit_order"/>
	    <xsl:if test="position() != 1">
	      <xsl:text>, </xsl:text>
	    </xsl:if>
	    <xsl:value-of select="skit:dbquote(@schema,@name)"/>
	  </xsl:for-each>
	  <xsl:text>)</xsl:text>
	</xsl:if>
	<xsl:if test ="@tablespace">
	  <xsl:text>&#x0A;tablespace </xsl:text>
	  <xsl:value-of select="@tablespace"/>
	</xsl:if>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:for-each select="constraint">
	  <xsl:if test="(@type = 'unique') or (@type = 'primary key')">
	    <xsl:text>&#x0A;alter table only </xsl:text>
            <xsl:value-of select="../../@qname"/>
	    <xsl:text>&#x0A;  add constraint </xsl:text>
	    <xsl:value-of select="skit:dbquote(@name)"/>
	    <xsl:choose>
	      <xsl:when test="@type = 'unique'">
		<xsl:text>&#x0A;  unique(</xsl:text>
	      </xsl:when>
	      <xsl:otherwise>
		<xsl:text>&#x0A;  primary key(</xsl:text>
	      </xsl:otherwise>
	    </xsl:choose>	
	    <xsl:for-each select="column">
	      <xsl:if test="position() != '1'">
		<xsl:text>, </xsl:text>
	      </xsl:if>
	      <xsl:value-of select="skit:dbquote(@name)"/>
	    </xsl:for-each>
	    <xsl:text>)</xsl:text>
	    <xsl:if test="@options">
	      <xsl:text>&#x0A;  with (</xsl:text>
	      <xsl:value-of select="@options"/>
	      <xsl:text>)</xsl:text>
	    </xsl:if>
	    <xsl:if test="@tablespace">
	      <xsl:text>&#x0A;  using index tablespace </xsl:text>
	      <xsl:value-of select="skit:dbquote(@tablespace)"/>
	    </xsl:if>
	    <xsl:text>;&#x0A;&#x0A;</xsl:text>
	  </xsl:if>
	</xsl:for-each>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:for-each select="column[@is_local='t']/comment">
	  <xsl:text>&#x0A;comment on column </xsl:text>
          <xsl:value-of select="concat(../../../@qname, '.',
	                        skit:dbquote(../@name))"/>
	  <xsl:text> is&#x0A;</xsl:text>
	  <xsl:value-of select="text()"/>
	  <xsl:text>;&#x0A;</xsl:text>
	</xsl:for-each>

	<xsl:call-template name="reset_owner"/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="set_owner"/>
        <xsl:text>&#x0A;drop table </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
      </print>
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
