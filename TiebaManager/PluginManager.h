﻿/*
Copyright (C) 2011-2017  xfgryujk
https://tieba.baidu.com/f?kw=%D2%BB%B8%F6%BC%AB%C6%E4%D2%FE%C3%D8%D6%BB%D3%D0xfgryujk%D6%AA%B5%C0%B5%C4%B5%D8%B7%BD

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once
#include <Singleton.h>
#include <Plugin.h>
#include <vector>


class CPluginManager final : public Singleton<CPluginManager>
{
	DECL_SINGLETON(CPluginManager);
public:
	void Init();
	void Uninit();

	const std::vector<CPlugin>& GetPlugins() { return m_plugins; }

	BOOL LoadPlugin(const CString& path);
	BOOL LoadDir(const CString& dir);
	BOOL UnloadPlugin(int index);
	BOOL UnloadAll();

private:
	CPluginManager();
	~CPluginManager();


	std::vector<CPlugin> m_plugins;
};
