#include <WinSock2.h>
#include "LocalMonitor.h"
#include "WinFileNoitfy.h"
#include "../LogReceiver.h"
#include <LogLib/LogUtil.h>
#include <LogLib/StrUtil.h>

using namespace std;

CLocalMonitor::CLocalMonitor() {
    mInit = false;
    mListener = NULL;
    mCfg = NULL;
}

CLocalMonitor::~CLocalMonitor() {
}

CLocalMonitor *CLocalMonitor::GetInst() {
    static CLocalMonitor *sPtr = NULL;

    if (NULL == sPtr)
    {
        sPtr = new CLocalMonitor();
    }
    return sPtr;
}

bool CLocalMonitor::Init(CMonitorEvent *listener) {
    mListener = listener;
    CWinFileNotify::GetInst()->InitNotify();
    mInit = false;
    mCfg = NULL;
    return true;
}

bool CLocalMonitor::Run(const LogServDesc *servDesc) {
    list<mstring> added;
    list<mstring> deled;

    if (!mInit || !mCfg)
    {
        added = servDesc->mLocalServDesc.mPathSet;
    } else {
        list<mstring>::const_iterator it1, it2;
        const list<mstring> &set1 = servDesc->mLocalServDesc.mPathSet;
        const list<mstring> &set2 = mCfg->mLocalServDesc.mPathSet;

        bool flag = false;
        for (it1 = set1.begin() ; it1 != set1.end() ; it1++)
        {
            flag = false;
            for (it2 = set2.begin() ; it2 != set2.end() ; it2++)
            {
                if (*it1 == *it2)
                {
                    flag = true;
                    break;
                }
            }

            if (!flag)
            {
                added.push_back(*it1);
            }
        }

        for (it1 = set2.begin() ; it1 != set2.end() ; it1++)
        {
            flag = false;
            for (it2 = set1.begin() ; it2 != set1.end() ; it2++)
            {
                if (*it1 == *it2)
                {
                    flag = true;
                    break;
                }
            }

            if (!flag)
            {
                deled.push_back(*it1);
            }
        }
    }

    list<mstring>::const_iterator it;
    for (it = added.begin() ; it != added.end() ; it++)
    {
        AddPath(*it);
    }

    for (it = deled.begin() ; it != deled.end() ; it++)
    {
        DelPath(*it);
    }
    mCfg = servDesc;
    return true;
}

bool CLocalMonitor::Stop() {
    return true;
}

bool CLocalMonitor::AddPath(const mstring &path) {
    if (IsFileInCache(path))
    {
        return false;
    }

    DWORD attr = GetFileAttributesA(path.c_str());
    if (INVALID_FILE_ATTRIBUTES == attr)
    {
        return false;
    }

    if (!(FILE_ATTRIBUTE_DIRECTORY & attr))
    {
        return false;
    }
    DWORD t1 = GetTickCount();
    EnumFiles(path, true, FileEnumProc, 0);
    DWORD t2 = GetTickCount() - t1;

    HFileNotify h = CWinFileNotify::GetInst()->Register(path, "log;txt",-1, FileNotify);
    mPathSet[path] = h;
    return true;
}

bool CLocalMonitor::DelPath(const std::mstring &path) {
    if (!IsFileInCache(path))
    {
        return false;
    }

    CWinFileNotify::GetInst()->UnRegister(mPathSet[path]);
    mPathSet.erase(path);
    return true;
}

list<mstring> CLocalMonitor::GetPathSet() const {
    return list<mstring> ();
}

bool CLocalMonitor::IsRunning() {
    return true;
}

CLocalMonitor::LocalLogCache *CLocalMonitor::GetFileCache(const mstring &filePath) {
    map<mstring, LocalLogCache *>::iterator it = mLogCache.find(filePath);
    if (mLogCache.end() == it)
    {
        LocalLogCache *newCache = new LocalLogCache();
        newCache->mFilePath = filePath;
        mLogCache[filePath] = newCache;
        return newCache;
    } else {
        return it->second;
    }
}

