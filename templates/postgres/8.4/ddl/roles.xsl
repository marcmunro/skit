<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <xsl:template match="dbobject[@type='role']/role">
    <xsl:if test="../@action='build'">
      <print>
	<xsl:call-template name="feedback"/>
        <xsl:value-of 
	    select="concat('create role ', ../@qname)"/>
	<xsl:choose>
	  <xsl:when test="@login = 'y'">
            <xsl:text> with login;&#x0A;</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
            <xsl:text> with nologin;&#x0A;</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="@password != ''">
          <xsl:text></xsl:text>
          <xsl:value-of 
	      select="concat('alter role ', ../@qname,
		             ' password ', $apos,
			     @password, $apos, ';&#x0A;')"/>
	</xsl:if>
	<xsl:if test="@max_connections != '-1'">
          <xsl:value-of 
	      select="concat('alter role ', ../@qname,
		             ' connection limit ', @max_connections,
			     ';&#x0A;')"/>
	</xsl:if>
	<xsl:if test="@expires != ''">
          <xsl:text></xsl:text>
          <xsl:value-of 
	      select="concat('alter role ', ../@qname,
		             ' valid until ', $apos,
			     @expires, $apos, ';&#x0A;')"/>
	</xsl:if>

	<xsl:if test="not(privilege/@priv='inherit')">
	  <xsl:value-of 
	      select="concat('alter role ', ../@qname,
		      ' with noinherit;&#x0A;')"/>
	</xsl:if>	

	<xsl:for-each select="privilege">
	  <xsl:text></xsl:text>
	  <xsl:value-of 
	      select="concat('alter role ', ../../@qname,
		      ' with ', @priv, ';&#x0A;')"/>
	</xsl:for-each>

	<xsl:for-each select="config">
          <xsl:text></xsl:text>
          <xsl:value-of 
	      select="concat('alter role ', ../../@qname,
		             ' set ', @type, ' = ', @value, ';&#x0A;')"/>
	</xsl:for-each>

	<xsl:for-each select="profile">
          <xsl:value-of 
	      select="concat('alter role ', ../../@qname,
		             ' set ', @param, ' = ', $apos,
			     @value, $apos, ';&#x0A;')"/>
	</xsl:for-each>
	<xsl:apply-templates/>
      </print>
    </xsl:if>

    <xsl:if test="../@action='drop'">
      <print>
	<xsl:call-template name="feedback"/>
        <xsl:value-of 
	    select="concat('\echo Not dropping role ', ../@name,
			   ' as it&#x0A;',
			   '\echo may own objects in other databases&#x0A;',
			   '\echo To perform the drop, uncomment ',
			   'the following line:&#x0A;',
			   '-- drop role ', ../@qname, ';&#x0A;')"/>
      </print> 
    </xsl:if>

    <xsl:if test="../@action='diffcomplete'">
      <print>
	<xsl:call-template name="feedback"/>

	<xsl:for-each select="../element/config">
	  <xsl:value-of 
	      select="concat('alter role ', ../../@qname,
		             ' set ', @type, ' = ', @value, ';&#x0A;')"/>

	</xsl:for-each>
	<xsl:for-each select="../element/privilege">
	  <xsl:choose>
	    <xsl:when test="../@status='gone'">
	      <xsl:value-of 
		  select="concat('alter role ', ../../@qname,
			         ' with no', @priv, ';&#x0A;')"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of 
		  select="concat('alter role ', ../../@qname,
			         ' with ', @priv, ';&#x0A;')"/>
	    </xsl:otherwise>
	  </xsl:choose>
	</xsl:for-each>

	<xsl:for-each select="../attribute">
	  <xsl:value-of 
	      select="concat('alter role ', ../@qname)"/>
	  <xsl:choose>
	    <xsl:when test="@name = 'expires'">
	      <xsl:value-of 
		  select="concat(' valid until ', $apos, @new, 
			         $apos, ';&#x0A;')"/>
	    </xsl:when>
	  </xsl:choose>
	</xsl:for-each>

	<xsl:call-template name="commentdiff"/>
      </print>
    </xsl:if>
  </xsl:template>

</xsl:stylesheet>

