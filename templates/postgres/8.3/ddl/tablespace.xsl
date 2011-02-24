<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/tablespace">
    <xsl:if test="../@action='build'">
      <print>
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
	<xsl:apply-templates/>
	<xsl:if test="@location=''">
	  <xsl:text> */&#x0A;</xsl:text>
	</xsl:if>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;\echo Not dropping tablespace </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> as it may</xsl:text>
        <xsl:text> contain objects in other dbs;&#x0A;</xsl:text>
        <xsl:text>\echo To perform the drop uncomment the</xsl:text>
	<xsl:text>  following line:&#x0A;</xsl:text>
	<xsl:if test="@name='pg_default'">
	  <!-- If the tablespace is pg_default, try even harder not to
	       drop it! -->
          <xsl:text>-- </xsl:text>
	</xsl:if>
        <xsl:text>-- drop tablespace </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='diff'">
      <print>
	<xsl:for-each select="../diffs/attribute">
	  <xsl:if test="@name='owner'">
            <xsl:text>alter tablespace &quot;</xsl:text>
            <xsl:value-of select="../../@name"/>
            <xsl:text>&quot; owner to &quot;</xsl:text>
            <xsl:value-of select="@new"/>
            <xsl:text>&quot;;&#x0A;</xsl:text>
	  </xsl:if>
          <xsl:if test="@name='location'">
            <xsl:text>\echo WARNING tablespace &quot;</xsl:text>
            <xsl:value-of select="../../@name"/>
            <xsl:text>&quot; has moved from</xsl:text>
            <xsl:text> &apos;</xsl:text>
            <xsl:value-of select="@old"/>
            <xsl:text>&apos; to &apos;</xsl:text>
            <xsl:value-of select="@new"/>
            <xsl:text>&apos;;&#x0A;&#x0A;</xsl:text>
	  </xsl:if>
	</xsl:for-each>
      </print>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>

