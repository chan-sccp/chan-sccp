<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:sccp="http://chan-sccp.net" exclude-result-prefixes="sccp"
>
  <xsl:include href="./lib/translate.xsl"/>
  <xsl:include href="./lib/error.xsl"/>

  <xsl:template match="/">
  <CiscoIPPhoneDirectory>
  <xsl:choose>
    <xsl:when test="not(//response/generic[@event='ParkedCall'])">
      <Title><xsl:call-template name="translate"><xsl:with-param name="key" select="'No Parked Calls'"/></xsl:call-template></Title>
      <Prompt><xsl:call-template name="translate"><xsl:with-param name="key" select="'There are no parked calls at this moment'"/></xsl:call-template>.</Prompt>
    </xsl:when>
    <xsl:otherwise>
      <Title><xsl:call-template name="translate"><xsl:with-param name="key" select="'Parked Calls'"/></xsl:call-template></Title>
      <Prompt><xsl:call-template name="translate"><xsl:with-param name="key" select="'Please Choose one of the parked Calls'"/></xsl:call-template></Prompt>
      <xsl:apply-templates select="//response/generic[@event='ParkedCall']"/>
    </xsl:otherwise>
  </xsl:choose>
  </CiscoIPPhoneDirectory>
  </xsl:template>

  <xsl:template match="//response/generic[@event='ParkedCall']">
  <DirectoryEntry>
    <Name><xsl:value-of select="@calleridname"/> (<xsl:value-of select="@calleridnum"/>) by <xsl:value-of select="@connectedlinename"/></Name>
    <Telephone><xsl:value-of select="@exten"/></Telephone>
    <!--<xsl:value-of select="@channel"/>-->
  </DirectoryEntry>
  </xsl:template>

</xsl:stylesheet> 
