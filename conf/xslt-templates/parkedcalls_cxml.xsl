<?xml version="1.0"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:str="http://exslt.org/strings"
   xmlns:exslt="http://exslt.org/common"
   xmlns:sccp="http://chan-sccp.net" version="1.0" exclude-result-prefixes="sccp"
>
  <xsl:param name="locales" select="'da, nl, de;q=0.9, en-gb;q=0.8, en;q=0.7'"/>
  <xsl:param name="translationFile" select="'./translate.xml'"/>
  <xsl:param name="locale"><xsl:call-template name="findLocale"/></xsl:param>

  <xsl:template name="findLocale">
    <xsl:param name="locales" select='$locales'/>
    <xsl:choose>
      <xsl:when test="document($translationFile)//sccp:translation and not($locales = '')">
        <xsl:variable name="translationNodeSet">
        <translations>
          <xsl:for-each select="str:tokenize($locales, ' -_,')">
            <xsl:variable name="testLocale" select="substring-before(concat(., ';'), ';')"/>
            <xsl:if test="$testLocale = 'en' or document($translationFile)//sccp:translation[@locale=$testLocale]">
            <translation><xsl:value-of select="$testLocale"/></translation>
            </xsl:if>
          </xsl:for-each>
        </translations>
        </xsl:variable>
        <xsl:for-each select="exslt:node-set($translationNodeSet)/translations/translation">
          <xsl:if test="position() = 1">
            <xsl:value-of select="."/>
          </xsl:if>
        </xsl:for-each>
      </xsl:when>
      <xsl:otherwise>en</xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="translate">
    <xsl:param name="key"/>
    <xsl:param name="locale" select='$locale'/>
    <xsl:choose>
      <xsl:when test="not(document($translationFile)//sccp:translation[@locale=$locale]) or $locale = 'en'">
        <xsl:value-of select="$key"/>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="document($translationFile)//sccp:translation[@locale=$locale]/entry[@key=$key]"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
  
  <xsl:template match="/error">
    <xsl:if test="errorno &gt; 0">
      <CiscoIPPhoneText>
        <Title>An Error Occurred</Title>
        <Text><xsl:value-of select="errorno"/>: 
    <xsl:call-template name="translate"><xsl:with-param name="key" select="concat('errorno-',errorno)"/></xsl:call-template></Text>
      </CiscoIPPhoneText>
    </xsl:if>
  </xsl:template>

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
