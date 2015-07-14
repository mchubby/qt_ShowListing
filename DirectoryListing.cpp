
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

#include "stdinc.h"
#include "Bundle.h"
#include "DirectoryListing.h"

#include "AutoSearchManager.h"
#include "QueueManager.h"
#include "ShareManager.h"

#include "StringTokenizer.h"
#include "SimpleXML.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "ResourceManager.h"
#include "SimpleXMLReader.h"
#include "User.h"
#include "ADLSearch.h"
#include "DirectoryListingManager.h"
#include "ScopedFunctor.h"


namespace dcpp {

using boost::range::for_each;
using boost::range::find_if;

DirectoryListing::DirectoryListing(const HintedUser& aUser, bool aPartial, const string& aFileName, bool aIsClientView, bool aIsOwnList) : 
	hintedUser(aUser), abort(false), root(new Directory(nullptr, Util::emptyString, Directory::TYPE_INCOMPLETE_NOCHILD)), partialList(aPartial), isOwnList(aIsOwnList), fileName(aFileName),
	isClientView(aIsClientView), curSearch(nullptr), lastResult(0), matchADL(SETTING(USE_ADLS) && !aPartial), typingFilter(false), waiting(false), maxResultCount(0), curResultCount(0)
{
	running.clear();

	ClientManager::getInstance()->addListener(this);
}

DirectoryListing::~DirectoryListing() {
	ClientManager::getInstance()->removeListener(this);
	delete root;
}

void DirectoryListing::sortDirs() {
	root->sortDirs();
}

void DirectoryListing::Directory::sortDirs(bool recursive /*true*/) {
	if (recursive) {
		for(auto d: directories)
			d->sortDirs();
	}

	sort(directories.begin(), directories.end(), Directory::DefaultSort());
}

void DirectoryListing::Directory::sortFiles() {
	sort(files.begin(), files.end(), File::DefaultSort());
}


bool DirectoryListing::Directory::Sort::operator()(const Ptr& a, const Ptr& b) const {
	return compare(a->getName(), b->getName()) < 0;
}

bool DirectoryListing::Directory::DefaultSort::operator()(const Ptr& a, const Ptr& b) const {
	if (a->getAdls() && !b->getAdls())
		return true;
	if (!a->getAdls() && b->getAdls())
		return false;
	return Util::DefaultSort(Text::toT(a->getName()).c_str(), Text::toT(b->getName()).c_str()) < 0;
}

bool DirectoryListing::File::DefaultSort::operator()(const Ptr& a, const Ptr& b) const {
	return Util::DefaultSort(Text::toT(a->getName()).c_str(), Text::toT(b->getName()).c_str()) < 0;
}

bool DirectoryListing::File::Sort::operator()(const Ptr& a, const Ptr& b) const {
	return compare(a->getName(), b->getName()) < 0;
}

string DirectoryListing::getNick(bool firstOnly) const {
	string ret;
	if (!hintedUser.user->isOnline()) {
		if (isOwnList) {
			ret = SETTING(NICK);
		} else if (!partialList) {
			ret = DirectoryListing::getNickFromFilename(fileName);
		}
	}

	if (ret.empty()) {
		if (firstOnly) {
			ret = ClientManager::getInstance()->getNick(hintedUser.user, hintedUser.hint, true);
		} else {
			ret = ClientManager::getInstance()->getFormatedNicks(hintedUser);
		}
	}

	return ret;
}

void stripExtensions(string& name) {
	if(stricmp(name.c_str() + name.length() - 4, ".bz2") == 0) {
		name.erase(name.length() - 4);
	}

	if(stricmp(name.c_str() + name.length() - 4, ".xml") == 0) {
		name.erase(name.length() - 4);
	}
}

string DirectoryListing::getNickFromFilename(const string& fileName) {
	// General file list name format: [username].[CID].[xml|xml.bz2]

	string name = Util::getFileName(fileName);

	// Strip off any extensions
	stripExtensions(name);

	// Find CID
	string::size_type i = name.rfind('.');
	if(i == string::npos) {
		return STRING(UNKNOWN);
	}

	return name.substr(0, i);
}

UserPtr DirectoryListing::getUserFromFilename(const string& fileName) {
	// General file list name format: [username].[CID].[xml|xml.bz2]

	string name = Util::getFileName(fileName);

	// Strip off any extensions
	stripExtensions(name);

	// Find CID
	string::size_type i = name.rfind('.');
	if(i == string::npos) {
		return UserPtr();
	}

	size_t n = name.length() - (i + 1);
	// CID's always 39 chars long...
	if(n != 39)
		return UserPtr();

	CID cid(name.substr(i + 1));
	if(cid.isZero())
		return UserPtr();

	return ClientManager::getInstance()->getUser(cid);
}

bool DirectoryListing::supportsASCH() const {
	return !partialList || isOwnList || hintedUser.user->isSet(User::ASCH);
}

void DirectoryListing::loadFile() {
	if (isOwnList) {
		auto mis = ShareManager::getInstance()->generatePartialList("/", true, Util::toInt(fileName));
		if (mis) {
			loadXML(*mis, true);
		} else {
			throw Exception(CSTRING(FILE_NOT_AVAILABLE));
		}
	} else {

		// For now, we detect type by ending...
		string ext = Util::getFileExt(fileName);

		dcpp::File ff(fileName, dcpp::File::READ, dcpp::File::OPEN);

		if(stricmp(ext, ".bz2") == 0) {
			FilteredInputStream<UnBZFilter, false> f(&ff);
			loadXML(f, false);
		} else if(stricmp(ext, ".xml") == 0) {
			loadXML(ff, false);
		}
	}
}

class ListLoader : public SimpleXMLReader::CallBack {
public:
	ListLoader(DirectoryListing* aList, DirectoryListing::Directory* root, const string& aBase, bool aUpdating, const UserPtr& aUser, bool aCheckDupe, bool aPartialList) : 
	  list(aList), cur(root), base(aBase), inListing(false), updating(aUpdating), user(aUser), checkDupe(aCheckDupe), partialList(aPartialList), dirsLoaded(0) { 
	}

