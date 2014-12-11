<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skit="http://www.bloodnok.com/xml/skit"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xi="http://www.w3.org/2003/XInclude"
  extension-element-prefixes="skit"
  version="1.0">

  <!-- These keys are used for grouping like-named objects together.
       They allow us to identify, for a given element, whether it
       is the first such named element or not.
    -->
  <xsl:key name="functions" match="dbobject[@type='function']" 
	   use="concat(function/@schema, '.', function/@name)"/>

  <xsl:key name="aggregates" match="dbobject[@type='aggregate']" 
	   use="concat(aggregate/@schema, '.', aggregate/@name)"/>

  <xsl:key name="operators" match="dbobject[@type='operator']" 
	   use="concat(operator/@schema, '.', operator/@name)"/>

  <xsl:template match="*" mode="fullcopy">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="fullcopy"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="/*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
      <xsl:for-each select="//dbobject">
	<xsl:choose>
	  <xsl:when test="function">
	    <xsl:if test="generate-id(.) = 
			  generate-id(key('functions', 
			                  concat(function/@schema, '.',
		                                 function/@name))[1])">
	      <xsl:call-template name="groupobject">
		<xsl:with-param name="object_type" select="'function'"/>
		<xsl:with-param name="object_type_plural" 
				select="'functions'"/>
	      </xsl:call-template>
	    </xsl:if>
	  </xsl:when>
	  <xsl:when test="aggregate">
	    <xsl:if test="generate-id(.) = 
			  generate-id(key('aggregates', 
			                  concat(aggregate/@schema, '.',
		                                 aggregate/@name))[1])">
	      <xsl:call-template name="groupobject">
		<xsl:with-param name="object_type" select="'aggregate'"/>
		<xsl:with-param name="object_type_plural"
				select="'aggregates'"/>
	      </xsl:call-template>
	    </xsl:if>
	  </xsl:when>
	  <xsl:when test="operator">
	    <xsl:if test="generate-id(.) = 
			  generate-id(key('operators', 
			                  concat(operator/@schema, '.',
		                                 operator/@name))[1])">
	      <xsl:call-template name="groupobject">
		<xsl:with-param name="object_type" select="'operator'"/>
		<xsl:with-param name="object_type_plural" select="'operators'"/>
	      </xsl:call-template>
	    </xsl:if>
	  </xsl:when>
	  <xsl:otherwise>
	    <xsl:call-template name="dbobject"/>
	  </xsl:otherwise>
	</xsl:choose>
      </xsl:for-each>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="/*/params">
    <!-- Copy the outermost params object.
      -->
    <xsl:copy>
      <xsl:copy-of select="@*"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template name="groupobject">
    <xsl:param name="object_type"/>
    <xsl:param name="object_type_plural"/>
    <!-- Template for dealing with grouped objects.  This is used for
	 functions, aggregates and operators, and allows all objects 
	 with the same schema and name to be considered to be a single
	 database object for the purpose of scatter.  This reduces the
	 number of files, allows us to keep filenames simple, and groups
	 related objects into a single file.
      -->
    <xsl:variable name="groupname" select="*[name()=$object_type]/@name"/>
    <xsl:variable name="groupschema" select="*[name()=$object_type]/@schema"/>
    <xsl:element name="skit:scatter">
      <xsl:attribute name="path">
	<xsl:value-of select="concat('cluster/databases/', 
			      ../../../@name, '/schemata/', 
			      ../../@name, '/', $object_type_plural, '/')"/>
      </xsl:attribute>
      <xsl:attribute name="name">
	<xsl:value-of select="concat(@name, '.xml')"/>
      </xsl:attribute>
      <xsl:variable name="here" select="."/>

      <xsl:for-each select="/*">
	<!-- Create a copy of the root element for each dbobject -->
	<xsl:copy>
	  <xsl:copy-of select="@*"/>
	  <!-- Copy each object -->
	  <xsl:for-each select="//dbobject[@type=$object_type and 
				*[name()=$object_type]/@name = $groupname and
				*[name()=$object_type]/@schema = $groupschema]">
	    <xsl:apply-templates mode="copy_topobject"/>
	  </xsl:for-each>
	</xsl:copy>
      </xsl:for-each>
    </xsl:element>
  </xsl:template>

  <xsl:template name="make-plural">
    <xsl:choose>
      <xsl:when test="@type='schema'">
	<xsl:text>schemata</xsl:text>
      </xsl:when>
      <xsl:when test="@type='index'">
	<xsl:text>indices</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="concat(@type,'s')"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="getpath">
    <xsl:variable name="depth"
		  select="string-length(@fqn) - 
			  string-length(translate(@fqn,'.',''))"/>
    <xsl:if test="$depth = 0">
      <xsl:text>cluster</xsl:text>
    </xsl:if>
    <xsl:if test="$depth > 0">
      <xsl:for-each select="../..">
	<xsl:call-template name="getpath"/>
	<xsl:text>/</xsl:text>
      </xsl:for-each>
      <xsl:call-template name="make-plural"/>
      <xsl:value-of select="concat('/',@name)"/>
    </xsl:if>
  </xsl:template>

  <xsl:template name="dbobject">
    <!-- Replace the dbobject element with a skit:scatter element.  This
	 provides a directive to skit to write a single file for the
	 given database object.  
      -->
    <xsl:if test="@type!='comment' and @type!='privilege' and
		  @type!='column' and @type!='dump' and
		  @type!='fallback'">
      <!-- The only comments which are dbobjects are for operator
	   families (for reasons of dependency handling).  As those
	   comments also appear in the operator family definition,
	   there is no need to process such elements. 
	   Privileges are ignored as they are recorded in their
	   containing roles.
	   Missing types are explicitly ignored.
	   Columns are dealt with in their containing objects.
	   Fallbacks have no business being here.
      -->
      <xsl:element name="skit:scatter">
	<xsl:attribute name="path">
	  <xsl:variable name="fullpath">
	    <xsl:call-template name="getpath"/>
	  </xsl:variable>
	  <xsl:value-of select="substring($fullpath, 1,
	                                  string-length($fullpath) -
					  string-length(@name))"/>
	</xsl:attribute>

	<xsl:attribute name="name">
	  <xsl:choose>
	    <xsl:when test="@type='cluster'">
	      <xsl:value-of select="'cluster.xml'"/>
	    </xsl:when>	
	    <xsl:otherwise>
	      <xsl:value-of select="concat(@name, '.xml')"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:attribute>
	
	<xsl:variable name="here" select="."/>
	<xsl:for-each select="/*">
	  <!-- Create a copy of the root element for each dbobject -->
	  <xsl:copy>
	    <xsl:copy-of select="@*"/>
	    
	    <!-- And copy the contents of the dbobject into the root copy -->
	    <xsl:for-each select="$here">
	      <xsl:apply-templates mode="copy_topobject"/>
	    </xsl:for-each>
	  </xsl:copy>
	</xsl:for-each>
      </xsl:element>
    </xsl:if>
  </xsl:template>

  <xsl:template match="dbobject">
    <!-- This prevents dbobjects being copied by apply-templates from
	 the root element.
      -->
  </xsl:template>

  <xsl:template match="dbobject" mode="copy_dbobject">
    <!-- This prevents dbobjects being copied recursively, thereby
	 allowing the dboject tree to be flattened by the for-each 
	 call in the root element.
      -->
  </xsl:template>

  <xsl:template match="dbobject/dependencies" mode="copy_dbobject">
    <!-- This prevents dbobject dependency information being copied.
	 After all, it is not very interesting.
      -->
  </xsl:template>

  <xsl:template match="*" mode="copy_topobject">
    <!-- This copies the contents of the top element in a dbobject,
	 calls another template to copy its contents, and adds a 
	 skit:gather element.
      -->
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy_dbobject"/>
      <xsl:element name="skit:gather"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="*" mode="copy_dbobject">
    <!-- This recursively copies the contents of a dbobject element.
      -->
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates mode="copy_dbobject"/>
    </xsl:copy>
  </xsl:template>
</xsl:stylesheet>

