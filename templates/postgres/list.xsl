<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet 
  xmlns:skituls="http://www.bloodnok.com/xml/skituls"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:xs="http://www.w3.org/2001/XMLSchema"
  version="1.0">

  <xsl:template match="/">
    <skituls:stylesheet>
      <xsl:copy select=".">
	<xsl:copy-of select="@*"/>
	<xsl:apply-templates>
	  <xsl:with-param name="depth" select="1"/>
	</xsl:apply-templates>
      </xsl:copy>
    </skituls:stylesheet>
  </xsl:template>

  <xsl:template name="printValue">
    <xsl:param name="name"/>
    <xsl:param name="value"/>
    <xsl:param name="prefix"/>
    <xsl:param name="depth"/>
    <xsl:value-of 
       select="substring('                            ', 1, $depth*2)"/>
    <xsl:value-of select="concat($prefix, $name)"/>
    <xsl:if test="$value">
      <xsl:value-of select="'=&quot;'"/>
    </xsl:if>
    <xsl:choose>
      <xsl:when test="string-length($value) > 60">
	<xsl:value-of select="concat(substring(translate($value,
			      '&#xA;', ' '), 1, 60), '...')"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="translate($value, '&#xA;', ' ')"/>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:if test="$value">
      <xsl:text>&quot;</xsl:text>
    </xsl:if>
    <xsl:text>&#xA;</xsl:text>
  </xsl:template>

  <xsl:template match="diffs">
    <xsl:param name="depth"/>
    <xsl:if test="//list[@fulldetails='true']">
    <dbobject generate="diff">
      <diff>
	<xsl:attribute name="code">
	  <xsl:for-each select="element">
	    <xsl:choose>
	      <xsl:when test="@status='Diff'">
		<xsl:for-each select="diffs/attribute">
		  <xsl:if test="@old">
		    <xsl:call-template name="printValue">
		      <xsl:with-param name="value" select="@old"/>
		      <xsl:with-param name="name" 
				      select="concat(../../@name, ',', @name)"/>
		      <xsl:with-param name="depth" select="$depth"/>
		      <xsl:with-param name="prefix" select="'&lt;&lt; '"/>
		    </xsl:call-template>
		  </xsl:if>
		  <xsl:if test="@new">
		    <xsl:call-template name="printValue">
		      <xsl:with-param name="value" select="@new"/>
		      <xsl:with-param name="name" 
				      select="concat(../../@name, ',', @name)"/>
		      <xsl:with-param name="depth" select="$depth"/>
		      <xsl:with-param name="prefix" select="'&gt;&gt; '"/>
		    </xsl:call-template>
		  </xsl:if>
		</xsl:for-each>
	      </xsl:when>
	      <xsl:when test="@status='Gone'">
		<xsl:call-template name="printValue">
		  <xsl:with-param name="name" 
				  select="concat(@name, ' (', @type, ')')"/>
		  <xsl:with-param name="depth" select="$depth"/>
		  <xsl:with-param name="prefix" select="'-- '"/>
		</xsl:call-template>
	      </xsl:when>
	      <xsl:when test="@status='New'">
		<xsl:call-template name="printValue">
		  <xsl:with-param name="name" 
				  select="concat(@name, ' (', @type, ')')"/>
		  <xsl:with-param name="depth" select="$depth"/>
		  <xsl:with-param name="prefix" select="'++ '"/>
		</xsl:call-template>
	      </xsl:when>
	    </xsl:choose>
	  </xsl:for-each>
	  <xsl:for-each select="attribute">
	    <xsl:if test="@old">
	      <xsl:call-template name="printValue">
		<xsl:with-param name="value" select="@old"/>
		<xsl:with-param name="name" select="@name"/>
		<xsl:with-param name="depth" select="$depth"/>
		<xsl:with-param name="prefix" select="'&lt;&lt; '"/>
	      </xsl:call-template>
	    </xsl:if>	
	    <xsl:if test="@new">
	      <xsl:call-template name="printValue">
		<xsl:with-param name="value" select="@new"/>
		<xsl:with-param name="name" select="@name"/>
		<xsl:with-param name="depth" select="$depth"/>
		<xsl:with-param name="prefix" select="'&gt;&gt; '"/>
	      </xsl:call-template>
	    </xsl:if>	
	  </xsl:for-each>
	</xsl:attribute>
      </diff>	
    </dbobject>
    </xsl:if>
  </xsl:template>

  <xsl:template match="dbobject">
    <xsl:param name="depth"/>
    <xsl:choose>
      <xsl:when test="self::*[@hide='always']"/>
      <xsl:when test="//list[@fulldetails='true']">
	<xsl:call-template name="printObject">
	  <xsl:with-param name="depth" select="$depth"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="self::*[@hide='yes']"/>
      <xsl:when test="//list[@withgrants='true']">
	<xsl:call-template name="printObject">
	  <xsl:with-param name="depth" select="$depth"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:when test="self::*[@type='grant']"/>
      <xsl:otherwise>
	<xsl:call-template name="printObject">
	  <xsl:with-param name="depth" select="$depth"/>
	</xsl:call-template>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="printObject">
    <xsl:param name="depth"/>
    <xsl:copy select=".">
      <xsl:copy-of select="@*"/>
      <xsl:attribute name="generate">
	<xsl:value-of select="'build'"/>
      </xsl:attribute>
      <build>
	<xsl:attribute name="code">
	  <xsl:if test="@type!='cluster'">
	    <xsl:value-of 
	       select="substring('                            ', 1, $depth*2)"/>
	  </xsl:if>

	  <xsl:choose>
	    <xsl:when test="@diff='DiffKids'">
	      <xsl:text>~ </xsl:text>
	    </xsl:when>
	    <xsl:when test="@diff='Diff'">
	      <xsl:text>* </xsl:text>
	    </xsl:when>
	    <xsl:when test="@diff='Gone'">
	      <xsl:text>- </xsl:text>
	    </xsl:when>
	    <xsl:when test="@diff='New'">
	      <xsl:text>+ </xsl:text>
	    </xsl:when>
	    <xsl:when test="@diff='None'">
	      <xsl:text>= </xsl:text>
	    </xsl:when>
	  </xsl:choose>

	  <xsl:value-of select="@type"/>
	  <xsl:text>=</xsl:text>
	  <xsl:value-of select="@name"/>
	  <xsl:text>|</xsl:text>
	  <xsl:value-of select="@fqn"/>
	  <xsl:text>|</xsl:text>
	  <xsl:choose>
	    <xsl:when test="@filtermode">
	      <xsl:value-of select="@filtermode"/>
	    </xsl:when>
	    <xsl:otherwise>
	      <xsl:value-of select="@generate"/>
	    </xsl:otherwise>
	  </xsl:choose>
	  <xsl:text>&#xA;</xsl:text>
	</xsl:attribute>
      </build>
      <xsl:choose>
	<xsl:when test="@type='cluster'">
	  <xsl:apply-templates>
	    <xsl:with-param name="depth" select="$depth"/>
	  </xsl:apply-templates>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates>
	    <xsl:with-param name="depth" select="$depth+1"/>
	  </xsl:apply-templates>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:copy>
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
