<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
   xmlns:sccp="http://chan-sccp.net" exclude-result-prefixes="sccp"
>
  <xsl:include href="./lib/translate.xsl"/>
  <xsl:include href="./lib/error.xsl"/>

<xsl:template match="/">
  <html>
  <body>
    <h2>Parked Calls</h2>

      <xsl:choose>
	<xsl:when test="not(//response/generic[@event='ParkedCall'])">
	    no Parked Calls 
	</xsl:when>
	<xsl:otherwise>
          <table border="1">
	    <thead>
	    <tr bgcolor="#9acd32">
	      <th>Extension</th>
	      <th>Number</th>
	      <th>Name</th>
	      <th>Channel</th>
	    </tr>
	    </thead>
	    <tbody>
	    <xsl:apply-templates select="//response/generic[@event='ParkedCall']"/>
	    </tbody>
          </table>
        </xsl:otherwise>
      </xsl:choose>
  </body>
  </html>
</xsl:template>

<xsl:template match="//response/generic[@event='ParkedCall']">
      <tr>
	<td><xsl:value-of select="@exten"/></td>
	<td><xsl:value-of select="@calleridnum"/></td>
	<td><xsl:value-of select="@calleridname"/></td>
	<td><xsl:value-of select="@channel"/></td>
      </tr>
</xsl:template>
</xsl:stylesheet> 
