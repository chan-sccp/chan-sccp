<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:sccp="http://chan-sccp.net" exclude-result-prefixes="sccp"
>
  <xsl:include href="./lib/translate.xsl"/>
  <xsl:include href="./lib/error.xsl"/>

<xsl:template match="/device">
<CiscoIPPhoneInput>
<xsl:if test="protocolversion &gt; 15">
<xsl:attribute name="appId"><xsl:value-of select="conference/appId"/></xsl:attribute>
</xsl:if>
<Title><xsl:call-template name="translate"><xsl:with-param name="key" select="'Conference'"/></xsl:call-template><xsl:text> </xsl:text><xsl:value-of select="conference/id"/></Title>
<Prompt><xsl:call-template name="translate"><xsl:with-param name="key" select="'Enter Phone# to Invite'"/></xsl:call-template></Prompt>
<URL><xsl:value-of select="server_url"/>?handler=<xsl:value-of select="handler"/>&amp;<xsl:value-of select="uri"/>&amp;action=PROCESS_INVITE</URL>
<xsl:apply-templates select="conference"/>
</CiscoIPPhoneInput>
</xsl:template>

<xsl:template match="conference">
<InputItem>
<DisplayName><xsl:call-template name="translate"><xsl:with-param name="key" select="'Phone Number'"/></xsl:call-template></DisplayName>
<QueryStringParam>NUMBER</QueryStringParam>
<InputFlags>T</InputFlags>
</InputItem>
<InputItem>
<DisplayName><xsl:call-template name="translate"><xsl:with-param name="key" select="'Name'"/></xsl:call-template></DisplayName>
<QueryStringParam>NAME</QueryStringParam>
<InputFlags>T</InputFlags>
</InputItem>
</xsl:template>

</xsl:stylesheet>
