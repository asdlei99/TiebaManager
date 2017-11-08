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

#include "stdafx.h"
#include "ExplorerLog.h"

#include "explorer1.h"
#include <MiscHelper.h>
#include <StringHelper.h>


static const TCHAR LOG_FRAME[] = _T(R"(<html><head><meta http-equiv="Content-Type" content="text/html; charset=gbk" /><title>日志</title>)")
								 _T(R"(<style type="text/css">)")
								 _T(R"(body {border:1px solid black; overflow:auto; margin:0px; padding:3px; font-family:"宋体",Verdana; font-size:9pt; line-height:9pt} )")
								 _T(R"(a:link {text-decoration:none} a:hover {text-decoration:underline} a:visited {text-decoration:none})")
								 _T(R"(</style>)")
								 _T(R"(</head><body>)");

static const UINT WM_LOG = WM_APP + 2;


WNDPROC CExplorerLog::s_oldExplorerWndProc = NULL;


// 初始化
void CExplorerLog::Init()
{
	// 取document
	m_logExplorer.Navigate(_T("about:blank"), NULL, NULL, NULL, NULL);
	while (TRUE)
	{
		Delay(1);
		CComDispatchDriver disp(m_logExplorer.get_Document());
		if (disp.p == NULL)
			continue;
		if (SUCCEEDED(disp->QueryInterface(&m_logDocument)))
			break;
	}

	// 写框架
	WriteDocument(LOG_FRAME);

	// 屏蔽右键菜单、监听Log消息
	m_explorerHwnd = m_logExplorer.m_hWnd;
	EnumChildWindows(m_explorerHwnd, EnumChildProc, (LPARAM)&m_explorerHwnd);
	s_oldExplorerWndProc = (WNDPROC)SetWindowLong(m_explorerHwnd, GWL_WNDPROC, (LONG)ExplorerWndProc);

	// 日志开始时间
	GetLocalTime(&m_logStartTime);
}

// 释放
void CExplorerLog::Release()
{
	m_explorerHwnd = NULL;
	m_logDocument.Release();
}

// 输出日志1，把内容格式化发送到消息队列
void CExplorerLog::Log(const CString& content)
{
	if (m_explorerHwnd == NULL)
		return;

	SYSTEMTIME time;
	GetLocalTime(&time);
	CString* output = new CString();
	output->Format(_T("%02d:%02d:%02d %s<br />"), time.wHour, time.wMinute, time.wSecond, content);
	PostMessage(m_explorerHwnd, WM_LOG, (WPARAM)this, (LPARAM)output);
}

// 输出日志2，在m_logExplorer写日志
void CExplorerLog::DoLog(const CString* output)
{
	if (m_logDocument.p == NULL)
	{
		delete output;
		return;
	}

	WriteDocument(*output);
	delete output;

	// 滚动到底端
	CComPtr<IHTMLElement> body;
	m_logDocument->get_body(&body);
	// 取ID
	static LPOLESTR scrollHeightName = OLESTR("scrollHeight"), scrollTopName = OLESTR("scrollTop");
	static DISPID scrollHeightID = -1, scrollTopID = -1;
	if (scrollHeightID == -1)
		body->GetIDsOfNames(IID_NULL, &scrollHeightName, 1, LOCALE_SYSTEM_DEFAULT, &scrollHeightID);
	if (scrollTopID == -1)
		body->GetIDsOfNames(IID_NULL, &scrollTopName, 1, LOCALE_SYSTEM_DEFAULT, &scrollTopID);
	// body.scrollTop = body.scrollHeight
	DISPPARAMS params = {};
	_variant_t scrollHeight;
#pragma warning(suppress: 6102)
	if (scrollHeightID != -1)
		body->Invoke(scrollHeightID, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYGET, &params,
			scrollHeight.GetAddress(), NULL, NULL);
	params.cArgs = 1;
	params.rgvarg = &scrollHeight;
#pragma warning(suppress: 6102)
	if (scrollTopID != -1)
		body->Invoke(scrollTopID, IID_NULL, LOCALE_SYSTEM_DEFAULT, DISPATCH_PROPERTYPUT, &params, NULL, NULL, NULL);
}

// 清空日志
void CExplorerLog::Clear()
{
	if (m_logDocument.p == NULL)
		return;

	IDispatch* tmp = NULL;
	m_logDocument->open(_T("about:blank"), variant_t(), variant_t(), variant_t(), &tmp);
	if (tmp != NULL)
		tmp->Release();
	WriteDocument(LOG_FRAME);
	GetSystemTime(&m_logStartTime);
}

// 保存日志
void CExplorerLog::Save(const CString& folder)
{
	if (m_logDocument.p == NULL)
		return;

	// 取日志HTML
	CComDispatchDriver documentDisp(m_logDocument);

	// document.documentElement.outerHTML
	_variant_t res;
	if (FAILED(documentDisp.GetPropertyByName(OLESTR("documentElement"), res.GetAddress())))
		return;
	CComDispatchDriver documentElementDisp((IDispatch*)res);
	if (FAILED(documentElementDisp.GetPropertyByName(OLESTR("outerHTML"), res.GetAddress())))
		return;
	CString strHtml = (LPCTSTR)(_bstr_t)res;

	// 保存
	CreateDir(folder);
	CString path;
	path.Format(_T("%s\\%d-%02d-%02d %02d：%02d：%02d.html"), folder, m_logStartTime.wYear, m_logStartTime.wMonth,
		m_logStartTime.wDay, m_logStartTime.wHour, m_logStartTime.wMinute, m_logStartTime.wSecond);
	WriteString(strHtml, path);
}

// 枚举寻找Internet Explorer_Server窗口
BOOL CALLBACK CExplorerLog::EnumChildProc(HWND hwnd, LPARAM lParam)
{
	TCHAR buf[30];
	GetClassName(hwnd, buf, _countof(buf));
	if (_tcscmp(buf, _T("Internet Explorer_Server")) == 0)
	{
		*(HWND*)lParam = hwnd;
		return FALSE;
	}
	else
		return TRUE;
}

// 屏蔽日志右键菜单、监听Log消息
LRESULT CALLBACK CExplorerLog::ExplorerWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_LOG)
	{
		((CExplorerLog*)wParam)->DoLog((CString*)lParam);
		return 0;
	}
	if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP)
		return 0;
	return CallWindowProc(s_oldExplorerWndProc, hwnd, uMsg, wParam, lParam);
}

// 写HTML到document
void CExplorerLog::WriteDocument(const CString& content)
{
	SAFEARRAY *arr = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	VARIANT *str = NULL;
	if (FAILED(SafeArrayAccessData(arr, (LPVOID*)&str)))
		return;
	str->vt = VT_BSTR;
	str->bstrVal = content.AllocSysString();
	SafeArrayUnaccessData(arr);
	m_logDocument->write(arr);
	SafeArrayDestroy(arr);
}
