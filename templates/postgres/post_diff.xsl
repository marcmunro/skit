<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">

  <!-- This stylesheet performs post-processing of diffs.  It is needed
       because during ddl operations the object hierarchy will have
       been lost, and sometimes objects need to know about related
       objects.  This is particularly true for columns within tables.  -->
  <xsl:template match="*">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <!-- Provide an inherits-from element describing what has happened to a
       column which we are/were inhertiing.  
       The context for this template is an inherited-column element, 
       which may be an original, or from a diff element. 
  -->
  <xsl:template name="inheritance-summary">
    <xsl:variable name="col-name" select="@name"/>
    <xsl:variable name='col-schema' select="@schemaname"/>
    <xsl:variable name='col-table' select="@tablename"/>

    <inherits-from table="{@tablename}" schema="{@schemaname}"
		   column="{@name}">
      <!-- Figure out whether we are dropping or adding this inheritence -->
      <xsl:if test="ancestor::dbobject[@type='table']/
		    element[@type='inherits' and @status='gone']/
		    inherits[@name=$col-table and @schema=$col-schema]">
	<xsl:attribute name="disinherit-table">
	  <xsl:text>t</xsl:text>
	</xsl:attribute>
      </xsl:if>
      <xsl:if test="ancestor::dbobject[@type='table']/
		    element[@type='inherits' and @status='new']/
		    inherits[@name=$col-table and @schema=$col-schema]">
	<xsl:attribute name="eninherit-table">
	  <xsl:text>t</xsl:text>
	</xsl:attribute>
      </xsl:if>

      <!-- Figure out the column diff status -->
      <xsl:for-each 
	  select="ancestor::database/
		  dbobject[@type='schema' and @name=$col-schema]/schema/
		  dbobject[@type='table' and @name=$col-table]/table/
		  dbobject[@type='column' and @name=$col-name]">
	<xsl:attribute name="col-diff">
	  <xsl:choose>
	    <xsl:when test="@diff='gone' and ../../@diff='gone'">
	      <!-- The column has not been dropped from the table, so
	            let's pretend the diff is really 'none'.  -->
	      <xsl:text>none</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="@diff"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:attribute>
      </xsl:for-each>
    </inherits-from>
  </xsl:template>


  <xsl:template match="table/dbobject[column]">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:attribute name="parent-type">
	<xsl:value-of select="'table'"/>
      </xsl:attribute>
      <xsl:attribute name="parent-qname">
	<xsl:value-of select="../../@qname"/>
      </xsl:attribute>
      <xsl:attribute name="parent-diff">
	<xsl:value-of select="../../@diff"/>
      </xsl:attribute>

      <xsl:if test="attribute[@name='is_local'] or column[@is_local='f']">
	<xsl:variable name="col-name" select="@name"/>
	<xsl:for-each select="../inherits/inherited-column[@name=$col-name]">
	  <xsl:call-template name="inheritance-summary"/>
	</xsl:for-each>

	<!-- This gets disinherited tables.  -->
	<xsl:for-each 
	    select="../../element[@status='gone' and @type='inherits']/
		    inherits/inherited-column[@name=$col-name]">
	  <xsl:call-template name="inheritance-summary"/>
	</xsl:for-each>

	<!-- This gets dropped inheritance columns.  -->
	<xsl:for-each 
	    select="../../element[@type='inherits']/
		    element[@status = 'gone' and @type='inherited-column']/
		    inherited-column[@name=$col-name]">
	  <xsl:call-template name="inheritance-summary"/>
	</xsl:for-each>
      </xsl:if>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>


