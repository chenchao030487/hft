#include "IndexGenerator.h"

CIndexGenerator::CIndexGenerator()
{
    ConfigFile = "../../../stconfig/" + strategy_name + "/config.ini";
	m_StrategyName="IndexGeneratorStrategy";
	m_mapMidQuote.clear();
	m_vecIndexPrice.resize(100);//最大容纳100个指数
    m_vecInstrument.clear();
	m_mapWeightInfo.clear();
}

void CIndexGenerator::CreateLog()
{
    string LogFile_filename = "../../../stlog/" + strategy_name + "/" + strategy_name + ".log";
    LogFile = fopen(LogFile_filename.c_str(), "wt");
    if (LogFile == NULL){
        printf("fopen log failed.\n");
    }
    else{
        printf("fopen log successfully.\n");
    }
}

void CIndexGenerator::RedisLog(string message)
{

    Output("%s %s : Redis Hostname[%s] Port[%s] DataBase[%s] %s\n",
           GetCurrentTime().c_str(),
           strategy_name.c_str(),
           config_field->redis_hostname.c_str(),
           config_field->redis_port.c_str(),
           config_field->redis_database.c_str(),
           message.c_str()
          );

}

void CIndexGenerator::Log(string message)
{
    Output("%s %s : %s \n",
           GetCurrentTime().c_str(),
           strategy_name.c_str(),
           message.c_str()
          );

}

void CIndexGenerator::ReadConfig()
{
    configSettings = new Config(ConfigFile);
    config_field = new Message_ConfigStruct;
    config_field->redis_hostname = configSettings->Read("redis_hostname", config_field->redis_hostname);
    config_field->redis_port = configSettings->Read("redis_port", config_field->redis_port);
    config_field->redis_password = configSettings->Read("redis_password", config_field->redis_password);
    config_field->redis_database = configSettings->Read("redis_database", config_field->redis_database);
}

void CIndexGenerator::RedisInit(string hostname, string port, string password, string database)
{

    m_pDB = redisConnect(hostname.c_str(), stoi(port));
    if (m_pDB->err){
        LogMessage = m_pDB->errstr;
        RedisLog(LogMessage);
        exit(1);
    }
    else{
        LogMessage = "Connected OK";
        RedisLog(LogMessage);
    }

    //auth
    redisReply *reply = (redisReply *)redisCommand(m_pDB, "AUTH %s", password.c_str());
    LogMessage = "Auth " + string(reply->str);
    RedisLog(LogMessage);
    freeReplyObject(reply);

    //select database
    reply = (redisReply *)redisCommand(m_pDB, "SELECT %s", database.c_str());
    LogMessage = "Select database " + string(reply->str);
    RedisLog(LogMessage);
    freeReplyObject(reply);
    return;
}


void CIndexGenerator::FetchWeight()
{
    redisReply* reply = (redisReply *)redisCommand(m_pDB, "LRANGE %s 0 -1", "20161209_weight");
    if(reply->type == REDIS_REPLY_ARRAY) {
        for (size_t no = 0; no < reply->elements; no++) {
            vector<string> vecWeight = split(reply->element[no]->str, ",");
            m_vecInstrument.push_back(vecWeight[0]);
			m_mapMidQuote[vecWeight[0]]=0.0;
            for(size_t i = 1; i < vecWeight.size(); i++){
                if(stod(vecWeight[i])!=0){
					WeightInfo info;
					info.Index=i;
					info.Weight=stod(vecWeight[i]);
					m_mapWeightInfo[vecWeight[0]].push_back(info);
                }
            }
        }
    }
    freeReplyObject(reply);
	return;
}

void  CIndexGenerator::Init()
{
    CreateLog();
    ReadConfig();
    RedisInit(config_field->redis_hostname, config_field->redis_port, config_field->redis_password, config_field->redis_database);
    FetchWeight();
	return;
}

void  CIndexGenerator::Release()
{
    delete this;
}

string CIndexGenerator::GetStrategyName()
{
    return m_strategyName;
}

void CIndexGenerator::RegisterTrader(ITrader * pTrader)
{
	return;
}

bool  CIndexGenerator::RspUserLogin(int nMaxStrategyOrderLocalID,int nInterfaceType)
{
    if(nInterfaceType==0){
        m_MaxStrategyOrderLocalID=nMaxStrategyOrderLocalID;
        m_isLogin=true;
    }
    return true;
}


