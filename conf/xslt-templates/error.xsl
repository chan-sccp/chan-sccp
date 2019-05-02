<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
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
