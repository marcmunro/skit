<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:xi="http://www.w3.org/2003/XInclude"
   xmlns:skit="http://www.bloodnok.com/xml/skit"
   version="1.0">

  <!--- This template is used to process fallback nodes.  These are
        automatically generated when dependencies that contain a
	fallback are processed.  Note that because diffs may have
	dynamically created dependencies (eg, when the owner of an
	object is changed), this is not part of the normal add_deps
	styleseheet procesing.   Instead, fallbacks are processed very
	explicitly from deps.c, as and when they are encountered.
  -->
  <xsl:template match="fallback">
    <root>
      <xsl:variable name="subtype" 
		    select="substring-before(substring-after(@fqn, '.'), 
			                     '.')"/> 
      <xsl:choose>
	<xsl:when test="$subtype='privilege'">
	  <xsl:variable name="names" 
			select="substring-after(@fqn, 
				'fallback.privilege.cluster.')"/> 
	  <xsl:variable name="role" 
			select="substring-before($names, '.')"/> 
	  <xsl:variable name="priv" 
			select="substring-after($names, '.')"/>
	  <dbobject type="fallback" subtype="{$subtype}" fqn="{@fqn}"
		    role="{$role}">
	    <dependencies>
	      <dependency fqn="{concat('role.cluster.', $role)}"/>
	    </dependencies>
	    <fallback fqn="{@fqn}" role="{$role}" privilege="{$priv}"/>
	  </dbobject>
	</xsl:when>
	<xsl:when test="$subtype='grant'">
	  <xsl:variable name="names" 
			select="substring-after(@fqn, 
				'fallback.grant.cluster.')"/> 

	  <xsl:variable name="to" 
			select="substring-before($names, '.')"/> 
	  <xsl:variable name="from" 
			select="substring-after($names, '.')"/>
	  <dbobject type="fallback" subtype="{$subtype}" fqn="{@fqn}"
		    from="{$from}" to="{$to}">
	    <dependencies>
	      <dependency fqn="{concat('role.cluster.', $from)}"/>
	      <dependency fqn="{concat('role.cluster.', $to)}"/>
	    </dependencies>
	  </dbobject>
	</xsl:when>
      </xsl:choose>
    </root>
  </xsl:template>
</xsl:stylesheet>
