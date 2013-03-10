<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template name="fk_event">
    <xsl:param name="event"/>
    <xsl:param name="action_code"/>
    <xsl:if test="$action_code and ($action_code != 'a')">
      <xsl:text>&#x0A;  on </xsl:text>
      <xsl:value-of select="$event"/>
      <xsl:choose>
	<xsl:when test="$action_code = 'c'">
	  <xsl:text> cascade</xsl:text>
	</xsl:when>
	<xsl:when test="$action_code = 'd'">
	  <xsl:text> set default</xsl:text>
	</xsl:when>
	<xsl:when test="$action_code = 'n'">
	  <xsl:text> set null</xsl:text>
	</xsl:when>
	<xsl:when test="$action_code = 'r'">
	  <xsl:text> restrict</xsl:text>
	</xsl:when>
      </xsl:choose>
    </xsl:if>
  </xsl:template>

  <xsl:template match="dbobject/constraint">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>

	<xsl:text>&#x0A;alter table only </xsl:text>
        <xsl:value-of select="../@table_qname"/>
	<xsl:text>&#x0A;  add constraint </xsl:text>
	<xsl:value-of select="skit:dbquote(@name)"/>

	<xsl:choose>
	  <xsl:when test="@type='foreign key'">
	    <xsl:text>&#x0A;  foreign key(</xsl:text>
	    <xsl:for-each select="column">
	      <xsl:if test="position() != '1'">
		<xsl:text>, </xsl:text>
	      </xsl:if>
	      <xsl:value-of select="skit:dbquote(@name)"/>
	    </xsl:for-each>
	    <xsl:text>)&#x0A;  references </xsl:text>
	    <xsl:value-of select="skit:dbquote(reftable/@refschema, 
			                       reftable/@reftable)"/>
	    <xsl:text>(</xsl:text>
	    <xsl:for-each select="reftable/column">
	      <xsl:if test="position() != '1'">
		<xsl:text>, </xsl:text>
	      </xsl:if>
	      <xsl:value-of select="skit:dbquote(@name)"/>
	    </xsl:for-each>
	    <xsl:text>)</xsl:text>
	    <xsl:choose>
	      <xsl:when test="@confmatchtype = 'f'">
		<xsl:text> match full</xsl:text>
	      </xsl:when>
	      <xsl:when test="@confmatchtype = 'p'">
		<xsl:text> match partial</xsl:text>
	      </xsl:when>
	      <xsl:when test="@confmatchtype = 'f'">
		<xsl:text> match simple</xsl:text>
	      </xsl:when>
	    </xsl:choose>
	    <xsl:call-template name="fk_event">
	      <xsl:with-param name="event" select="'update'"/>
	      <xsl:with-param name="action_code" select="@confupdtype"/>
	    </xsl:call-template>
	    <xsl:call-template name="fk_event">
	      <xsl:with-param name="event" select="'delete'"/>
	      <xsl:with-param name="action_code" select="@confdeltype"/>
	    </xsl:call-template>
	    <xsl:if test="@deferrable='t'">
	      <xsl:text>&#x0A;  deferrable</xsl:text>
	    </xsl:if>
	    <xsl:if test="@deferred='t'">
	      <xsl:text>&#x0A;  initially deferred</xsl:text>
	    </xsl:if>
	  </xsl:when>
	  <xsl:when test="(@type = 'unique') or (@type = 'primary key')">
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
	  </xsl:when>
	  <xsl:when test="@type = 'check'">
	    <xsl:text>&#x0A;  </xsl:text>
	    <xsl:value-of select="@source"/>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:text>OTHER SORT OF CONSTRAINT</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates/>  <!-- Deal with comments -->

	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:call-template name="set_owner"/>
	<xsl:text>&#x0A;alter table only </xsl:text>
        <xsl:value-of select="../@table_qname"/>
	<xsl:text>&#x0A;  drop constraint </xsl:text>
	<xsl:value-of select="skit:dbquote(@name)"/>
        <xsl:text>;&#x0A;</xsl:text>
	<xsl:call-template name="reset_owner"/>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>


