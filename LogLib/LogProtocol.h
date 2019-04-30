#pragma once
#include <string>
#include <list>
#include <string.h>

#define GROUP_ADDR   "228.4.5.6"
#define GROUP_PORT   7881

#define GROUP_MSG_SCAN  "scan"
#define GROUT_MSG_DESC  "desc"

enum LpCommand {
    em_cmd_register = 0,
    em_cmd_version,
    em_cmd_desc,
    em_cmd_log
};

//
struct LpHeader {
    unsigned int mMagic;
    unsigned int mCommand;
    unsigned int mLength;

    LpHeader() {
        memcpy(&mMagic, "logs", sizeof(unsigned int));
        mCommand = 0, mLength = 0;
    }
};

//
struct LpLogStruct {
    unsigned int mLength1;
    const char *mContent1;
    unsigned int mLength2;
    const char *mContent2;
    unsigned int mLength3;
    const char *mContent3;
};

struct LpServDesc {
    std::string mStartTime;
    std::list<std::string> mIpSet;
    std::string mSystem;
    std::list<std::string> mPathSet;
};

struct LpLogInfo {
    std::string mFilePath;
    std::string mContent;
};

struct LpResult {
    unsigned int mCommand;
    std::string mContent;
};

class CLogProtocol {
public:
    static CLogProtocol *GetInst();
    std::string &EncodeLog(const std::string &logPath, const std::string &logContent, std::string &outStr);
    LpLogInfo &DecodeLog(const std::string &packet, LpLogInfo &outInfo);

    //group msg
    std::string &EncodeDesc(const std::list<std::string> &pathSet, std::string &outStr);
    LpServDesc &DecodeDesc(const std::string &packet, LpServDesc &outDesc);

    int GetRecvResult(std::string &packet, std::list<LpResult> &result);
private:
    CLogProtocol();
    virtual ~CLogProtocol();

public:
    std::string ExecProc(const std::string &proc);
};