<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:str="http://exslt.org/strings"
	xmlns:exslt="http://exslt.org/common"
	xmlns:sccp="http://chan-sccp.net" exclude-result-prefixes="sccp"
>
  <xsl:template match="/error">
    <xsl:if test="errorno &gt; 0">
      <CiscoIPPhoneText>
        <Title>An Error Occurred</Title>
        <Text><xsl:value-of select="errorno"/>: 
		<xsl:call-template name="translate"><xsl:with-param name="key" select="concat('errorno-',errorno)"/></xsl:call-template>
	</Text>
      </CiscoIPPhoneText>
    </xsl:if>
  </xsl:template>
</xsl:stylesheet>
