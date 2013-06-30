<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/tablespace">
    <xsl:if test="../@action='build'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 

	<xsl:call-template name="feedback"/>
        <xsl:text>&#x0A;</xsl:text>

	<xsl:if test="@location=''">
	  <xsl:text>/* This is a default tablespace.  We </xsl:text>
	  <xsl:text>cannot and should not attempt</xsl:text>
	  <xsl:text>&#x0A;   to create it.&#x0A;</xsl:text>
	</xsl:if>
        <xsl:value-of 
	    select="concat('create tablespace ', ../@qname,
		           ' owner ', skit:dbquote(@owner),
			   '&#x0A;  location ', $apos,
			   @location, $apos, ';&#x0A;')"/>
	<xsl:if test="@location=''">
	  <xsl:text> */&#x0A;</xsl:text>
	</xsl:if>

	<xsl:apply-templates/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT ', ../@fqn, '&#x0A;')"/> 

	<xsl:call-template name="feedback"/>
        <xsl:value-of 
	    select="concat('&#x0A;\echo Not dropping tablespace ', ../@name,
		           ' as it may contain objects in other dbs;&#x0A;',
			   '\echo To perform the drop uncomment the',
			   ' following line:&#x0A;')"/>
	<xsl:if test="@name='pg_default'">
	  <!-- If the tablespace is pg_default, try even harder not to
	       drop it! -->
          <xsl:text>-- </xsl:text>
	</xsl:if>
        <xsl:value-of 
	    select="concat('-- drop tablespace ', ../@qname, ';&#x0A;')"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<!-- QQQ -->
	<xsl:value-of 
	    select="concat('---- DBOBJECT DIFF ', ../@fqn, '&#x0A;')"/> 

	<xsl:call-template name="feedback"/>
	<xsl:text>&#x0A;</xsl:text>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:value-of 
		select="concat('alter tablespace ', ../@qname,
			       ' owner to ', skit:dbquote(@new),
			       ';&#x0A;')"/>
	  </xsl:if>
          <xsl:if test="@name='location'">
            <xsl:value-of 
		select="concat('\echo WARNING tablespace &quot;', ../@name,
			       '&quot; has moved from ', $apos,
			       @old, $apos, ' to ', $apos, @new,
			       $apos, ';'&#x0A;)"/>
	  </xsl:if>
	</xsl:for-each>

	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <print>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
	    <xsl:variable name="old_fqn" 
			  select="concat('role.cluster.', @old)"/>
	    <xsl:if test="//dbobject[@fqn=$old_fqn and @action='drop']">
	      <!-- If the old dependency is on a role that is being
		   dropped, we must first reassign the ownership to a
		   role that we know will exist throughout this
		   transction.  We could make this even smarter and 
	           determine whether the drop will happen before our
	           corresponding diffcomplete but that seems like 
	           overkill.  -->
	      
	      <!-- QQQ -->
	      <xsl:value-of 
		  select="concat('---- DBOBJECT PREP ', ../@fqn, '&#x0A;')"/> 

	      <xsl:call-template name="feedback"/>
	      <xsl:value-of 
		  select="concat('&#x0A;---- Changing ownership from role ',
			         'to be dropped...&#x0A;alter tablespace ', 
			         ../@qname, ' owner to :USER;&#x0A;')"/>
	    </xsl:if>
	  </xsl:if>
	</xsl:for-each>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