	virtual ~ListLoader() { }

	void startTag(const string& name, StringPairList& attribs, bool simple);
	void endTag(const string& name);

	//const string& getBase() const { return base; }
	int getLoadedDirs() { return dirsLoaded; }
private:
	DirectoryListing* list;
	DirectoryListing::Directory* cur;
	UserPtr user;

	string baseLower;
	string base;
	bool inListing;
	bool updating;
	bool checkDupe;
	bool partialList;
	int dirsLoaded;
};

int DirectoryListing::updateXML(const string& xml, const string& aBase) {
	MemoryInputStream mis(xml);
	return loadXML(mis, true, aBase);
}

int DirectoryListing::loadXML(InputStream& is, bool updating, const string& aBase) {
	ListLoader ll(this, root, aBase, updating, getUser(), !isOwnList && isClientView && SETTING(DUPES_IN_FILELIST), partialList);
	try {
		dcpp::SimpleXMLReader(&ll).parse(is);
	} catch(SimpleXMLException& e) {
		//Better to abort and show the error, than just leave it hanging.
		LogManager::getInstance()->message("Error in Filelist loading: "  + e.getError() + ". User: [ " +  
			getNick(false) + " ]", LogManager::LOG_ERROR);
		//dcdebug("DirectoryListing loadxml error: %s", e.getError());
	}
	return ll.getLoadedDirs();
}

static const string sFileListing = "FileListing";
static const string sBase = "Base";
static const string sBaseDate = "BaseDate";
static const string sGenerator = "Generator";
static const string sDirectory = "Directory";
static const string sIncomplete = "Incomplete";
static const string sChildren = "Children";
static const string sFile = "File";
static const string sName = "Name";
static const string sSize = "Size";
static const string sTTH = "TTH";
static const string sDate = "Date";
void ListLoader::startTag(const string& name, StringPairList& attribs, bool simple) {
	if(list->getAbort()) {
		throw AbortException();
	}

	if(inListing) {
		if(name == sFile) {
			const string& n = getAttrib(attribs, sName, 0);
			if(n.empty())
				return;
			const string& s = getAttrib(attribs, sSize, 1);
			if(s.empty())
				return;
			auto size = Util::toInt64(s);

			const string& h = getAttrib(attribs, sTTH, 2);
			if(h.empty() && !SettingsManager::lanMode)
				return;		
			TTHValue tth(h); /// @todo verify validity?

			DirectoryListing::File* f = new DirectoryListing::File(cur, n, size, tth, checkDupe, Util::toUInt32(getAttrib(attribs, sDate, 3)));
			cur->files.push_back(f);
		} else if(name == sDirectory) {
			const string& n = getAttrib(attribs, sName, 0);
			if(n.empty()) {
				throw SimpleXMLException("Directory missing name attribute");
			}

			bool incomp = getAttrib(attribs, sIncomplete, 1) == "1";
			bool children = getAttrib(attribs, sChildren, 2) == "1";

			const string& size = getAttrib(attribs, sSize, 2);
			const string& date = getAttrib(attribs, sDate, 3);

			DirectoryListing::Directory* d = nullptr;
			if(updating) {
				dirsLoaded++;
				auto s =  list->baseDirs.find(baseLower + Text::toLower(n) + '/');
				if (s != list->baseDirs.end()) {
					d = s->second.first;
				}
			}

			if(!d) {
				d = new DirectoryListing::Directory(cur, n, incomp ? (children ? DirectoryListing::Directory::TYPE_INCOMPLETE_CHILD : DirectoryListing::Directory::TYPE_INCOMPLETE_NOCHILD) : 
					DirectoryListing::Directory::TYPE_NORMAL, (partialList && checkDupe), size, Util::toUInt32(date));
				cur->directories.push_back(d);
				if (updating && !incomp)
					list->baseDirs[baseLower + Text::toLower(n) + '/'] = make_pair(d, true); //recursive partial lists
			} else {
				if(!incomp) {
					d->setComplete();
				}
				d->setDate(Util::toUInt32(date));
			}
			cur = d;
			if (updating && cur->isComplete())
				baseLower += Text::toLower(n) + '/';

			if(simple) {
				// To handle <Directory Name="..." />
				endTag(name);
			}
		}
	} else if(name == sFileListing) {
		if (updating) {
			const string& b = getAttrib(attribs, sBase, 2);
			if(b.size() >= 1 && b[0] == '/' && b[b.size()-1] == '/') {
				base = b;
				if (b != base) 
					throw AbortException("The base directory specified in the file list (" + b + ") doesn't match with the excepted base (" + base + ")");
			}
			const string& date = getAttrib(attribs, sBaseDate, 3);

			StringList sl = StringTokenizer<string>(base.substr(1), '/').getTokens();
			for(auto& name: sl) {
				auto s = find_if(cur->directories, [&name](DirectoryListing::Directory* dir) { return dir->getName() == name; });
				if (s == cur->directories.end()) {
					auto d = new DirectoryListing::Directory(cur, name, DirectoryListing::Directory::TYPE_INCOMPLETE_CHILD, true);
					cur->directories.push_back(d);
					list->baseDirs[Text::toLower(Util::toAdcFile(d->getPath()))] = make_pair(d, false);
					cur = d;
				} else {
					cur = *s;
				}
			}

			baseLower = Text::toLower(base);
			auto& p = list->baseDirs[baseLower];

			//set the dir as visited
			p.second = true;

			cur->setDate(Util::toUInt32(date));
		}

		//set the root complete only after we have finished loading (will prevent possible problems like the GUI counting the size for this folder)
		inListing = true;

		if(simple) {
			// To handle <Directory Name="..." />
			endTag(name);
		}
	}
}

void ListLoader::endTag(const string& name) {
	if(inListing) {
		if(name == sDirectory) {
			if (updating && cur->isComplete())
				baseLower = baseLower.substr(0, baseLower.length()-cur->getName().length()-1);
			cur = cur->getParent();
		} else if(name == sFileListing) {
			// cur should be root now, set it complete
			cur->setComplete();
			inListing = false;
		}
	}
}

DirectoryListing::File::File(Directory* aDir, const string& aName, int64_t aSize, const TTHValue& aTTH, bool checkDupe, time_t aDate) noexcept : 
	name(aName), size(aSize), parent(aDir), tthRoot(aTTH), adls(false), dupe(DUPE_NONE), date(aDate) {
	if (checkDupe && size > 0) {
		dupe = SettingsManager::lanMode ? AirUtil::checkFileDupe(name, size) : AirUtil::checkFileDupe(tthRoot, name);
	}
}

DirectoryListing::Directory::Directory(Directory* aParent, const string& aName, Directory::DirType aType, bool checkDupe, const string& aSize, time_t aDate /*0*/) 
		: name(aName), parent(aParent), type(aType), dupe(DUPE_NONE), partialSize(0), date(aDate), loading(false) {

	if (!aSize.empty()) {
		partialSize = Util::toInt64(aSize);
	}

	if (checkDupe) {
		dupe = AirUtil::checkDirDupe(getPath(), partialSize);
	}
}

void DirectoryListing::Directory::search(OrderedStringSet& aResults, AdcSearch& aStrings, StringList::size_type maxResults) {
	if (getAdls())
		return;

	if (aStrings.hasRoot) {
		auto pos = find_if(files, [aStrings](File* aFile) { return aFile->getTTH() == aStrings.root; });
		if (pos != files.end()) {
			aResults.insert(getPath());
		}
	} else {
		if(aStrings.matchesDirectory(name)) {
			auto path = parent ? parent->getPath() : Util::emptyString;
			auto res = find(aResults, path);
			if (res == aResults.end() && aStrings.matchesSize(getTotalSize(false))) {
				aResults.insert(path);
			}
		}

		if(aStrings.itemType != AdcSearch::TYPE_DIRECTORY) {
			for(auto& f: files) {
				if(aStrings.matchesFileLower(Text::toLower(f->getName()), f->getSize(), f->getDate())) {
					aResults.insert(getPath());
					break;
				}
			}
		}
	}

	for(auto l = directories.begin(); (l != directories.end()) && (aResults.size() < maxResults); ++l) {
		(*l)->search(aResults, aStrings, maxResults);
	}
}

string DirectoryListing::getPath(const Directory* d) const {
	if(d == root)
		return Util::emptyString;

	string dir;
	dir.reserve(128);
	dir.append(d->getName());
	dir.append(1, '\\');

	Directory* cur = d->getParent();
	while(cur!=root) {
		dir.insert(0, cur->getName() + '\\');
		cur = cur->getParent();
	}
	return dir;
}

bool DirectoryListing::Directory::findIncomplete() {
	/* Recursive check for incomplete dirs */
	if(!isComplete()) {
		return true;
	}
	return find_if(directories, [](Directory* dir) { return dir->findIncomplete(); }) != directories.end();
}

void DirectoryListing::Directory::download(const string& aTarget, BundleFileList& aFiles) {
	// First, recurse over the directories
	sort(directories.begin(), directories.end(), Directory::Sort());
	for(auto d: directories) {
		d->download(aTarget + d->getName() + PATH_SEPARATOR, aFiles);
	}

	// Then add the files
	sort(files.begin(), files.end(), File::Sort());
	for(auto& f: files) {
		aFiles.emplace_back(aTarget + f->getName(), f->getTTH(), f->getSize());
	}
}

bool DirectoryListing::createBundle(Directory* aDir, const string& aTarget, QueueItemBase::Priority prio, ProfileToken aAutoSearch) {
	string target = aTarget;
	if (aDir != root)
		target += aDir->getName() + PATH_SEPARATOR;

	BundleFileList aFiles;
	aDir->download(Util::emptyString, aFiles);

	if (aFiles.empty() || (SETTING(SKIP_ZERO_BYTE) && none_of(aFiles.begin(), aFiles.end(), [](const BundleFileInfo& aFile) { return aFile.size > 0; }))) {
		fire(DirectoryListingListener::UpdateStatusMessage(), STRING(DIR_EMPTY) + " " + aDir->getName());
		return false;
	}

	string errorMsg;
	BundlePtr b = QueueManager::getInstance()->createDirectoryBundle(target, hintedUser, aFiles, prio, aDir->getDate(), errorMsg);
	if (!errorMsg.empty()) {
		if (aAutoSearch == 0) {
			LogManager::getInstance()->message(STRING_F(ADD_BUNDLE_ERRORS_OCC, target % getNick(false) % errorMsg), LogManager::LOG_WARNING);
		} else {
			AutoSearchManager::getInstance()->onBundleError(aAutoSearch, errorMsg, target, hintedUser);
		}
	}

	if (b) {
		if (aAutoSearch > 0) {
			AutoSearchManager::getInstance()->onBundleCreated(b, aAutoSearch);
		}
		return true;
	}
	return false;
}

bool DirectoryListing::downloadDir(Directory* aDir, const string& aTarget, TargetUtil::TargetType aTargetType, bool isSizeUnknown, QueueItemBase::Priority prio, ProfileToken aAutoSearch) {
	//check if there are incomplete dirs in a partial list
	if (partialList && aDir->findIncomplete()) {
		if (isClientView) {
			DirectoryListingManager::getInstance()->addDirectoryDownload(aDir->getPath(), hintedUser, aTarget, aTargetType, isSizeUnknown ? ASK_USER : NO_CHECK, prio);
		} else {
			//there shoudn't be incomplete dirs in recursive partial lists, most likely the other client doesn't support the RE flag
			DirectoryListingManager::getInstance()->addDirectoryDownload(aDir->getPath(), hintedUser, aTarget, aTargetType, isSizeUnknown ? ASK_USER : NO_CHECK, prio, true);
		}
		return false;
	}

	/* Check if this is a root dir containing release dirs */
	boost::regex reg;
	reg.assign(AirUtil::getReleaseRegBasic());
	if (!boost::regex_match(aDir->getName(), reg) && aDir->files.empty() && !aDir->directories.empty() &&
		all_of(aDir->directories.begin(), aDir->directories.end(), [&reg](Directory* d) { return boost::regex_match(d->getName(), reg); })) {
			
		/* Create bundles from each subfolder */
		bool queued = false;
		for(auto d: aDir->directories) {
			if (createBundle(d, aTarget + aDir->getName() + PATH_SEPARATOR, prio, aAutoSearch))
				queued = true;
		}
		return queued;
	}

	return createBundle(aDir, aTarget, prio, aAutoSearch);
}

bool DirectoryListing::downloadDir(const string& aDir, const string& aTarget, TargetUtil::TargetType aTargetType, bool highPrio, QueueItemBase::Priority prio, ProfileToken aAutoSearch) {
	dcassert(aDir.size() > 2);
	dcassert(aDir[aDir.size() - 1] == '\\'); // This should not be PATH_SEPARATOR
	Directory* d = findDirectory(aDir, root);
	if(d)
		return downloadDir(d, aTarget, aTargetType, highPrio, prio, aAutoSearch);
	return false;
}

int64_t DirectoryListing::getDirSize(const string& aDir) {
	dcassert(aDir.size() > 2);
	dcassert(aDir[aDir.size() - 1] == '\\'); // This should not be PATH_SEPARATOR
	Directory* d = findDirectory(aDir, root);
	if(d)
		return d->getTotalSize(false);
	return 0;
}

void DirectoryListing::openFile(File* aFile, bool isClientView) {
	QueueManager::getInstance()->addOpenedItem(aFile->getName(), aFile->getSize(), aFile->getTTH(), hintedUser, isClientView);
}

DirectoryListing::Directory* DirectoryListing::findDirectory(const string& aName, const Directory* current) const {
	if (aName.empty())
		return root;

	string::size_type end = aName.find('\\');
	dcassert(end != string::npos);
	string name = aName.substr(0, end);

	auto i = find(current->directories.begin(), current->directories.end(), name);
	if(i != current->directories.end()) {
		if(end == (aName.size() - 1))
			return *i;
		else
			return findDirectory(aName.substr(end + 1), *i);
	}
	return nullptr;
}

void DirectoryListing::Directory::findFiles(const boost::regex& aReg, File::List& aResults) const {
	copy_if(files.begin(), files.end(), back_inserter(aResults), [&aReg](File* df) { return boost::regex_match(df->getName(), aReg); });

	for(auto d: directories)
		d->findFiles(aReg, aResults); 
}

bool DirectoryListing::findNfo(const string& aPath) {
	auto dir = findDirectory(aPath, root);
	if (dir) {
		boost::regex reg;
		reg.assign("(.+\\.nfo)", boost::regex_constants::icase);
		File::List results;
		dir->findFiles(reg, results);

		if (!results.empty()) {
			try {
				openFile(results.front(), true);
			} catch(const Exception&) { }
			return true;
		}
	}

	if (isClientView)
		fire(DirectoryListingListener::UpdateStatusMessage(), CSTRING(NO_NFO_FOUND));
	else
		LogManager::getInstance()->message(getNick(false) + ": " + STRING(NO_NFO_FOUND), LogManager::LOG_INFO);
	return false;
}

struct HashContained {
	HashContained(const DirectoryListing::Directory::TTHSet& l) : tl(l) { }
	const DirectoryListing::Directory::TTHSet& tl;
	bool operator()(const DirectoryListing::File::Ptr i) const {
		return tl.count((i->getTTH())) && (DeleteFunction()(i), true);
	}
private:
	HashContained& operator=(HashContained&);
};

struct DirectoryEmpty {
	bool operator()(const DirectoryListing::Directory::Ptr i) const {
		bool r = i->getFileCount() + i->directories.size() == 0;
		if (r) DeleteFunction()(i);
		return r;
	}
};

struct SizeLess {
	bool operator()(const DirectoryListing::File::Ptr f) const {
		return f->getSize() < (SETTING(SKIP_SUBTRACT) *1024);
	}
};

DirectoryListing::Directory::~Directory() {
	for_each(directories, DeleteFunction());
	for_each(files, DeleteFunction());
}

void DirectoryListing::Directory::clearAll() {
	for_each(directories, DeleteFunction());
	for_each(files, DeleteFunction());
	directories.clear();
	files.clear();
}

void DirectoryListing::Directory::filterList(DirectoryListing& dirList) {
	DirectoryListing::Directory* d = dirList.getRoot();

	TTHSet l;
	d->getHashList(l);
	filterList(l);
}

void DirectoryListing::Directory::filterList(DirectoryListing::Directory::TTHSet& l) {
	for(auto d: directories) 
		d->filterList(l);

	directories.erase(remove_if(directories.begin(), directories.end(), DirectoryEmpty()), directories.end());
	files.erase(remove_if(files.begin(), files.end(), HashContained(l)), files.end());

	if((SETTING(SKIP_SUBTRACT) > 0) && (files.size() < 2)) {   //setting for only skip if folder filecount under x ?
		files.erase(remove_if(files.begin(), files.end(), SizeLess()), files.end());
	}
}

void DirectoryListing::Directory::getHashList(DirectoryListing::Directory::TTHSet& l) {
	for(auto d: directories)  
		d->getHashList(l);

	for(auto d: files) 
		l.insert(d->getTTH());
}
	
void DirectoryListing::getLocalPaths(const File* f, StringList& ret) {
	if(f->getParent()->getAdls() && (f->getParent()->getParent() == root || !isOwnList))
		return;

	string path;
	if (f->getParent()->getAdls())
		path = ((AdlDirectory*)f->getParent())->getFullPath();
	else
		path = getPath(f->getParent());

	ShareManager::getInstance()->getRealPaths(Util::toAdcFile(path + f->getName()), ret, Util::toInt(fileName));
}

void DirectoryListing::getLocalPaths(const Directory* d, StringList& ret) {
	if(d->getAdls() && (d->getParent() == root || !isOwnList))
		return;

	string path;
	if (d->getAdls())
		path = ((AdlDirectory*)d)->getFullPath();
	else
		path = getPath(d);
	ShareManager::getInstance()->getRealPaths(Util::toAdcFile(path), ret, Util::toInt(fileName));
}

int64_t DirectoryListing::Directory::getTotalSize(bool countAdls) {
	if(!isComplete())
		return partialSize;
	if(!countAdls && getAdls())
		return 0;
	
	int64_t x = getFilesSize();
	for(auto d: directories) {
		if(!countAdls && d->getAdls())
			continue;
		x += d->getTotalSize(getAdls());
	}
	return x;
}

size_t DirectoryListing::Directory::getTotalFileCount(bool countAdls) {
	if(!countAdls && getAdls())
		return 0;

	size_t x = getFileCount();
	for(auto d: directories) {
		if(!countAdls && d->getAdls())
			continue;
		x += d->getTotalFileCount(getAdls());
	}
	return x;
}

void DirectoryListing::Directory::clearAdls() {
	for(auto i = directories.begin(); i != directories.end();) {
		if((*i)->getAdls()) {
			delete *i;
			i = directories.erase(i);
		} else {
			++i;
		}
	}
}

string DirectoryListing::Directory::getPath() const {
	string tmp;
	//make sure to not try and get the name of the root dir
	if(getParent() && getParent()->getParent()){
		return getParent()->getPath() +  getName() + '\\';
	}
	return getName() + '\\';
}

int64_t DirectoryListing::Directory::getFilesSize() const {
	int64_t x = 0;
	for(auto f: files) {
		x += f->getSize();
	}
	return x;
}

uint8_t DirectoryListing::Directory::checkShareDupes() {
	uint8_t result = DUPE_NONE;
	bool first = true;
	for(auto d: directories) {
		result = d->checkShareDupes();
		if(dupe == DUPE_NONE && first)
			setDupe((DupeType)result);

		//full dupe with same type for non-dupe dir, change to partial (or pass partial dupes to upper level folder)
		else if((result == SHARE_DUPE || result == PARTIAL_SHARE_DUPE) && (dupe == DUPE_NONE || dupe == SHARE_DUPE) && !first)
			setDupe(PARTIAL_SHARE_DUPE);
		else if((result == QUEUE_DUPE || result == PARTIAL_QUEUE_DUPE) && (dupe == DUPE_NONE || dupe == QUEUE_DUPE) && !first)
			setDupe(PARTIAL_QUEUE_DUPE);

		//change to mixed dupe type
		else if((getDupe() == SHARE_DUPE || dupe == PARTIAL_SHARE_DUPE) && (result == QUEUE_DUPE || result == PARTIAL_QUEUE_DUPE))
			setDupe(SHARE_QUEUE_DUPE);
		else if((getDupe() == QUEUE_DUPE || dupe == PARTIAL_QUEUE_DUPE) && (result == SHARE_DUPE || result == PARTIAL_SHARE_DUPE))
			setDupe(SHARE_QUEUE_DUPE);

		else if (result == SHARE_QUEUE_DUPE)
			setDupe(SHARE_QUEUE_DUPE);

		first = false;
	}

	first = true;
	for(auto f: files) {
		//don't count 0 byte files since it'll give lots of partial dupes
		//of no interest
		if(f->getSize() > 0) {			
			//if it's the first file in the dir and no sub-folders exist mark it as a dupe.
			if(getDupe() == DUPE_NONE && f->getDupe() == SHARE_DUPE && directories.empty() && first)
				setDupe(SHARE_DUPE);
			else if(getDupe() == DUPE_NONE && f->isQueued() && directories.empty() && first)
				setDupe(QUEUE_DUPE);

			//if it's the first file in the dir and we do have sub-folders but no dupes, mark as partial.
			else if(getDupe() == DUPE_NONE && f->getDupe() == SHARE_DUPE && !directories.empty() && first)
				setDupe(PARTIAL_SHARE_DUPE);
			else if(getDupe() == DUPE_NONE && f->isQueued() && !directories.empty() && first)
				setDupe(PARTIAL_QUEUE_DUPE);
			
			//if it's not the first file in the dir and we still don't have a dupe, mark it as partial.
			else if(getDupe() == DUPE_NONE && f->getDupe() == SHARE_DUPE && !first)
				setDupe(PARTIAL_SHARE_DUPE);
			else if(getDupe() == DUPE_NONE && f->isQueued() && !first)
				setDupe(PARTIAL_QUEUE_DUPE);
			
			//if it's a dupe and we find a non-dupe, mark as partial.
			else if(getDupe() == SHARE_DUPE && f->getDupe() != SHARE_DUPE)
				setDupe(PARTIAL_SHARE_DUPE);
			else if(getDupe() == QUEUE_DUPE && !f->isQueued())
				setDupe(PARTIAL_QUEUE_DUPE);

			//if we find different type of dupe, change to mixed
			else if((getDupe() == SHARE_DUPE || getDupe() == PARTIAL_SHARE_DUPE) && f->isQueued())
				setDupe(SHARE_QUEUE_DUPE);
			else if((getDupe() == QUEUE_DUPE || getDupe() == PARTIAL_QUEUE_DUPE) && f->getDupe() == SHARE_DUPE)
				setDupe(SHARE_QUEUE_DUPE);

			first = false;
		}
	}
	return getDupe();
}

void DirectoryListing::checkShareDupes() {
	root->checkShareDupes();
	root->setDupe(DUPE_NONE); //never show the root as a dupe or partial dupe.
}

void DirectoryListing::addMatchADLTask() {
	tasks.addUnique(MATCH_ADL, nullptr);
	runTasks();
}

struct ListDiffTask : public Task {
	ListDiffTask(const string& aName, bool aOwnList) : name(aName), 
		ownList(aOwnList) { }

