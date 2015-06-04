<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" doctype-system="about:legacy-compat" encoding="UTF-8" indent="yes" />

<xsl:variable name='endl'><xsl:text>
</xsl:text></xsl:variable>

<xsl:template match="/">
	<html>
		<head>
			<meta charset="utf-8" />
			<title>Valgrind report</title>
			<style>
				.field-label {
					display: inline-block;
					width: 20ex;
					vertical-align: top;
				}
				.field-value {
					display: inline-block;
					vertical-align: baseline;
				}
				.stack-table {
					border-collapse: collapse;
					border-top: solid 2px;
					border-bottom: solid 2px;
				}
				.table-heading {
					font-weight: normal;
					text-align: left;
					border-bottom: solid 2px;
				}
				.table-cell {
					vertical-align: top;
				}
				pre {
					margin: 0px;
					font-size: 100%;
				}
			</style>
		</head>
		<body>
			<h1>Valgrind report</h1>
			<xsl:apply-templates select="valgrindoutput/protocolversion"/> 
			<xsl:apply-templates select="valgrindoutput/protocoltool"/> 
			<xsl:apply-templates select="valgrindoutput/pid"/> 
			<xsl:apply-templates select="valgrindoutput/ppid"/> 
			<xsl:apply-templates select="valgrindoutput/args"/> 
			<h2>Status</h2>
			<xsl:apply-templates select="valgrindoutput/status"/> 
			<h2>Errors</h2>
			<xsl:apply-templates select="valgrindoutput/error"/> 
		</body>
	</html>
</xsl:template>

<xsl:template match="protocolversion">
	<div>
		<div class="field-label">Protocol version:</div>
		<div class="field-value"><xsl:value-of select="."/></div>
	</div>
</xsl:template>

<xsl:template match="preamble">
<!--
	<div>
		<div class="field-label">Protocol version:</div>
		<pre class="field-value">
			<xsl:for-each select="line">
				<xsl:value-of select="."/>
				<xsl:value-of select="$endl"/>
			</xsl:for-each>
		</pre>
	</div>
-->
</xsl:template>

<xsl:template match="pid">
	<div>
		<div class="field-label">PID:</div>
		<div class="field-value"><xsl:value-of select="."/></div>
	</div>
</xsl:template>

<xsl:template match="ppid">
	<div>
		<div class="field-label">PPID:</div>
		<div class="field-value"><xsl:value-of select="."/></div>
	</div>
</xsl:template>

<xsl:template match="protocoltool">
	<div>
		<div class="field-label">Valgrind Tool:</div>
		<div class="field-value"><xsl:value-of select="."/></div>
	</div>
</xsl:template>

<xsl:template match="args">
	<div>
		<div class="field-label">Command line:</div>
		<pre class="field-value">
			<xsl:value-of select="vargv/exe"/> 
			<xsl:text> </xsl:text>
			<xsl:for-each select="vargv/arg">
				<xsl:value-of select="."/> 
				<xsl:text> </xsl:text>
			</xsl:for-each>
			<xsl:text> </xsl:text>
			<xsl:value-of select="argv/exe"/>
		</pre>
	</div>
</xsl:template>

<xsl:template match="status">
	<div>
		<div class="field-label"><xsl:value-of select="time"/></div>
		<div class="field-value"><xsl:value-of select="state"/></div>
	</div>
</xsl:template>

<xsl:template match="error">
	<div>
		<div class="field-label">Unique:</div>
		<div class="field-value"><xsl:value-of select="unique"/></div>
	</div>
	<div>
		<div class="field-label">Thread:</div>
		<div class="field-value"><xsl:value-of select="tid"/></div>
	</div>
	<div>
		<div class="field-label">Kind:</div>
		<div class="field-value"><xsl:value-of select="kind"/></div>
	</div>
	<div>
		<div class="field-label">What:</div>
		<div class="field-value"><xsl:value-of select="xwhat/text"/></div>
	</div>
	<xsl:if test="xwhat/leakedbytes">
		<div>
			<div class="field-label">Leaked bytes:</div>
			<div class="field-value"><xsl:value-of select="xwhat/leakedbytes"/></div>
		</div>
	</xsl:if>
	<xsl:if test="xwhat/leakedblocks">
		<div>
			<div class="field-label">Leaked blocks:</div>
			<div class="field-value"><xsl:value-of select="xwhat/leakedblocks"/></div>
		</div>
	</xsl:if>
	<h2>Stack</h2>
	<xsl:apply-templates select="stack"/> 
	<xsl:apply-templates select="auxwhat"/>
</xsl:template>

<xsl:template match="stack">
<table class="stack-table">
	<tr>
		<th class="table-heading">IP</th>
		<th class="table-heading">Obj</th>
		<th class="table-heading">Function</th>
		<th class="table-heading">Directory</th>
		<th class="table-heading">File</th>
		<th class="table-heading">Line</th>
	</tr>
	<xsl:for-each select="frame">
		<tr>
			<td class="table-cell"><xsl:value-of select="ip"/></td>
			<td class="table-cell"><xsl:value-of select="obj"/></td>
			<td class="table-cell"><xsl:value-of select="fn"/></td>
			<xsl:if test="dir">
				<td class="table-cell"><xsl:value-of select="dir"/></td>
			</xsl:if>
			<xsl:if test="file">
				<td class="table-cell"><xsl:value-of select="file"/></td>
			</xsl:if>
			<xsl:if test="line">
				<td class="table-cell"><xsl:value-of select="line"/></td>
			</xsl:if>
		</tr>
	</xsl:for-each>
</table>
</xsl:template>

<xsl:template match="auxwhat">
<h2 style="color:#336600"><u>AUXWHAT:</u></h2>
 <table width="40%" border="0" cellpadding="2" cellspacing="2">
  <tr>
   <td><xsl:value-of select="."/></td>
  </tr>
 </table>
</xsl:template>

<xsl:template match="errorcounts[not(node())]">
<h2 style="color:#336600"><u> ERRORCOUNTS: </u></h2>
<table width="30%" border="0" cellpadding="2" cellspacing="2">
 <tr>
   <td width="10%" align="left"> Count </td>
   <td width="20%" align="left"> Unique </td>
 </tr>
 <xsl:for-each select="pair">
  <tr>
    <td style="color:#0000ff" align="left"><xsl:value-of select="count"/></td>
    <td style="color:#0000ff" align="left"><xsl:value-of select="unique"/></td>
 </tr>
 </xsl:for-each>
 </table>
</xsl:template>

<xsl:template match="suppcounts[not(node())]">
<h5 style="color:#336600"><u> SUPPCOUNTS: </u></h5>
<table width="30%" border="0" cellpadding="2" cellspacing="2">
 <tr>
   <td width="10%" align="left"> Count </td>
   <td width="20%" align="left"> Name </td>
 </tr>
 <xsl:for-each select="pair">
  <tr>
    <td style="color:#0000ff" align="left"><xsl:value-of select="count"/></td>
    <td style="color:#0000ff" align="left"><xsl:value-of select="name"/></td>
 </tr>
 </xsl:for-each>
 </table>
</xsl:template>

</xsl:stylesheet>
