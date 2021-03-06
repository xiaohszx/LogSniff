#include "ScriptEngine.h"
#include <vector>
#include <Shlwapi.h>
#include <LogLib/StrUtil.h>
#include "../SyntaxHlpr/SyntaxDef.h"
#include <LogLib/LogUtil.h>

using namespace std;

CScriptEngine::CScriptEngine() {
    mSplitStrSet.push_back("&&");
    mSplitStrSet.push_back("||");
    mSplitStrSet.push_back("(");
    mSplitStrSet.push_back(")");
}

CScriptEngine::~CScriptEngine() {
}

void CScriptEngine::ScriptCleanUp(mstring &script) const {
    mstring tmp;
    size_t lastPos = 0;
    bool spaceFlag = false;

    for (size_t i = 0 ; i < script.size() ;)
    {
        size_t j = 0;
        for (j = 0 ; j < mSplitStrSet.size() ; j++)
        {
            if (0 == script.comparei(mSplitStrSet[j], i))
            {
                break;
            }
        }

        if (j == mSplitStrSet.size())
        {
            tmp += script[i];
            i++;
        } else {
            tmp.trimright();
            tmp += mSplitStrSet[j];
            i += mSplitStrSet[j].size();

            while (i < script.size()) {
                char c = script[i];
                if (c == ' ' || c == '\t')
                {
                    i++;
                } else {
                    break;
                }
            }
        }
    }
    script = tmp;
}

//Compile Rule Without Bracket
vector<CScriptEngine::FilterRule> CScriptEngine::SimpleCompile(const mstring &script) const {
    vector<FilterRule> result;

    vector<mstring> strSet;
    size_t lastPos = 0;
    size_t i = 0;
    for (i = 0 ; i < script.size() ;) {
        if (i < script.size() - 1)
        {
            if (0 == StrCmpNA(script.c_str() + i, "&&", 2) || 0 == StrCmpNA(script.c_str() + i, "||", 2))
            {
                if (i > lastPos)
                {
                    mstring sub = script.substr(lastPos, i - lastPos);
                    strSet.push_back(sub);
                    strSet.push_back(script.substr(i, 2));
                    i = i + 2;
                    lastPos = i;
                    continue;
                }
            }
        }
        i++;
    }

    if (lastPos <= (script.size() - 1))
    {
        strSet.push_back(script.substr(lastPos, script.size() - lastPos));
    }

    //Syntax Check
    for (i = 0 ; i < strSet.size() ; i++) {
        if (i % 2 != 0)
        {
            if (strSet[i] != "&&" && strSet[i] != "||")
            {
                throw (ScriptException("�﷨����"));
                return vector<FilterRule>();
            }
        }

        if (i % 2 == 0)
        {
            if (strSet[i] == "&&" || strSet[i] == "||")
            {
                throw (ScriptException("�﷨����"));
                return vector<FilterRule>();
            }
        }
    }

    //Push Rule Node To Result
    int mode = 0;   //0:&&, 1:||
    FilterRule tmp;
    for(i = 0 ; i < strSet.size() ; i++) {
        string cur = strSet[i];
        if (i % 2 == 0)
        {
            bool varFlag = false;
            bool exclude = false;
            vector<FilterRule> varContent;

            map<mstring, vector<FilterRule>>::const_iterator it;
            if (mVarSet.end() != (it = mVarSet.find(cur)))
            {
                varFlag = true;
                varContent = it->second;
            } else {
                if (cur[0] == '!')
                {
                    exclude = true;
                    cur.erase(0, 1);
                }
            }

            if (0 == mode)
            {
                // &&
                if (result.empty())
                {
                    if (varFlag)
                    {
                        result = varContent;
                    } else {
                        if (exclude)
                        {
                            tmp.mExclude.insert(cur);
                        } else {
                            tmp.mInclude.insert(cur);
                        }
                        result.push_back(tmp);
                    }
                } else {
                    if (varFlag)
                    {
                        result = CalAndResult(result, varContent);
                    } else {
                        if (exclude)
                        {
                            result[result.size() - 1].mExclude.insert(cur);
                        } else {
                            result[result.size() - 1].mInclude.insert(cur);
                        }
                    }
                }
            } else if (1 == mode)
            {
                // ||
                if (varFlag)
                {
                    result = CalOrResult(result, varContent);
                } else {
                    tmp.Clear();
                    if (exclude)
                    {
                        tmp.mExclude.insert(cur);
                    } else {
                        tmp.mInclude.insert(cur);
                    }
                    result.push_back(tmp);
                }
            }
        } else {
            if (strSet[i] == "&&")
            {
                mode = 0;
            } else {
                mode = 1;
            }
        }
    }
    return result;
}

// (a || b) && (c || d)->(a && c) || (a && d) || (b && c) || (b && d)
// (a || c) || (a || d)
vector<CScriptEngine::FilterRule> CScriptEngine::CalAndResult(const vector<CScriptEngine::FilterRule> &a, const vector<CScriptEngine::FilterRule> &b) const {
    vector<FilterRule> result;

    size_t i = 0, j = 0;
    for (i = 0 ; i < a.size() ; i++)
    {
        const FilterRule &t1 = a[i];
        for (j = 0 ; j < b.size() ; j++)
        {
            const FilterRule &t2 = b[j];
            FilterRule tmp = t1;
            tmp.mInclude.insert(t2.mInclude.begin(), t2.mInclude.end());
            tmp.mExclude.insert(t2.mExclude.begin(), t2.mExclude.end());
            result.push_back(tmp);
        }
    }
    return result;
}