	string name;
	bool ownList;
};

void DirectoryListing::addListDiffTask(const string& aFile, bool aOwnList) {
	tasks.add(LISTDIFF, unique_ptr<Task>(new ListDiffTask(aFile, aOwnList)));
	runTasks();
}

struct PartialLoadingTask : public Task {
	PartialLoadingTask(const string& aXml, const string& aBaseDir, std::function<void ()> aF) : f(aF), xml(aXml), baseDir(aBaseDir) { }

	string xml;
	string baseDir;
	std::function<void ()> f;
};

void DirectoryListing::addPartialListTask(const string& aXml, const string& aBase, std::function<void ()> f) {
	tasks.add(REFRESH_DIR, unique_ptr<Task>(new PartialLoadingTask(aXml, Util::toAdcFile(aBase), f)));
	runTasks();
}

void DirectoryListing::addFullListTask(const string& aDir) {
	tasks.addUnique(LOAD_FILE, unique_ptr<Task>(new StringTask(aDir)));
	runTasks();
}

void DirectoryListing::addQueueMatchTask() {
	tasks.addUnique(MATCH_QUEUE, nullptr);
	runTasks();
}

void DirectoryListing::close() {
	tasks.add(CLOSE, nullptr);
	runTasks();
}

struct SearchTask : public Task {
	SearchTask(const string& aSearchString, int64_t aSize, int aTypeMode, int aSizeMode, const StringList& aExtList, const string& aDir) : searchString(aSearchString), 
		size(aSize), typeMode(aTypeMode), sizeMode(aSizeMode), extList(aExtList), directory(aDir) { }

