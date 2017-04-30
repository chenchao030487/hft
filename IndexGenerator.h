/////////////////////////////////////////////////////////////////////////
///@system 指数计算策略
///@company 上海明汯投资管理有限公司
///@file IndexGenerator.h
///@brief定义了类CIndexGenerator
///@history 
///20170429	陈超	创建该文件
/////////////////////////////////////////////////////////////////////////

#ifndef INDEX_GENERATOR_H
#define INDEX_GENERATOR_H

#include "StrategyInterface.h"
#include <map>
#include "hiredis.h"

#define TIME_LEN 19200
#define BEGIN_TIME 34200
#define END_TIME 53700

using namespace std;

struct IndexPrice
{
	IndexPrice(){
		LastPublishPrice=0.0;
		CurrentPrice=0.0;
	}
	double LastPublishPrice;
	double CurrentPrice;
};

struct WeightInfo
{
	WeightInfo(){
		Index=0;
		Weight=0.0;
	}
	int Index;
	double Weight;
};

class CIndexGenerator: public CStrategyInterface
{
public:
    CIndexGenerator();
	~CIndexGenerator();
    virtual void    Init();
    virtual void    Release();
    virtual void    RegisterTrader(ITrader * pTrader);
    virtual string  GetStrategyName();
    virtual bool    RspUserLogin(int nMaxStrategyOrderLocalID,int nInterfaceType);
    virtual void    GetSubscribedInstrument(vector<string>& vecInstrument);
    virtual bool    RtnL2MktData(CL2MktData* pL2MktData);
    virtual void    OnTimer1s();
    virtual void    HandleOtherTask();
private:
    int GetTime(const string& time);
	int GetTime();
	string GetCurrentTime();
	const vector<string>&  Split(const string& str, const string& pattern);
	void SendMessage(const string& target,int type,const string& message);
public:
    void CreateLog();
    void RedisLog(string message);
    void Log(string message);
    void ReadConfig();
    void RedisInit(string hostname, string port, string password, string database);
    void FetchWeight();
    void InitShareVariable();
private:
    map<string,double> m_mapMidQuote;//ExchangeID+InstrumentID==>MidQuote
	vector<int,IndexPrice> m_vecIndexPrice;//Index==>IndexPrice
    vector<string> m_vecInstrument;//需要订阅的合约
	map<string,vector<WeightInfo>> m_mapWeightInfo;//成分股对应的各指数权重
    void Output(const char * format,...);
private:
    int m_MaxStrategyOrderLocalID;
    volatile bool m_isLogin;
    FILE*  LogFile;
	string m_strategyName;
    redisContext *m_pDB;;
};

#endif //MESSAGEINTERFACE_H
