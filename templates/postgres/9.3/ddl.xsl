<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <xsl:include href="skitfile:common_defs.xsl"/>
  <xsl:variable name="quoted-username"
		select="skit:eval('(username)')"/>
  <xsl:variable name="username"
		select="substring($quoted-username, 2,
			string-length($quoted-username) - 2)"/>

  <!-- Anything not matched explicitly will match this and be copied 
       This handles dbobject, dependencies, etc -->
  <xsl:template match="*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <!-- Don't do anything with text nodes matched by calls to
       apply-templates - text nodes must be copied explicitly as and
       when needed.  -->
  <xsl:template match="text()"/>
  <xsl:template match="text()" mode="diff"/>
  <xsl:template match="text()" mode="diffprep"/>
  <xsl:template match="text()" mode="build"/>
  <xsl:template match="text()" mode="drop"/>

  <!-- Templates for getting object signatures preceded by the object
       type name.  -->
  <xsl:template name="dbobject-typename">
    <xsl:param name="typename"/>
    <xsl:choose>
      <xsl:when test="$typename = 'user_mapping'">
	<xsl:text>user mapping</xsl:text>
      </xsl:when>
      <xsl:when test="$typename = 'foreign_server'">
	<xsl:text>foreign server</xsl:text>
      </xsl:when>
      <xsl:when test="$typename = 'foreign_data_wrapper'">
	<xsl:text>foreign data wrapper</xsl:text>
      </xsl:when>
      <xsl:when test="$typename = 'text_search_dictionary'">
	<xsl:text>text search dictionary</xsl:text>
      </xsl:when>
      <xsl:when test="$typename = 'text_search_template'">
	<xsl:text>text search template</xsl:text>
      </xsl:when>
      <xsl:when test="$typename = 'text_search_parser'">
	<xsl:text>text search parser</xsl:text>
      </xsl:when>
      <xsl:when test="$typename = 'text_search_configuration'">
	<xsl:text>text search configuration</xsl:text>
      </xsl:when>
      <xsl:when test="contains($typename, '_')">
	<xsl:value-of 
	    select="concat(substring-before($typename, '_'),
		    ' ', substring-after($typename, '_'))" />
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="$typename"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="obj-signature">
    <xsl:param name="objnode"/>
    <xsl:call-template name="dbobject-typename">
      <xsl:with-param name="typename">
	<xsl:if test="(name($objnode) = 'table') and 
                      ($objnode/@is_foreign = 't')">
	  <xsl:text>foreign </xsl:text>
	</xsl:if>
	<xsl:value-of select="name($objnode)"/>
      </xsl:with-param>
    </xsl:call-template>
    <xsl:text> </xsl:text>
    <xsl:choose>
      <xsl:when test="(name($objnode) = 'constraint') or 
                      (name($objnode) = 'trigger')">
	<xsl:value-of 
	    select="concat(skit:dbquote($objnode/@name), ' on ',
		           $objnode/../@table_qname)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="$objnode/../@qname"/>
	<xsl:if test="$objnode/@method">
	  <xsl:value-of select="concat(' using ', $objnode/@method)"/>
	</xsl:if>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>


  <!-- Template for dealing with comments.  This is invoked simply by
       using xsl:apply-templates from within the template for the 
       current dbobject -->
  <xsl:template name="comment">
    <xsl:param name="objnode"/>
    <xsl:param name="text"/>
    <xsl:param name="sig" select="''"/>
    <xsl:text>&#x0A;comment on </xsl:text>
    <xsl:choose>
      <xsl:when test="$sig=''">
	<xsl:call-template name="obj-signature">
	  <xsl:with-param name="objnode" select="$objnode"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="$sig"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:value-of select="concat(' is ', $text, ';&#x0A;')"/>
  </xsl:template>

  <xsl:template name="commentdiff">
    <xsl:param name="sig" select="''"/>
    <xsl:for-each select="../element[@type='comment']">
      <!-- If comments exist, this conditional print section must be
           enabled.  -->
      <do-print/>  
      <xsl:call-template name="comment">
	<xsl:with-param name="objnode" select="../*[name() = ../@type]"/>
	<xsl:with-param name="sig" select="$sig"/>
	<xsl:with-param name="text">
	  <xsl:choose>
	    <xsl:when test="@status = 'gone'">
	      <xsl:value-of select="'null'"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="concat('&#x0A;', comment/text())"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:with-param>
      </xsl:call-template>
    </xsl:for-each>
  </xsl:template>

  <!-- Handle comments as found directly in a dbobject definition.  -->
  <xsl:template match="dbobject/*[name(.)!='element']/comment">
    <xsl:call-template name="comment">
      <xsl:with-param name="objnode" select=".."/>
      <xsl:with-param name="text" select="concat('&#x0A;', ./text())"/>
    </xsl:call-template>
  </xsl:template>


  <xsl:template name="set_owner">
    <!-- Explicit set session authorization created when ignore-contexts
        is true, for those cases where we must always do this in spite of
        the ignore-contexts flag.  -->
    <xsl:if test="skit:eval('ignore-contexts') = 't'">
      <xsl:if test="@owner != //cluster/@username">
	<xsl:value-of 
	    select="concat('&#x0A;set session authorization ', $apos, 
		           @owner, $apos, ';&#x0A;')"/>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template name="reset_owner">
    <xsl:if test="skit:eval('ignore-contexts') = 't'">
      <xsl:if test="@owner != //cluster/@username">
	<xsl:text>reset session authorization;&#x0A;</xsl:text>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template name="set_owner_from">
    <xsl:if test="skit:eval('ignore-contexts') = 't'">
      <xsl:if test="@from != //cluster/@username">
	<xsl:value-of 
	    select="concat('&#x0A;set session authorization ', $apos, 
		           @from, $apos, ';&#x0A;')"/>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template name="reset_owner_from">
    <xsl:if test="skit:eval('ignore-contexts') = 't'">
      <xsl:if test="@from != //cluster/@username">
	<xsl:text>reset session authorization;&#x0A;</xsl:text>
      </xsl:if>
    </xsl:if>
  </xsl:template>

  <xsl:template name="dbobject">
    <xsl:element name="print">
      <!-- diffprep and diff actions may not actually result in
           code being created.  Such printable sections are therefore
	   made conditional.  To enable printing of such conditional
	   print nodes, a <do-print/> element should be added.  This
	   conditionality is handled by navigation.xsl -->
      <xsl:if test="(../@action = 'diff') or 
		    (../@action = 'diffprep' or
		    @extension)">
	<xsl:attribute name="conditional">yes</xsl:attribute>
      </xsl:if>
      <xsl:call-template name="feedback"/>
      <xsl:call-template name="set_owner"/>

      <xsl:choose>
	<xsl:when test="@extension">
	  <!-- For now, we will do nothing for objects which are part of
	       an extension.  -->
	</xsl:when>
	<xsl:when test="../@action='build'">
	  <xsl:apply-templates select="." mode="build"/>
	  <xsl:apply-templates/>   <!-- Deal with comments -->
	</xsl:when>
	<xsl:when test="../@action='drop'">
	  <xsl:apply-templates select="." mode="drop"/>
	</xsl:when>
	<xsl:when test="../@action='diffprep'">
	  <xsl:if test="not(@extension)">
	    <xsl:apply-templates select="." mode="diffprep"/>
	  </xsl:if>
	</xsl:when>
	<xsl:when test="../@action='diff'">
	  <xsl:if test="not(@extension)">
	    <xsl:apply-templates select="." mode="diff"/>
	  </xsl:if>
	  <xsl:call-template name="commentdiff"/>
	</xsl:when>
      </xsl:choose>

      <xsl:call-template name="reset_owner"/>
    </xsl:element>
  </xsl:template>

  <!-- This is the default dbobject handling template.  Any explicitly
       matched template (eg for match="dbobject/cluster" will have a
       higher priority and so take precedence.  Hence this template only
       applies when no other has been specified.  -->
  <xsl:template match="dbobject/*[name(.)=../@type and
		                  ../@action != 'exists']" priority="-0.5">
    <xsl:call-template name="dbobject"/>
  </xsl:template>

  <!-- Special case for viewbase dbobject -->
  <xsl:template 
      match="dbobject[@type='viewbase' and @action != 'exists']/view"
      priority="-0.5">
    <xsl:call-template name="dbobject"/>
  </xsl:template>

  <!-- The following objects must not be handled by the template
       above, and do not elsewhere have their own explicit templates.  -->
  <xsl:template match="dbobject/privilege"/>
  <xsl:template match="dbobject/column"/>

  <xsl:include href="skitfile:ddl/feedback.xsl"/>

  <xsl:include href="skitfile:ddl/cluster.xsl"/>
  <xsl:include href="skitfile:ddl/database.xsl"/>
  <xsl:include href="skitfile:ddl/tablespace.xsl"/>
  <xsl:include href="skitfile:ddl/roles.xsl"/>
  <xsl:include href="skitfile:ddl/grants.xsl"/>
  <xsl:include href="skitfile:ddl/languages.xsl"/>
  <xsl:include href="skitfile:ddl/schemata.xsl"/>
  <xsl:include href="skitfile:ddl/domains.xsl"/>
  <xsl:include href="skitfile:ddl/types.xsl"/>
  <xsl:include href="skitfile:ddl/functions.xsl"/>
  <xsl:include href="skitfile:ddl/aggregates.xsl"/>
  <xsl:include href="skitfile:ddl/casts.xsl"/>
  <xsl:include href="skitfile:ddl/operators.xsl"/>
  <xsl:include href="skitfile:ddl/operator_classes.xsl"/>
  <xsl:include href="skitfile:ddl/operator_families.xsl"/>
  <xsl:include href="skitfile:ddl/comments.xsl"/>
  <xsl:include href="skitfile:ddl/sequences.xsl"/>
  <xsl:include href="skitfile:ddl/tables.xsl"/>
  <xsl:include href="skitfile:ddl/columns.xsl"/>
  <xsl:include href="skitfile:ddl/constraints.xsl"/>
  <xsl:include href="skitfile:ddl/indices.xsl"/>
  <xsl:include href="skitfile:ddl/triggers.xsl"/>
  <xsl:include href="skitfile:ddl/rules.xsl"/>
  <xsl:include href="skitfile:ddl/views.xsl"/>
  <xsl:include href="skitfile:ddl/matviews.xsl"/>
  <xsl:include href="skitfile:ddl/conversions.xsl"/>
  <xsl:include href="skitfile:ddl/tsconfigs.xsl"/>
  <xsl:include href="skitfile:ddl/tsconfig_mappings.xsl"/>
  <xsl:include href="skitfile:ddl/ts_templates.xsl"/>
  <xsl:include href="skitfile:ddl/ts_dicts.xsl"/>
  <xsl:include href="skitfile:ddl/ts_parsers.xsl"/>
  <xsl:include href="skitfile:ddl/fdws.xsl"/>
  <xsl:include href="skitfile:ddl/foreign_servers.xsl"/>
  <xsl:include href="skitfile:ddl/user_mappings.xsl"/>
  <xsl:include href="skitfile:ddl/extensions.xsl"/>
  <xsl:include href="skitfile:ddl/collations.xsl"/>
  <xsl:include href="skitfile:ddl/event_triggers.xsl"/>
  <xsl:include href="skitfile:ddl/fallback.xsl"/>
</xsl:stylesheet>
