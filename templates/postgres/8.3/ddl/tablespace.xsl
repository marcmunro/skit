<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/tablespace">
    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>---- DBOBJECT </xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:if test="skit:eval('echoes') = 't'">
          <xsl:text>\echo tablespace </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
	<xsl:if test="@location=''">
	  <xsl:text>/* This is a default tablespace.  We </xsl:text>
	  <xsl:text>cannot and should not attempt</xsl:text>
	  <xsl:text>&#x0A;   to create it.&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>create tablespace </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text> owner </xsl:text>
        <xsl:value-of select="skit:dbquote(@owner)"/>
        <xsl:text>&#x0A;  location &apos;</xsl:text>
        <xsl:value-of select="@location"/>
        <xsl:text>&apos;;&#x0A;</xsl:text>

	<xsl:if test="@location=''">
	  <xsl:text> */&#x0A;</xsl:text>
	</xsl:if>

	<xsl:apply-templates/>

      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>---- DBOBJECT </xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
        <xsl:text>&#x0A;\echo Not dropping tablespace </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> as it may</xsl:text>
        <xsl:text> contain objects in other dbs;&#x0A;</xsl:text>
        <xsl:text>\echo To perform the drop uncomment the</xsl:text>
	<xsl:text> following line:&#x0A;</xsl:text>
	<xsl:if test="@name='pg_default'">
	  <!-- If the tablespace is pg_default, try even harder not to
	       drop it! -->
          <xsl:text>-- </xsl:text>
	</xsl:if>
        <xsl:text>-- drop tablespace </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
        <xsl:text>---- DBOBJECT</xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:for-each select="../attribute">
	  <xsl:if test="@name='owner'">
            <xsl:text>&#x0A;alter tablespace </xsl:text>
            <xsl:value-of select="../@qname"/>
            <xsl:text> owner to </xsl:text>
            <xsl:value-of select="skit:dbquote(@new)"/>
            <xsl:text>;&#x0A;</xsl:text>
	  </xsl:if>
          <xsl:if test="@name='location'">
            <xsl:text>&#x0A;\echo WARNING tablespace &quot;</xsl:text>
            <xsl:value-of select="../@name"/>
            <xsl:text>&quot; has moved from</xsl:text>
            <xsl:text> &apos;</xsl:text>
            <xsl:value-of select="@old"/>
            <xsl:text>&apos; to &apos;</xsl:text>
            <xsl:value-of select="@new"/>
            <xsl:text>&apos;;&#x0A;</xsl:text>
	  </xsl:if>
	</xsl:for-each>

	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diffprep'">
      <print>
        <xsl:text>---- DBOBJECT </xsl:text> <!-- QQQ -->
	<xsl:value-of select="../@fqn"/>
        <xsl:text>&#x0A;</xsl:text>
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
	      
	      <xsl:text>&#x0A;---- Changing ownership from role </xsl:text>
	      <xsl:text>to be dropped...&#x0A;alter tablespace </xsl:text>
	      <xsl:value-of select="../@qname"/>
	      <xsl:text> owner to :USER;&#x0A;</xsl:text>
	    </xsl:if>
	  </xsl:if>
	</xsl:for-each>
      </print>
    </xsl:if>

  </xsl:template>
</xsl:stylesheet>

