#include "ServMonitor.h"

using namespace std;

bool CServMonitor::Init(const MonitorCfg &cfg, CMonitorEvent *listener) {
    mCfg = cfg;
    mListener = listener;
    mInit = false;
    return true;
}

bool CServMonitor::Start() {
    if (mInit)
    {
        return false;
    }

    return mTcpClient.InitClient(mCfg.mServIp, LOG_PORT, this, 3000);
}

bool CServMonitor::AddPath(const std::mstring &path) {
    return true;
}

bool CServMonitor::Stop() {
    return true;
}

bool CServMonitor::IsStart() {
    return true;
}

list<mstring> CServMonitor::GetPathSet() const {
    return mPathSet;
}

void CServMonitor::OnSingleResult(const LpResult &result) {
    switch (result.mCommand) {
        case  em_cmd_log:
            {
                LpLogInfo logInfo;
                CLogProtocol::GetInst()->DecodeLog(result.mContent, logInfo);

                mListener->OnLogReceived(logInfo.mFilePath, logInfo.mContent);
            }
            break;
        default:
            break;
    }
}

void CServMonitor::OnClientConnect(CTcpClient &client) {
    LpViewRegisger abc;
    abc.mVersion = LOGVIEW_VERSION;

    string packet;
    client.Send(CLogProtocol::GetInst()->EncodeRegister(abc, packet));
}

void CServMonitor::OnClientRecvData(CTcpClient &client, const std::string &strRecved, std::string &strResp) {
    mLogCache += strRecved;

    list<LpResult> result;
    CLogProtocol::GetInst()->GetRecvResult(mLogCache, result);

    if (result.empty())
    {
        return;
    }

    for (list<LpResult>::const_iterator it = result.begin() ; it != result.end() ; it++)
    {
        OnSingleResult(*it);
    }
}

void CServMonitor::OnClientSocketErr(CTcpClient &client) {
}