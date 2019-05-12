<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:sccp="http://chan-sccp.net" exclude-result-prefixes="sccp"
>
  <xsl:include href="./lib/translate.xsl"/>
  <xsl:include href="./lib/error.xsl"/>

  <xsl:template match="/voicemails">
  <CiscoIPPhoneDirectory>
	<Title><xsl:call-template name="translate"><xsl:with-param name="key" select="'Parked Calls'"/></xsl:call-template></Title>

	<!--
	<xsl:for-each select="folders/folder">
	<CiscoIPDirectoryEntry>
		<URL>https://1234/<xsl:value-of select="@name"/>/</URL>
	</CiscoIPDirectoryEntry>
	</xsl:for-each>
	-->

	<xsl:apply-templates select="folders/folder"/>
  </CiscoIPPhoneDirectory>
  </xsl:template>

  <xsl:template match="folders/folder">
	<CiscoIPDirectoryEntry>
		<URL>https://1234/<xsl:value-of select="@name"/>/</URL>
	</CiscoIPDirectoryEntry>
  </xsl:template>

  <xsl:template match="/msgs">
  <CiscoIPPhoneDirectory>
	<Title>Folder: $foldername</Title>

	<xsl:for-each select="msg">
	<CiscoIPDirectoryEntry>
		<Title>Message Entry</Title>
		<URL>https://1234/<xsl:value-of select="@name"/>/</URL>
	</CiscoIPDirectoryEntry>
	</xsl:for-each>

  </CiscoIPPhoneDirectory>
  </xsl:template>

</xsl:stylesheet>