	string searchString;
	int64_t size;
	int typeMode;
	int sizeMode;
	StringList extList;
	string directory;
};

void DirectoryListing::addSearchTask(const string& aSearchString, int64_t aSize, int aTypeMode, int aSizeMode, const StringList& aExtList, const string& aDir) {
	tasks.add(SEARCH, unique_ptr<Task>(new SearchTask(aSearchString, aSize, aTypeMode, aSizeMode, aExtList, aDir)));
	runTasks();
}

struct DirDownloadTask : public Task {
	DirDownloadTask(DirectoryListing::Directory* aDir, const string& aTarget, TargetUtil::TargetType aTargetType, bool aIsSizeUnknown, QueueItemBase::Priority aPrio) : dir(aDir), 
		target(aTarget), targetType(aTargetType), isSizeUnknown(aIsSizeUnknown), prio(aPrio) { }

	DirectoryListing::Directory* dir;
	QueueItemBase::Priority prio;
	bool isSizeUnknown;
	TargetUtil::TargetType targetType;
	string target;
};

void DirectoryListing::addDirDownloadTask(Directory* aDir, const string& aTarget, TargetUtil::TargetType aTargetType, bool isSizeUnknown, QueueItemBase::Priority prio) {
	tasks.add(DIR_DOWNLOAD, unique_ptr<Task>(new DirDownloadTask(aDir, aTarget, aTargetType, isSizeUnknown, prio)));
	runTasks();
}

void DirectoryListing::addFilterTask() {
	if (tasks.addUnique(FILTER, nullptr))
		runTasks();
	else
		typingFilter = true;
}

void DirectoryListing::runTasks() {
	if (!running.test_and_set()) {
		join();
		try {
			start();
		} catch(const ThreadException& /*e*/) {
			LogManager::getInstance()->message("DirListThread error", LogManager::LOG_WARNING);
			running.clear();
		}
	}
}

int DirectoryListing::run() {
	for (;;) {
		TaskQueue::TaskPair t;
		if (!tasks.getFront(t))
			break;

		ScopedFunctor([this] { tasks.pop_front(); });

		auto waitFinished = [this] {
			while (waiting)
				sleep(50);
		};

		try {
			int64_t start = GET_TICK();
			
			if (t.first == LISTDIFF) {
				if (isOwnList && partialList) {
					auto mis = ShareManager::getInstance()->generatePartialList("/", true, Util::toInt(fileName));
					if (mis) {
						loadXML(*mis, true);
						partialList = false;
					} else {
						throw CSTRING(FILE_NOT_AVAILABLE);
					}
				}

				auto ldt = static_cast<ListDiffTask*>(t.second);
				DirectoryListing dirList(hintedUser, false, ldt->name, false, ldt->ownList);
				dirList.loadFile();

				root->filterList(dirList);
				fire(DirectoryListingListener::LoadingFinished(), start, Util::emptyString, false, true, false);
			} else if(t.first == MATCH_ADL) {
				root->clearAdls(); //not much to check even if its the first time loaded without adls...
				ADLSearchManager::getInstance()->matchListing(*this);
				fire(DirectoryListingListener::LoadingFinished(), start, Util::emptyString, false, true, false);
			} else if(t.first == FILTER) {
				for(;;) {
					typingFilter = false;
					sleep(500);
					if (!typingFilter)
						break;
				}

				fire(DirectoryListingListener::Filter());
			}else if(t.first == LOAD_FILE) {
				partialList = false;

				waiting = true;
				fire(DirectoryListingListener::LoadingStarted(), false);
				bool reloading = !root->directories.empty();

				if (reloading) {
					//wait for the gui to disable the window
					waitFinished();

					root->clearAll();
					baseDirs.clear();
				}

				loadFile();
				
				if(matchADL) {
					fire(DirectoryListingListener::UpdateStatusMessage(), CSTRING(MATCHING_ADL));
					ADLSearchManager::getInstance()->matchListing(*this);
				}

				fire(DirectoryListingListener::LoadingFinished(), start, static_cast<StringTask*>(t.second)->str, reloading, true, false);
				reloading = false;
			} else if (t.first == REFRESH_DIR) {
				if (!partialList)
					continue;

				auto lt = static_cast<PartialLoadingTask*>(t.second);

				bool reloading = false;
				auto bd = baseDirs.find(Text::toLower(lt->baseDir));
				if (bd != baseDirs.end()) {
					reloading = bd->second.second;
					if (reloading) {
						waiting = true;
						fire(DirectoryListingListener::LoadingStarted(), false);

						//wait for the gui to disable the window
						waitFinished();


						if (lt->baseDir.empty()) {
							baseDirs.clear();
							root->clearAll();
							root->setComplete();
						} else {
							auto cur = findDirectory(Util::toNmdcFile(lt->baseDir));
							if (cur && (!cur->directories.empty() || !cur->files.empty())) {
								//we have been here already, just reload all items
								cur->clearAll();

								//also clean the visited dirs
								for(auto i = baseDirs.begin(); i != baseDirs.end(); ) {
									if (AirUtil::isSub(i->first, lt->baseDir)) {
										baseDirs.erase(i++);
									} else {
										i++;
									}
								}
							}
						}
					}
				}

				waiting = true;
				if (!reloading) {
					fire(DirectoryListingListener::LoadingStarted(), true);
					waitFinished();
				}
				
				int dirsLoaded = 0;
				if (isOwnList) {
					auto mis = ShareManager::getInstance()->generatePartialList(lt->baseDir, false, Util::toInt(fileName));
					if (mis) {
						dirsLoaded = loadXML(*mis, true, lt->baseDir);
					} else {
						throw Exception(CSTRING(FILE_NOT_AVAILABLE));
					}
				} else {
					dirsLoaded = updateXML(lt->xml, lt->baseDir);
				}

				waiting = true;
				bool useGuiThread = !reloading && dirsLoaded < 5000;
				if (!useGuiThread && !reloading) {
					fire(DirectoryListingListener::LoadingStarted(), false);
					waitFinished();
				}

				waiting = true;
				fire(DirectoryListingListener::LoadingFinished(), start, Util::toNmdcFile(lt->baseDir), reloading && lt->baseDir == "/", lt->f == nullptr, useGuiThread);
				if (lt->f) {
					lt->f();
				}

				if (useGuiThread) {
					waitFinished();
				}
			} else if (t.first == CLOSE) {
				//delete this;
				fire(DirectoryListingListener::Close());
				return 0;
			} else if (t.first == MATCH_QUEUE) {
				int matches=0, newFiles=0;
				BundleList bundles;
				QueueManager::getInstance()->matchListing(*this, matches, newFiles, bundles);
				fire(DirectoryListingListener::QueueMatched(), AirUtil::formatMatchResults(matches, newFiles, bundles, false));
			} else if (t.first == DIR_DOWNLOAD) {
				auto dli = static_cast<DirDownloadTask*>(t.second);
				downloadDir(dli->dir, dli->target, dli->targetType, dli->isSizeUnknown , dli->prio);
			} else if (t.first == SEARCH) {
				lastResult = GET_TICK();
				maxResultCount = 0;
				curResultCount = 0;
				searchResults.clear();

				auto s = static_cast<SearchTask*>(t.second);
				fire(DirectoryListingListener::SearchStarted());

				auto search = AdcSearch::getSearch(s->searchString, Util::emptyString, s->size, s->typeMode, s->sizeMode, s->extList, AdcSearch::MATCH_NAME, true);
				if (search)
					curSearch.reset(search);

				if (isOwnList && partialList) {
					SearchResultList results;
					try {
						ShareManager::getInstance()->search(results, *curSearch, 50, Util::toInt(fileName), CID(), s->directory);
					} catch (...) { }

					for (const auto& sr: results)
						searchResults.insert(sr->getFile());

					curResultCount = searchResults.size();
					maxResultCount = searchResults.size();
					endSearch(false);
				} else if (partialList && !hintedUser.user->isNMDC()) {
					SearchManager::getInstance()->addListener(this);

					searchToken = Util::toString(Util::rand());
					ClientManager::getInstance()->directSearch(hintedUser, s->sizeMode, s->size, s->typeMode, s->searchString, searchToken, s->extList, s->directory);

					TimerManager::getInstance()->addListener(this);
				} else {
					const auto dir = (s->directory.empty()) ? root : findDirectory(Util::toNmdcFile(s->directory), root);
					if (dir)
						dir->search(searchResults, *curSearch, 100);

					curResultCount = searchResults.size();
					maxResultCount = searchResults.size();
					endSearch(false);
				}
			}
		} catch(const AbortException&) {
			fire(DirectoryListingListener::LoadingFailed(), Util::emptyString);
			break;
		} catch(const ShareException& e) {
			fire(DirectoryListingListener::LoadingFailed(), e.getError());
		} catch(const Exception& e) {
			fire(DirectoryListingListener::LoadingFailed(), ClientManager::getInstance()->getNick(hintedUser.user, hintedUser.hint) + ": " + e.getError());
		}
	}

	running.clear();
	return 0;
}

void DirectoryListing::on(SearchManagerListener::SR, const SearchResultPtr& aSR) noexcept {
	if (compare(aSR->getToken(), searchToken) == 0) {
		lastResult = GET_TICK();

		string path;
		if (supportsASCH()) {
			path = aSR->getFile();
		} else {
			//convert the regular search results
			path = aSR->getType() == SearchResult::TYPE_DIRECTORY ? Util::getParentDir(aSR->getFile()) : aSR->getFilePath();
		}

		auto insert = searchResults.insert(path);
		if (insert.second)
			curResultCount++;

		if (maxResultCount == curResultCount)
			lastResult = 0; //we can call endSearch only from the TimerManagerListener thread
	}
}

void DirectoryListing::on(ClientManagerListener::DirectSearchEnd, const string& aToken, int aResultCount) noexcept {
	if (compare(aToken, searchToken) == 0) {
		maxResultCount = aResultCount;
		if (maxResultCount == curResultCount)
			endSearch(false);
	}
}

void DirectoryListing::on(TimerManagerListener::Second, uint64_t aTick) noexcept {
	if (curResultCount == 0) {
		if (lastResult + 5000 < aTick)
			endSearch(true);
	} else if (lastResult + 1000 < aTick) {
		endSearch(false);
	}
}

void DirectoryListing::endSearch(bool timedOut /*false*/) {
	SearchManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);

