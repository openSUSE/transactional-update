<?xml version='1.0'?>
<!--
  Copyright 2018 Ignaz Forster <iforster@suse.com>

  This file is part of transactional-update.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
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
