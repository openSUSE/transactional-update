<?xml version='1.0'?>
<!--
  SPDX-License-Identifier: GPL-2.0-or-later
  SPDX-FileCopyrightText: Copyright SUSE LLC
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:ss="http://docbook.sf.net/xmlns/string.subst/1.0" version="1.0">
  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>
  <xsl:param name="sysconfdir"/>
  <xsl:param name="prefix"/>

  <xsl:param name="man.string.subst.map.local.pre">
    <ss:substitution oldstring="%sysconfdir%" newstring="{$sysconfdir}" />
    <ss:substitution oldstring="%prefix%" newstring="{$prefix}" />
  </xsl:param>
</xsl:stylesheet>