bool  CIndexGenerator::RtnL2MktData(CL2MktData* pL2MktData) 
{
	int time=GetTime(pL2MktData->TimeStamp);
	if(time<BEGIN_TIME||time>END_TIME) {
		return true;
    }
	
    if ((pL2MktData.OfferVolume1 != 0)&&(pL2MktData.BidVolume1 != 0)) {
	    double midQuote=(pL2MktData->OfferPrice1 + pL2MktData->BidPrice1)/2.0;
		double delta=midQuote-m_mapMidQuote[pL2MktData->ExchangeID+pL2MktData->InstrumentID];
		///查找合约相关的指数信息
		map<string,vector<WeightInfo>>::iterator iter=m_mapWeightInfo.find(pL2MktData->ExchangeID+pL2MktData->InstrumentID);
		if(iter!=m_mapWeightInfo.end()){
			for(size_t i=0;i<m_mapWeightInfo.second.size();i++){
				int index=m_mapWeightInfo.second[i].Index;
				double weight=m_mapWeightInfo.second[i].Weight;
				m_vecIndexPrice[index].CurrentPrice+=delta*weight;
				///如果引起指数价格变化达到0.01就发布指数,并且开盘前30秒不发布指数（可能成分股行情还未全部收到）
				if(fabs(m_vecIndexPrice[index].CurrentPrice-m_vecIndexPrice[index].LastPublishPrice)>0.01&&time>BEGIN_TIME+30){
					///向策略发送指数更新信息
					char buff[1024] = {0};
					sprintf(buff, "%d,%d,%.6f",time,index,m_vecIndexPrice[index].CurrentPrice);
					SendMessage("tyrande",1,buf);
					///更新最新发布指数
					m_vecIndexPrice[index].LastPublishPrice=m_vecIndexPrice[index].CurrentPrice;
				}
			}
		}
		m_mapMidQuote[pL2MktData->ExchangeID+pL2MktData->InstrumentID] =midQuote;
	}
	return true;
}


void CIndexGenerator::GetSubscribedInstrument(vector<string>& vecInstrument)
{
    vecInstrument=m_vecInstrument
}

void CIndexGenerator::Output(const char * format,...)
{
	if(LogFile){
		va_list vl;
		va_start(vl,format);
		vfprintf(LogFile,format,vl);
		va_end(vl);
		fflush(LogFile);
	}
}


void CIndexGenerator::OnTimer1s()
{
    return;
}


void CIndexGenerator::HandleOtherTask()
{
    return;
}

int CIndexGenerator::GetTime(const string& time)
{
	return 3600 * atoi(time.substr(0, 2).c_str()) 
	       + 60 * atoi(time.substr(3, 5).c_str()) 
		   + atoi(time.substr(6, 8).c_str());
}

int CIndexGenerator::GetTime()
{
	time_t nowtime;
    nowtime = time(NULL);
    struct tm *local;
    local=localtime(&nowtime);
    return local->tm_hour*3600 + local->tm_min*60 + local->tm_sec;
}

string CIndexGenerator::GetCurrentTime()
{
    char strTime[100] = {0};
    time_t nowtime;
    nowtime = time(NULL); //获取日历时间

    struct tm *local;
    local=localtime(&nowtime);  //获取当前系统时间
    strftime(strTime, sizeof(strTime) - 1, "%Y-%m-%d %H:%M:%S", local);
    strTime[sizeof(strTime) - 1] = '\0';
    return string(strTime);

}

const vector<string>& CIndexGenerator::Split(const string& str,const string& pattern)
{
    static vector<string> ret;
	ret.clear();
	
    if(pattern.empty()){
        return ret;
    }
    
    size_t start=0,index=str.find_first_of(pattern,0);
    while(index!=str.npos){
        if(start!=index){
            ret.push_back(str.substr(start, index - start));
        }
        start=index+1;
        index=str.find_first_of(pattern,start);
    }
    if(!str.substr(start).empty()){
        ret.push_back(str.substr(start));
    }
    return ret;
}

void CIndexGenerator::SendMessage(const string& target,int type,const string& message)
{
	CStrategyMessage strategyMessage;
	strategyMessage.TargetStrategyName=target;
	strategyMessage.MessageType=type;
	strategyMessage.MessageContent=message;
	m_pCallBack->SendMessage(&strategyMessage);
}