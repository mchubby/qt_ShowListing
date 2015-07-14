/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined(DIRECTORY_LISTING_FRM_H)
#define DIRECTORY_LISTING_FRM_H

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "../client/User.h"
#include "../client/FastAlloc.h"

#include "FlatTabCtrl.h"
#include "FilteredListViewCtrl.h"
#include "UCHandler.h"
#include "UserInfoBaseHandler.h"
#include "DownloadBaseHandler.h"
#include "TypedTreeCtrl.h"

#include "Async.h"


#include "../client/DirectoryListing.h"
#include "../client/DirectoryListingListener.h"
#include "../client/TargetUtil.h"

#define FILTER_MESSAGE_MAP 8
#define PATH_MESSAGE_MAP 9
#define CONTROL_MESSAGE_MAP 10

struct cmdBarButton {
	int id, image;
	ResourceManager::Strings tooltip;
};


static const cmdBarButton cmdBarButtons[] = {
	{IDC_RELOAD_DIR, 0, ResourceManager::RELOAD},
	{IDC_GETLIST, -2, ResourceManager::GET_FULL_LIST},
	{IDC_MATCH_ADL, -2, ResourceManager::MATCH_ADL},
	{IDC_MATCH_QUEUE, -2, ResourceManager::MATCH_QUEUE},
	{IDC_FILELIST_DIFF, -2, ResourceManager::FILE_LIST_DIFF},
	{IDC_FIND, 1, ResourceManager::FIND},
	{IDC_PREV, 2, ResourceManager::PREVIOUS_SHORT},
	{IDC_NEXT, 3, ResourceManager::NEXT},
};