vector<CScriptEngine::FilterRule> CScriptEngine::CalOrResult(const vector<CScriptEngine::FilterRule> &a, const vector<CScriptEngine::FilterRule> &b) const {
    vector<FilterRule> result;
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

void CScriptEngine::SetRuleStyle() {
    mRuleStyle.clear();

    int index = 0;
    size_t i = 0, j = 0, k = 0;
    int styleIndex = STYLE_LOG_KEYWORD_BASE;
    for (i = 0 ; i < mRuleSet.size() ; i++)
    {
        const set<mstring> &tmp = mRuleSet[i].mInclude;
        for (set<mstring>::const_iterator j = tmp.begin() ; j != tmp.end() ; j++)
        {
            const mstring &keyWord = *j;

            if (mRuleStyle.end() == mRuleStyle.find(keyWord))
            {
                mRuleStyle[keyWord] = styleIndex++;
            }
        }
    }
}

bool CScriptEngine::Compile(const mstring &str) {
    mstring script = str;
    ScriptCleanUp(script);

    if (str.empty())
    {
        ClearCache();
        return true;
    }

    //Check Syntax For Bracket
    mVarSet.clear();
    list<size_t> st1;
    size_t varIndex = 0;
    size_t i = 0;
    for (i = 0 ; i < script.size() ;)
    {
        char c = script[i];
        if (c == '(')
        {
            st1.push_back(i);
        }

        if (c == ')')
        {
            if (st1.empty())
            {
                throw (ScriptException("�﷨����,�������ʧ��"));
                return false;
            }

            size_t pos1 = st1.back();
            size_t pos2 = i;
            st1.pop_back();

            mstring simple = script.substr(pos1 + 1, pos2 - pos1 - 1);
            vector<FilterRule> result = SimpleCompile(simple);

            mstring varName = FormatA("var_<%04d>", varIndex++);
            mVarSet[varName] = result;
            script.replace(pos1, pos2 - pos1 + 1, varName);
            i = pos1 + varName.size();
            continue;
        }
        i++;
    }

    if (!st1.empty())
    {
        throw (ScriptException("�﷨����,�������ʧ��"));
        return false;
    }
    mRuleSet = SimpleCompile(script);

    //�﷨��ɫ
    SetRuleStyle();
    return true;
}

map<mstring, int> CScriptEngine::GetStyleSet() {
    mRuleStyle["����"] = STYLE_LOG_WARN;
    mRuleStyle["warn"] = STYLE_LOG_WARN;
    mRuleStyle["����"] = STYLE_LOG_ERROR;
    mRuleStyle["error"] = STYLE_LOG_ERROR;
    mRuleStyle["err"] = STYLE_LOG_ERROR;
    return mRuleStyle;
}

bool CScriptEngine::OnRuleFilter(const char *content, size_t length) const {
    mstring lineStr(content, length);
    for (size_t j = 0 ; j < mRuleSet.size() ; j++)
    {
        const FilterRule &rule = mRuleSet[j];
        set<mstring>::const_iterator k;

        bool stat1 = false;
        if (!rule.mInclude.empty())
        {
            for (k = rule.mInclude.begin() ; k != rule.mInclude.end() ; k++)
            {
                if (mstring::npos == lineStr.find_in_rangei(*k))
                {
                    break;
                }
            }

            if (k == rule.mInclude.end())
            {
                stat1 = true;
            }
        } else {
            stat1 = true;
        }

        if (!stat1)
        {
            continue;
        }

        bool stat2 = false;
        if (rule.mExclude.empty())
        {
            stat2 = true;
        } else {
            for (k = rule.mExclude.begin() ; k != rule.mExclude.end() ; k++)
            {
                if (mstring::npos != lineStr.find_in_rangei(*k))
                {
                    stat2 = false;
                    break;
                }
            }

            if (k == rule.mExclude.end())
            {
                stat2 = true;
            }
        }

        if (stat1 && stat2)
        {
            return true;
        }
    }
    return false;
}

void CScriptEngine::OnStrColour(const mstring &filterStr, LogFilterResult &result) const {
    mstring low = filterStr;
    low.makelower();
    size_t initPos = result.mContent.size();

    for (size_t i = 0 ; i < low.size() ;)
    {
        bool match = false;
        for (map<mstring, int>::const_iterator it = mRuleStyle.begin() ; it != mRuleStyle.end() ; it++)
        {
            if (0 == low.comparei(it->first, i))
            {
                LogKeyword node;
                node.mKeyword = filterStr.substr(i, it->first.size());
                node.mStyle = it->second;
                node.mKeywordStart = initPos + i;
                node.mKeywordEnd = node.mKeywordStart + it->first.size();
                result.mKeywordSet.push_back(node);
                i += node.mKeyword.size();
                match = true;
                break;
            }
        }

        if (!match)
        {
            i++;
        }
    }
}

void CScriptEngine::ClearCache() {
    mRuleSet.clear();
    mRuleStyle.clear();
    mVarSet.clear();
}

bool CScriptEngine::InputLog(const mstring &content, size_t initPos, LogFilterResult &result) const {
    mstring filterStr;

    if (mRuleSet.empty()) {
        filterStr = content.substr(initPos, content.size() - initPos);
    } else {
        size_t lastPos = initPos;
        for (size_t i = initPos ; i < content.size() ; i++)
        {
            char c = content[i];
            if (c == '\n')
            {
                if (i > lastPos)
                {
                    if (OnRuleFilter(content.c_str() + lastPos, i - lastPos))
                    {
                        filterStr.append(content.c_str() + lastPos, i - lastPos + 1);
                    }
                }
                lastPos = i + 1;
            }
        }
    }
    result.mContent += filterStr;
    return !filterStr.empty();
}