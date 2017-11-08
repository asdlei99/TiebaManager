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

// Defriend.cpp : 定义 DLL 的初始化例程。
//

#include "stdafx.h"
#include "Defriend.h"

#include <StringHelper.h>
#include <NetworkHelper.h>
#include <MiscHelper.h>

#include <TBMAPI.h>
#include <TBMEvents.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


CDefriend::CDefriend(HMODULE module) :
	m_module(module)
{
	g_mainDialogPostInitEvent.AddListener(std::bind(&CDefriend::Init, this), m_module);
}

CDefriend::~CDefriend()
{
	Uninit();
}


void CDefriend::Init()
{
	auto plugin = GetPlugin(m_module);
	if (plugin == NULL)
		return;

	plugin->m_description = _T("批量拉黑插件\r\n")
		                    _T("\r\n")
		                    _T("作者：盗我原号的没J8");
	plugin->m_onConfig = std::bind(&CDefriend::OnConfig, this);
}

void CDefriend::Uninit()
{
	// 关闭窗口
	if (m_defriendDlg != NULL)
		m_defriendDlg->DestroyWindow();

	// 停止线程
	StopDefriend();
	if (m_defriendThread != nullptr && m_defriendThread->joinable())
		m_defriendThread->join();
}

void CDefriend::OnConfig()
{
	if (!GetTiebaOperate().HasSetTieba())
	{
		AfxMessageBox(_T("请先确认贴吧！"), MB_ICONERROR);
		return;
	}

	if (m_defriendDlg == NULL)
	{
		m_defriendDlg = new CDefriendDlg(m_defriendDlg);
		m_defriendDlg->Create(m_defriendDlg->IDD, CWnd::GetDesktopWindow());
	}
}


void CDefriend::StartDefriend(const CString& startPage, const CString& endPage, BOOL defriendNewUsers)
{
	StopDefriend();
	if (m_defriendThread != nullptr && m_defriendThread->joinable())
		m_defriendThread->join();
	m_defriendThread = std::make_unique<std::thread>(&CDefriend::DefriendThread, this, startPage, endPage, defriendNewUsers);
}

void CDefriend::StopDefriend()
{
	m_stopFlag = TRUE;
}

void CDefriend::DefriendThread(CString startPage, CString endPage, BOOL defriendNewUsers)
{
	m_stopFlag = FALSE;

	// 初始化
	if (!CoInitializeHelper())
		return;

	CTiebaOperate& tiebaOperate = GetTiebaOperate();


	int iStartPage = _ttoi(startPage), iEndPage = _ttoi(endPage);
	int iPrevTotalPage = 0;
	do
	{
		if (defriendNewUsers)
		{
			CString url;
			url.Format(_T("https://tieba.baidu.com/bawu2/platform/listMember?ie=utf-8&word=%s"), (LPCTSTR)tiebaOperate.GetEncodedForumName());
			CString src = tiebaOperate.HTTPGet(url);
			CString totalPage = GetStringBetween(src, _T(R"(class="tbui_total_page">共)"), _T("页"));
			if (totalPage == _T(""))
			{
				AfxMessageBox(_T("获取总页数失败！"), MB_ICONERROR);
				break;
			}
			int iTotalPage = _ttoi(totalPage);

			if (iTotalPage <= iPrevTotalPage) // 没有新关注的用户（或者不足1页）
				break;
			if (iPrevTotalPage != 0)
			{
				//iStartPage = 1;
				iEndPage = iTotalPage - iPrevTotalPage;
			}
			iPrevTotalPage = iTotalPage;
		}

		// 拉黑
		DoDefriend(iStartPage, iEndPage);
	} while (defriendNewUsers); // 循环直到没有新关注的用户

	// 结束
	m_stopFlag = TRUE;
	CoUninitialize();
	if (m_defriendDlg != NULL)
	{
		m_defriendDlg->m_startPageEdit.EnableWindow(TRUE);
		m_defriendDlg->m_endPageEdit.EnableWindow(TRUE);
		m_defriendDlg->m_defriendNewUsersCheck.EnableWindow(TRUE);
		m_defriendDlg->m_startButton.EnableWindow(TRUE);
		m_defriendDlg->m_stopButton.EnableWindow(FALSE);
		m_defriendDlg->m_stateStatic.SetWindowText(_T(""));
	}

	TRACE(_T("拉黑线程结束\n"));
}

void CDefriend::DoDefriend(int startPage, int endPage)
{
	ILog& log = GetLog();
	CTiebaOperate& tiebaOperate = GetTiebaOperate();
	CUserCache& userCache = GetUserCache();


	// 获取拉黑列表
	int index = 0;
	std::vector<CString> userName, userID;
	for (int page = startPage; page <= endPage; page++)
	{
		if (m_stopFlag)
			break;
		CString state;
		state.Format(_T("采集第%d页"), page);
		if (m_defriendDlg != NULL)
			m_defriendDlg->m_stateStatic.SetWindowText(state);
		CString url;
		url.Format(_T("https://tieba.baidu.com/bawu2/platform/listMember?ie=utf-8&word=%s&pn=%d"), (LPCTSTR)tiebaOperate.GetEncodedForumName(), page);
		CString src = tiebaOperate.HTTPGet(url);
		if (m_stopFlag)
			break;

		CStringArray rawUsers;
		SplitString(rawUsers, src, _T("<tr><td class=\"left_cell\">"));
		if (rawUsers.GetSize() < 2)
		{
			if (src != NET_TIMEOUT_TEXT)
				WriteString(src, _T("defriend.txt"));
			else
				page--;
			continue;
		}

		userName.resize((int)userName.size() + (int)rawUsers.GetSize() - 1);
		userID.resize((int)userID.size() + (int)rawUsers.GetSize() - 1);
		CString tmp;
		for (int i = 1; i < rawUsers.GetSize(); i++, index++)
		{
			userName[index] = tmp = GetStringBetween(rawUsers[i], _T("un=\""), _T("\""));
			userID[index] = tmp = GetStringBetween(rawUsers[i], _T("id=\""), _T("\""));
		}
	}

	// 拉黑
	for (UINT i = 0; i < userName.size(); i++)
	{
		if (m_stopFlag)
			break;
		/*if (g_defriendedUser.find(userName[i]) != g_defriendedUser.end())
			continue;*/

		CString state;
		state.Format(_T("拉黑中，剩余%u"), userName.size() - i - 1);
		if (m_defriendDlg != NULL)
			m_defriendDlg->m_stateStatic.SetWindowText(state);

		CString code = tiebaOperate.Defriend(userID[i]);
		if (code != _T("0"))
		{
			CString content;
			content.Format(_T("<font color=red>拉黑 </font>%s<font color=red> 失败！错误代码：%s(%s)</font><a href=")
				_T("\"df:%s\">重试</a>"), (LPCTSTR)userName[i], (LPCTSTR)code, (LPCTSTR)GetTiebaErrorText(code), (LPCTSTR)userID[i]);
			log.Log(content);
		}
		else
		{
			userCache.m_defriendedUser->insert(userName[i]);
			log.Log(_T("<font color=red>拉黑 </font>") + userName[i]);
		}

		// 每分钟最多拉黑20个
		if (i % 20 == 19 && i < userName.size() - 1)
		{
			if (m_defriendDlg != NULL)
				m_defriendDlg->m_stateStatic.SetWindowText(_T("延迟中"));
			for (int i = 0; i < 400; i++)
			{
				if (m_stopFlag)
					break;
				Sleep(100);
			}
		}
	}
}