class DirectoryListingFrame : public MDITabChildWindowImpl<DirectoryListingFrame>, public CSplitterImpl<DirectoryListingFrame>, 
	public UCHandler<DirectoryListingFrame>, private SettingsManagerListener, public UserInfoBaseHandler<DirectoryListingFrame>,
	public DownloadBaseHandler<DirectoryListingFrame>, private DirectoryListingListener, private Async<DirectoryListingFrame>
{
public:
	static void openWindow(DirectoryListing* aList, const string& aDir, const string& aXML);
	static void closeAll();

	typedef MDITabChildWindowImpl<DirectoryListingFrame> baseClass;
	typedef UCHandler<DirectoryListingFrame> ucBase;
	typedef UserInfoBaseHandler<DirectoryListingFrame> uibBase;

	enum {
		COLUMN_FILENAME,
		COLUMN_TYPE,
		COLUMN_EXACTSIZE,
		COLUMN_SIZE,
		COLUMN_TTH,
		COLUMN_DATE,
		COLUMN_LAST
	};
		
	enum {
		STATUS_TEXT,
		STATUS_TOTAL_SIZE,
		STATUS_TOTAL_FILES,
		STATUS_SELECTED_FILES,
		STATUS_SELECTED_SIZE,
		STATUS_FILTER,
		STATUS_DUMMY,
		STATUS_LAST
	};

	DirectoryListingFrame(DirectoryListing* aList);
	 ~DirectoryListingFrame();


	DECLARE_FRAME_WND_CLASS(_T("DirectoryListingFrame"), IDR_DIRECTORY)

	BEGIN_MSG_MAP(DirectoryListingFrame)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_GETINFOTIP, ctrlTree.OnGetChildInfo)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_GETDISPINFO, ctrlTree.OnGetItemDispInfo)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_ITEMEXPANDING, ctrlTree.OnItemExpanding)

		NOTIFY_HANDLER(IDC_FILES, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_FILES, LVN_COLUMNCLICK, ctrlList.onColumnClick)

		NOTIFY_HANDLER(IDC_FILES, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_FILES, NM_DBLCLK, onDoubleClickFiles)
		NOTIFY_HANDLER(IDC_FILES, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_DBLCLK, onDoubleClickDirs)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onSelChangedDirectories)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_CLICK, onClickTree)
		NOTIFY_HANDLER(IDC_FILES, NM_CUSTOMDRAW, onCustomDrawList)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_CUSTOMDRAW, onCustomDrawTree)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		COMMAND_ID_HANDLER(IDC_GO_TO_DIRECTORY, onGoToDirectory)
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchByTTH)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_DATE, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_DIR, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_EXACT_SIZE, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy);
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)

		COMMAND_ID_HANDLER(IDC_OPEN, onOpen)
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenDupe)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenDupe)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER_TREE, onOpenDupeTree)

		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_VIEW_NFO, onViewNFO)
		COMMAND_ID_HANDLER(IDC_RELOAD, onReload)
		COMMAND_ID_HANDLER(IDC_FINDMISSING, onFindMissing)
		COMMAND_ID_HANDLER(IDC_CHECKSFV, onCheckSFV)
		COMMAND_ID_HANDLER(IDC_REFRESH_FILE_LIST, onRefreshShare)

		COMMAND_ID_HANDLER(IDC_SEARCHDIR, onSearchDir)
		
		COMMAND_ID_HANDLER(IDC_FIND, onFind)
		COMMAND_ID_HANDLER(IDC_NEXT, onNext)
		COMMAND_ID_HANDLER(IDC_PREV, onPrev)
		
		COMMAND_ID_HANDLER(IDC_UP, onUp)
		COMMAND_ID_HANDLER(IDC_FORWARD, onForward)
		COMMAND_ID_HANDLER(IDC_BACK, onBack)

		COMMAND_ID_HANDLER(IDC_MATCH_QUEUE, onMatchQueue)
		COMMAND_ID_HANDLER(IDC_MATCH_ADL, onMatchADL)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetFullList)
		NOTIFY_CODE_HANDLER(TBN_DROPDOWN, onListDiff)

		MESSAGE_HANDLER(WM_EXITMENULOOP, onExitMenuLoop)

		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uibBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<DirectoryListingFrame>)
	ALT_MSG_MAP(FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
	ALT_MSG_MAP(PATH_MESSAGE_MAP)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
	ALT_MSG_MAP(CONTROL_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_XBUTTONUP, onXButtonUp)
		MESSAGE_HANDLER(WM_CHAR, onChar)
	END_MSG_MAP()

	LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT onFilterChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onGoToDirectory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onDoubleClickFiles(int idCtrl, LPNMHDR pnmh, BOOL& bHandled); 
	LRESULT onDoubleClickDirs(int idCtrl, LPNMHDR pnmh, BOOL& bHandled); 
	LRESULT onSelChangedDirectories(int idCtrl, LPNMHDR pnmh, BOOL& bHandled); 
	LRESULT onClickTree(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled);
	LRESULT onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT onCustomDrawTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
	LRESULT onXButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
	LRESULT onChar(UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT onOpenDupe(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onOpenDupeTree(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
	LRESULT onSearchDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onFindMissing(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onCheckSFV(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onMatchADL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onGetFullList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onViewNFO(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onReload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onRefreshShare(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT onSearch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void refreshShare(bool usingTree);
	void scanShare(bool usingTree, bool isSfvCheck);

	void onCopyDir();

	void UpdateLayout(BOOL bResizeBars = TRUE);
	void findFile(bool findNext);
	void runUserCommand(UserCommand& uc);
	void resetFilter();

	void refreshTree(const tstring& root, bool reloadList, bool changeDir);

	void selectItem(const tstring& name);
	
	LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);


	LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	void onFind();
	void setWindowTitle();

	LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
		return 1;
	}

	void clearList();

	LRESULT onFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onPrev(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT onUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onForward(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onBack(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled);

	LRESULT onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT onListDiff(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	LRESULT onExitMenuLoop(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

	void onTab();
	
	LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
		PostMessage(WM_CLOSE);
		return 0;
	}
	
	struct UserListHandler {
		UserListHandler(DirectoryListing& _dl) : dl(_dl) { }
		
		void forEachSelected(void (UserInfoBase::*func)()) {
			(dl.*func)();
		}
		
		template<class _Function>
		_Function forEachSelectedT(_Function pred) {
			pred(&dl);
			return pred;
		}
		
	private:
		DirectoryListing& dl;	
	};
	
	UserListHandler getUserList() { return UserListHandler(*dl); }

	/* DownloadBaseHandler functions */
	void appendDownloadItems(OMenu& aMenu, bool isWhole);
	int64_t getDownloadSize(bool isWhole);
	void handleDownload(const string& aTarget, QueueItemBase::Priority p, bool usingTree, TargetUtil::TargetType aTargetType, bool isSizeUnknown);
	bool showDirDialog(string& fileName);
	
	ChildrenState DirectoryListingFrame::getChildrenState(const DirectoryListing::Directory* d) const;
	int getIconIndex(const DirectoryListing::Directory* d) const;
	void expandDir(DirectoryListing::Directory* d, bool /*collapsing*/);
	bool isBold(const DirectoryListing::Directory* d) const;
private:
	bool getLocalPaths(StringList& paths_, bool usingTree, bool dirsOnly);
	void openDupe(const DirectoryListing::Directory* d);
	void openDupe(const DirectoryListing::File* f, bool openDir);

	void updateStatus(const tstring& aMsg);
	string curPath;
	void changeWindowState(bool enable);
	void onReloadPartial(bool dirOnly);
	
	string filter;
	void updateItems(const DirectoryListing::Directory* d, BOOL enableRedraw);
	void changeDir(DirectoryListing::Directory* d, BOOL enableRedraw, bool reload = false);
	void findSearchHit(bool newDir = false);
	int searchPos;
	bool gotoPrev;

	enum ChangeType {
		CHANGE_LIST,
		CHANGE_TREE_SINGLE,
		CHANGE_TREE_DOUBLE,
		CHANGE_TREE_EXPAND,
		CHANGE_TREE_COLLAPSE,
	};

	ChangeType changeType;

	void updateStatus();
	void initStatus();
	void addHistory(const string& name);
	void up();
	void back();
	void forward();

	class ItemInfo : public FastAlloc<ItemInfo> {
	public:
		enum ItemType {
			FILE,
			DIRECTORY
		} type;
		
		union {
			DirectoryListing::File* file;
			DirectoryListing::Directory* dir;
		};

		ItemInfo(DirectoryListing::File* f) : type(FILE), file(f) { }
		ItemInfo(DirectoryListing::Directory* d) : type(DIRECTORY), dir(d) { }

		const tstring getText(uint8_t col) const;
		
		struct TotalSize {
			TotalSize() : total(0) { }
			void operator()(ItemInfo* a) { total += a->type == DIRECTORY ? a->dir->getTotalSize(true) : a->file->getSize(); }
			int64_t total;
		};

		static int compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col);

		int getImageIndex() const;
	};

	void onListItemAction();

	CContainedWindow pathContainer;
	CContainedWindow treeContainer;
	CContainedWindow listContainer;
	
	deque<string> history;
	size_t historyIndex;
	
	TypedTreeCtrl<DirectoryListingFrame, DirectoryListing::Directory> ctrlTree;
	FilteredListViewCtrl<ItemInfo, IDC_FILES> ctrlList;
	CStatusBarCtrl ctrlStatus;
	HTREEITEM treeRoot;
	
	CToolBarCtrl ctrlToolbar;
	CToolBarCtrl arrowBar;
	CComboBox ctrlPath;

	void addCmdBarButtons();
	void addarrowBarButtons();

	CEdit ctrlFilter;
	CContainedWindow ctrlFilterContainer;

	//string currentDir;
	tstring error;

	int skipHits;

	size_t files;

	bool updating;
	bool closed;
	bool statusDirty;

	int statusSizes[13];
	
	DirectoryListing* dl;

	ParamMap ucLineParams;

	typedef unordered_map<UserPtr, DirectoryListingFrame*, User::Hash> UserMap;
	typedef UserMap::const_iterator UserIter;

	static int columnIndexes[COLUMN_LAST];
	static int columnSizes[COLUMN_LAST];

	typedef map< HWND , DirectoryListingFrame* > FrameMap;
	typedef pair< HWND , DirectoryListingFrame* > FramePair;
	typedef FrameMap::iterator FrameIter;

	static FrameMap frames;
	void DisableWindow(bool redraw = true);
	void EnableWindow(bool redraw = true);

	bool disabled;
	void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept;

	void on(DirectoryListingListener::LoadingFinished, int64_t aStart, const string& aDir, bool reloadList, bool changeDir, bool loadInGUIThread) noexcept;
	void on(DirectoryListingListener::LoadingFailed, const string& aReason) noexcept;
	void on(DirectoryListingListener::LoadingStarted, bool changeDir) noexcept;
	void on(DirectoryListingListener::QueueMatched, const string& aMessage) noexcept;
	void on(DirectoryListingListener::Close) noexcept;
	void on(DirectoryListingListener::SearchStarted) noexcept;
	void on(DirectoryListingListener::SearchFailed, bool timedOut) noexcept;
	void on(DirectoryListingListener::ChangeDirectory, const string& aDir, bool isSearchChange) noexcept;
	void on(DirectoryListingListener::UpdateStatusMessage, const string& aMessage) noexcept;
	void on(DirectoryListingListener::Filter) noexcept;

	void filterList();
	void createRoot();
	void convertToFull();
	void onLoadingFinished(int64_t aStart, const string& aDir, bool reloadList, bool changeDir, bool usingGuiThread);
};

#endif // !defined(DIRECTORY_LISTING_FRM_H)

/**
 * @file
 * $Id: DirectoryListingFrm.h 500 2010-06-25 22:08:18Z bigmuscle $
 */