	if (curResultCount == 0) {
		curSearch = nullptr;
		fire(DirectoryListingListener::SearchFailed(), timedOut);
	} else {
		curResult = searchResults.begin();
		changeDir();
	}
}

void DirectoryListing::changeDir(bool reload) {
	auto path = *curResult;
	if (!partialList) {
		fire(DirectoryListingListener::ChangeDirectory(), path, true);
	} else {
		const auto dir = (path == Util::emptyString) ? root : findDirectory(path, root);
		if (dir && dir->isComplete() && !reload) {
			fire(DirectoryListingListener::ChangeDirectory(), path, true);
		} else if (isOwnList) {
			auto mis = ShareManager::getInstance()->generatePartialList(Util::toAdcFile(path), false, Util::toInt(fileName));
			if (mis) {
				loadXML(*mis, true, Util::toAdcFile(path));
				fire(DirectoryListingListener::LoadingFinished(), 0, path, false, true, true);
			} else {
				//might happen if have refreshed the share meanwhile
				fire(DirectoryListingListener::LoadingFailed(), CSTRING(FILE_NOT_AVAILABLE));
			}
		} else {
			try {
				QueueManager::getInstance()->addList(hintedUser, QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_CLIENT_VIEW, path);
			} catch (...) { }
		}
	}
}

bool DirectoryListing::nextResult(bool prev) {
	if (prev) {
		if (curResult == searchResults.begin()) {
			return false;
		}
		advance(curResult, -1);
	} else {
		if (static_cast<size_t>(distance(searchResults.begin(), curResult)) == searchResults.size()-1) {
			return false;
		}
		advance(curResult, 1);
	}

	changeDir();
	return true;
}

bool DirectoryListing::isCurrentSearchPath(const string& path) {
	if (searchResults.empty())
		return false;

	return *curResult == path;
}

} // namespace dcpp
