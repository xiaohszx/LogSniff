#include "LogProtocol.h"
#include <string.h>
#include <stdio.h>
#include "json/json.h"
#include "LogUtil.h"

#ifndef __linux__
#include <Windows.h>
#include "StrUtil.h"
#else
#endif //__linux__

using namespace std;
using namespace Json;

CLogProtocol *CLogProtocol::GetInst() {
    static CLogProtocol *sPtr = NULL;

    if (sPtr == NULL)
    {
        sPtr = new CLogProtocol();
    }
    return sPtr;
}

string &CLogProtocol::EncodeRegister(const LpViewRegisger &info, string &outStr) {
    Value content;
    content["version"] = info.mVersion;

    string str = FastWriter().write(content);
    LpHeader header;
    header.mCommand = em_cmd_register;
    header.mLength = sizeof(header) + str.size();

    outStr.append((const char *)&header, sizeof(header));
    outStr += str;
    return outStr;
}

LpViewRegisger &CLogProtocol::DecodeRegister(const std::string &packet, LpViewRegisger &info) {
    size_t pos = sizeof(LpHeader);

    Value content;
    Reader().parse(packet.c_str() + pos, content);

    if (content.type() != objectValue)
    {
        return info;
    }
    info.mVersion = content["version"].asString();
    return info;
}

string &CLogProtocol::EncodeLog(const string &logPath, const string &logContent, string &outStr) {
    string content;
    content.reserve(logContent.size() + 256);

    unsigned int length = logPath.size();
    content.append((const char *)&length, sizeof(length));
    content.append(logPath);

    length = logContent.size();
    content.append((const char *)&length, sizeof(length));
    content.append(logContent);

    outStr.reserve(content.size() + sizeof(LpHeader));
    LpHeader header;
    header.mCommand = em_cmd_log;
    header.mLength = (sizeof(LpHeader) + content.size());
    outStr.append((const char *)&header, sizeof(LpHeader));
    outStr += content;
    return outStr;
}

LpLogInfo &CLogProtocol::DecodeLog(const string &packet, LpLogInfo &outInfo) {
    outInfo.mContent.reserve(packet.size());
    size_t pos = 0;
    unsigned int length = 0;

    memcpy(&length, packet.c_str() + pos, sizeof(length));
    pos += 4;
    outInfo.mFilePath.assign(packet.c_str() + pos, length);
    pos += length;
    memcpy(&length, packet.c_str() + pos, sizeof(length));
    pos += 4;
    outInfo.mContent.assign(packet.c_str() + pos, length);
    return outInfo;
}

#ifdef __linux__
string &CLogProtocol::EncodeDesc(const string &unique, const list<string> &pathSet, time_t startTime, string &outStr) {
    Value content;
    Value ipSet(arrayValue);
    Value pathJson(arrayValue);
    size_t pos1 = 0;
    size_t pos2 = 0;

    //
    content["startCount"] = (int)(time(0) - startTime);
    string desc = ExecProc("ifconfig");
    while (string::npos != (pos1 = desc.find("inet addr:", pos1))) {
        pos1 += strlen("inet addr:");
        pos2 = desc.find(" ", pos1 + 1);
        ipSet.append(desc.substr(pos1, pos2 - pos1));

        pos1 = pos2 + 1;
    }
    content["ipSet"] = ipSet;
    content["unique"] = unique;

    desc = ExecProc("cat /etc/issue");
    pos1 = desc.find("\n");
    if (string::npos != pos1)
    {
        content["system"] = desc.substr(0, pos1);
    }
    else {
        content["system"] = desc;
    }

    for (list<string>::const_iterator it = pathSet.begin() ; it != pathSet.end() ; it++)
    {
        pathJson.append(*it);
    }
    content["pathSet"] = pathJson;
    content["cmd"] = GROUT_MSG_DESC;
    outStr = FastWriter().write(content);
    return outStr;
}
#else
LpServDesc &CLogProtocol::DecodeDesc(const string &packet, LpServDesc &outDesc) {
    string jsonStr = packet;

    Value content;
    Reader().parse(jsonStr, content);

    Value ipSet = content["ipSet"];
    Value sys = content["system"];
    Value pathSet = content["pathSet"];

    outDesc.mUnique = content["unique"].asString();
    size_t i = 0;
    for (i = 0 ; i < ipSet.size() ; i++)
    {
        outDesc.mIpSet.push_back(ipSet[i].asString());
    }

    for (i = 0 ; i < pathSet.size() ; i++)
    {
        outDesc.mPathSet.push_back(pathSet[i].asString());
    }
    outDesc.mSystem = sys.asString();

    int timeCount = content["startCount"].asInt();
    SYSTEMTIME time = {0};
    GetLocalTime(&time);

    FILETIME fTime = {0};
    SystemTimeToFileTime(&time, &fTime);
    //100����
    *((ULONGLONG *)(&fTime)) -= timeCount * 1000 * 10000;
    FileTimeToSystemTime(&fTime, &time);
    outDesc.mStartTime = FormatA(
        "%04d-%02d-%02d %02d:%02d:%02d",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond
        );
    return outDesc;
}
#endif //__linux__

#ifdef __linux__
string CLogProtocol::ExecProc(const string &proc) {
    FILE *fstream = NULL;
    char buff[1024];

    if(NULL == (fstream = popen(proc.c_str(),"r")))
    {
        return "";
    }

    string result;
    result.reserve(1024);
    const char *p = NULL;
    while (NULL != (p = fgets(buff, sizeof(buff), fstream))) {
        result += p;
    }

    pclose(fstream);
    return result;
}
#endif //__linux__

int CLogProtocol::GetRecvResult(std::string &packet, std::list<LpResult> &result) {
    LpHeader header;
    while (true) {
        if (packet.size() < sizeof(LpHeader))
        {
            break;
        }

        memcpy(&header, packet.c_str(), sizeof(LpHeader));
        if (0 != memcmp((const void *)&header.mMagic, "logs", 4))
        {
            dp("exit 1");
            packet.clear();
            result.clear();
            return 0;
        }

        if (packet.size() < header.mLength)
        {
            break;
        }

        LpResult tmp;
        tmp.mCommand = header.mCommand;

        if (header.mLength > sizeof(LpHeader))
        {
            tmp.mContent = packet.substr(sizeof(LpHeader), header.mLength - sizeof(LpHeader));
        }

        result.push_back(tmp);
        packet.erase(0, header.mLength);
    }
    dp("ret:%d", result.size());
    return (int)result.size();
}

CLogProtocol::CLogProtocol() {}

CLogProtocol::~CLogProtocol() {}