void CLocalMonitor::OnLogReceived(LocalLogCache *cache) {
    if (cache->mLastCache.empty())
    {
        return;
    }

    mstring lineEnd = "\n";
    if (cache->mEncodeType == em_text_unicode_le)
    {
        lineEnd.assign((const char *)L"\n", 2);
    }

    size_t curPos = 0;
    size_t lastPos = 0;
    while (true) {
        curPos = cache->mLastCache.find(lineEnd, lastPos);
        if (string::npos == curPos) {
            break;
        }

        if (curPos > lastPos)
        {
            string lineStr = cache->mLastCache.substr(lastPos, curPos - lastPos );
            if (!lineStr.empty())
            {
                if (cache->mEncodeType == em_text_unicode_le)
                {
                    ustring wstr((const wchar_t *)lineStr.c_str(), lineStr.size() / 2);
                    lineStr = WtoA(wstr);
                } else if (cache->mEncodeType == em_text_utf8)
                {
                    lineStr = UtoA(lineStr);
                }

                mListener->OnLogReceived(cache->mFilePath, lineStr);
            }
        }

        lastPos = curPos + lineEnd.size();
    }

    if (lastPos > 0)
    {
        cache->mLastCache.erase(0, lastPos);
    }
}

bool CLocalMonitor::IsFileInCache(const mstring &filePath) const {
    return (mPathSet.end() != mPathSet.find(filePath));
}

bool CLocalMonitor::FileEnumProc(bool isDir, const char *filePath, void *param) {
    if (isDir)
    {
        return true;
    }

    if (IsLogFile(filePath))
    {
        LocalLogCache *newCache = new LocalLogCache();
        newCache->mFilePath = filePath;
        FILE *fp = fopen(filePath, "rb");

        if (NULL != fp)
        {
            fseek(fp, 0, SEEK_END);
            long size = ftell(fp);
            newCache->mLastPos = size;
            newCache->mFileSize = size;
            fclose(fp);
        }

        int bomLen = 0;
        newCache->mEncodeType = GetInst()->GetFileEncodeType(filePath);
        GetInst()->mLogCache[newCache->mFilePath] = newCache;
    }
    return true;
}

void CLocalMonitor::FileNotify(const char *filePath, unsigned int mask) {
    AutoLocker locker(GetInst());

   if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(filePath))
   {
       return;
   }

   if (!IsLogFile(filePath))
   {
       return;
   }

   LocalLogCache *cache = GetInst()->GetFileCache(filePath);
   FILE *fp = fopen(filePath, "rb");
   if (!fp)
   {
       return;
   }
   fseek(fp, 0, SEEK_END);
   int size = ftell(fp);
   fclose(fp);

   if (0 == size)
   {
       int dd = 1234;
   }

   long lastPos = cache->mLastPos;
   if (cache->mFileSize < (DWORD)size)
   {
       cache->mLastCache += GetInst()->GetFileContent(filePath, lastPos);

       cache->mLastPos = size;
       cache->mFileSize = size;
   }

   if (cache->mEncodeType == em_text_unknown || cache->mEncodeType == em_text_no_unicode)
   {
       cache->mEncodeType = GetInst()->GetFileEncodeType(filePath, lastPos);
   }
   GetInst()->OnLogReceived(cache);
}

TextEncodeType CLocalMonitor::GetFileEncodeType(const mstring &filePath, long lastPos) const {
    TextEncodeType type = em_text_unknown;

    int bomLen = 0;
    type = CTextDecoder::GetInst()->GetFileType(filePath, bomLen);

    if (0 != bomLen)
    {
        return type;
    }

    //从文件内容分析
    mstring content;
    FILE *fp = fopen(filePath.c_str(), "rb");
    if (!fp)
    {
        return type;
    }

    int readCount = 0;
    char buffer[1024];
    fseek(fp, 0, SEEK_SET);

    while (true) {
        readCount = fread(buffer, 1, 1024, fp);

        if (readCount <= 0)
        {
            break;
        }
        content.append(buffer, readCount);
    }
    fclose(fp);

    type = CTextDecoder::GetInst()->GetTextType(content);
    return type;
}

mstring CLocalMonitor::GetFileContent(const mstring &filePath, long lastPos) {
    if (0 == lastPos)
    {
        int bomLen = 0;
        CTextDecoder::GetInst()->GetFileType(filePath, bomLen);
        lastPos += bomLen;
    }

    mstring result;
    FILE *fp = fopen(filePath.c_str(), "rb");
    if (!fp)
    {
        return result;
    }

    fseek(fp, lastPos, SEEK_SET);
    char buffer[1024];
    int count = 0;

    while (true) {
        count = fread(buffer, 1, 1024, fp);

        if (count <= 0)
        {
            break;
        }
        result.append(buffer, count);
    }
    fclose(fp);
    return result;
}