<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject/role">

    <xsl:if test="../@action='build'">
      <print>
        <xsl:text>&#x0A;create role </xsl:text>
        <xsl:value-of select="../@qname"/>
	<xsl:choose>
	  <xsl:when test="@login = 'y'">
            <xsl:text> with login;&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
            <xsl:text> with nologin;&#x0A;</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="@password != ''">
          <xsl:text>alter role </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text> password &apos;</xsl:text>
          <xsl:value-of select="@password"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:if>
	<xsl:if test="@max_connections != '-1'">
          <xsl:text>alter role </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text> connection limit </xsl:text>
          <xsl:value-of select="@max_connections"/>
          <xsl:text>;&#x0A;</xsl:text>
	</xsl:if>
	<xsl:if test="@expires != ''">
          <xsl:text>alter role </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text> valid until &apos;</xsl:text>
          <xsl:value-of select="@expires"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:if>
	<xsl:if test="not(privilege/@priv='inherit')">
          <xsl:text>alter role </xsl:text>
          <xsl:value-of select="../@qname"/>
          <xsl:text> noinherit;&#x0A;</xsl:text>
	</xsl:if>
	<xsl:for-each select="config">
          <xsl:text>alter role </xsl:text>
          <xsl:value-of select="../../@qname"/>
          <xsl:text> set </xsl:text>
          <xsl:value-of select="@type"/>
          <xsl:text> = </xsl:text>
          <xsl:value-of select="@value"/>
          <xsl:text>;&#x0A;</xsl:text>
	</xsl:for-each>
	<xsl:for-each select="privilege">
	  <xsl:if test="@priv != 'inherit'">
            <xsl:text>alter role </xsl:text>
            <xsl:value-of select="../../@qname"/>
            <xsl:text> with </xsl:text>
            <xsl:value-of select="@priv"/>
            <xsl:text>;&#x0A;</xsl:text>
	  </xsl:if>
	</xsl:for-each>
	<xsl:for-each select="profile">
          <xsl:text>alter role </xsl:text>
          <xsl:value-of select="../../@qname"/>
          <xsl:text> set </xsl:text>
          <xsl:value-of select="@param"/>
          <xsl:text> = &apos;</xsl:text>
          <xsl:value-of select="@value"/>
          <xsl:text>&apos;;&#x0A;</xsl:text>
	</xsl:for-each>
	<xsl:apply-templates/>
        <xsl:text>&#x0A;</xsl:text>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
        <xsl:text>&#x0A;\echo Not dropping or revoking </xsl:text>
        <xsl:text>privs from role </xsl:text>
        <xsl:value-of select="../@name"/>
        <xsl:text> as it&#x0A;</xsl:text>
        <xsl:text>\echo may own objects in other databases&#x0A;</xsl:text>
        <xsl:text>\echo To perform the drop, uncomment </xsl:text>
        <xsl:text>the following lines:&#x0A;</xsl:text>
        <xsl:text>-- drop role </xsl:text>
        <xsl:value-of select="../@qname"/>
        <xsl:text>;&#x0A;</xsl:text>
      </print>     
    </xsl:if>

    <xsl:if test="../@action='diff'">
      <print>
        <xsl:text>&#x0A;</xsl:text>
	<xsl:for-each select="../diffs">
	  <xsl:for-each select="element/config">
	    <!-- This assumes config elements will always be diffs
		 TODO: Verify this.  -->
            <xsl:text>alter role </xsl:text>
            <xsl:value-of select="../../../@qname"/>
            <xsl:text> set </xsl:text>
            <xsl:value-of select="@type"/>
            <xsl:text> = </xsl:text>
            <xsl:value-of select="@value"/>
            <xsl:text>;&#x0A;</xsl:text>
	  </xsl:for-each>
	  <xsl:for-each select="attribute">
            <xsl:text>alter role </xsl:text>
            <xsl:value-of select="../../@qname"/>
	    <xsl:choose>
	      <xsl:when test="@name = 'expires'">
		<xsl:text> valid until &apos;</xsl:text>
		<xsl:value-of select="@new"/>
		<xsl:text>&apos;;&#x0A;</xsl:text>
	      </xsl:when>
	    </xsl:choose>
	  </xsl:for-each>
	  <xsl:for-each select="element/privilege">
	    <!-- Privilege elements will always be either new or gone. -->
            <xsl:text>alter role </xsl:text>
            <xsl:value-of select="../../../@qname"/>
            <xsl:text> with </xsl:text>
	    <xsl:if test="../@status='Gone'">
              <xsl:text>no</xsl:text>
	    </xsl:if>
            <xsl:value-of select="@priv"/>
            <xsl:text>;&#x0A;</xsl:text>
	  </xsl:for-each>

	  <xsl:apply-templates/>
	</xsl:for-each>
        <xsl:text>&#x0A;</xsl:text>
      </print>     
    </xsl:if>

  </xsl:template>

</xsl:stylesheet>

