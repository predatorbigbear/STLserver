#pragma once


#include<string_view>
#include<algorithm>
#include<memory>
#include<cstring>
#include<string>
#include<vector>

//目前target version字段不在这里存储
struct HTTPRESULT
{
	HTTPRESULT();

	//重置状态位，不要直接将所有string_view置空，性能开销大很多
	//使用指针记录下实际发生过改变的string_view，对改变过的进行重置
	void resetheader();


	////////////////////////////判断是否为空的函数

	const bool isMethodEmpty();

	const bool isTargetEmpty();

	const bool isVersionEmpty();

	const bool isAcceptEmpty();

	const bool isAccept_CharsetEmpty();

	const bool isAccept_EncodingEmpty();

	const bool isAccept_LanguageEmpty();

	const bool isAccept_RangesEmpty();

	const bool isAuthorizationEmpty();

	const bool isCache_ControlEmpty();

	const bool isConnectionEmpty();

	const bool isCookieEmpty();

	const bool isContent_LengthEmpty();

	const bool isContent_TypeEmpty();

	const bool isDateEmpty();

	const bool isExpectEmpty();

	const bool isFromEmpty();

	const bool isHostEmpty();

	const bool isIf_MatchEmpty();

	const bool isIf_Modified_SinceEmpty();

	const bool isIf_None_MatchEmpty();

	const bool isIf_RangeEmpty();

	const bool isIf_Unmodified_SinceEmpty();

	const bool isMax_ForwardsEmpty();

	const bool isPragmaEmpty();

	const bool isProxy_AuthorizationEmpty();

	const bool isRangeEmpty();

	const bool isRefererEmpty();

	const bool isSetCookieEmpty();

	const bool isTEEmpty();

	const bool isUpgradeEmpty();

	const bool isUser_AgentEmpty();

	const bool isViaEmpty();

	const bool isWarningEmpty();

	const bool isBodyEmpty();

	const bool isTransfer_EncodingEmpty();

	const bool isparaEmpty();




	//////////////////////////////////////////////////////////////////  设置函数

	void setMethod(const char *begin,const char *end);

	void setTarget(const char *begin,const char *end);

	void setVersion(const char *begin,const char *end);

	void setAccept(const char *begin,const char *end);

	void setAccept_Charset(const char *begin,const char *end);

	void setAccept_Encoding(const char *begin,const char *end);

	void setAccept_Language(const char *begin,const char *end);

	void setAccept_Ranges(const char *begin,const char *end);

	void setAuthorization(const char *begin,const char *end);

	void setCache_Control(const char *begin,const char *end);

	void setConnection(const char *begin,const char *end);

	void setCookie(const char *begin,const char *end);

	void setContent_Length(const char *begin,const char *end);

	void setContent_Type(const char *begin,const char *end);

	void setDate(const char *begin,const char *end);

	void setExpect(const char *begin,const char *end);

	void setFrom(const char *begin,const char *end);

	void setHost(const char *begin,const char *end);

	void setIf_Match(const char *begin,const char *end);

	void setIf_Modified_Since(const char *begin,const char *end);

	void setIf_None_Match(const char *begin,const char *end);

	void setIf_Range(const char *begin,const char *end);

	void setIf_Unmodified_Since(const char *begin,const char *end);

	void setMax_Forwards(const char *begin,const char *end);

	void setPragma(const char *begin,const char *end);

	void setProxy_Authorization(const char *begin,const char *end);

	void setRange(const char *begin,const char *end);

	void setReferer(const char *begin,const char *end);

	void setSetCookie(const char* begin, const char* end);

	void setTE(const char *begin,const char *end);

	void setUpgrade(const char *begin,const char *end);

	void setUser_Agent(const char *begin,const char *end);

	void setVia(const char *begin,const char *end);

	void setWarning(const char *begin,const char *end);

	void setBody(const char *begin,const char *end);

	void setTransfer_Encoding(const char *begin,const char *end);

	void setpara(const char *begin,const char *end);



	/////////////////////////////////////////////////获取对应的std::string_view对象函数

	const std::string_view getMethod();

	const std::string_view getTarget();

	const std::string_view getVersion();

	const std::string_view getAccept();

	const std::string_view getAccept_Charset();

	const std::string_view getAccept_Encoding();

	const std::string_view getAccept_Language();

	const std::string_view getAccept_Ranges();

	const std::string_view getAuthorization();

	const std::string_view getCache_Control();

	const std::string_view getConnection();

	const std::string_view getCookie();

	const std::string_view getContent_Length();

	const std::string_view getContent_Type();

	const std::string_view getDate();

	const std::string_view getExpect();

	const std::string_view getFrom();

	const std::string_view getHost();

	const std::string_view getIf_Match();

	const std::string_view getIf_Modified_Since();

	const std::string_view getIf_None_Match();

	const std::string_view getIf_Range();

	const std::string_view getIf_Unmodified_Since();

	const std::string_view getMax_Forwards();

	const std::string_view getPragma();

	const std::string_view getProxy_Authorization();

	const std::string_view getRange();

	const std::string_view getReferer();

	const std::vector<std::string_view> &getSetCookie();

	const std::string_view getTE();

	const std::string_view getUpgrade();

	const std::string_view getUser_Agent();

	const std::string_view getVia();

	const std::string_view getWarning();

	const std::string_view getBody();

	const std::string_view getTransfer_Encoding();

	const std::string_view getpara();



	///////////////////////////////////////////////

	//直接查找尝试获取对应的string_view 最底层函数 
	std::string_view findView(const char* begin, const char* end);

	//查找尝试获取对应的string_view  
	std::string_view findView(const char* str);

	std::string_view findView(const std::string &str);


	std::string_view findView(const std::string_view str);



private:
	std::string_view m_Method{};
	std::string_view m_Target{};
	std::string_view m_Version{};
	std::string_view m_Accept{};
	std::string_view m_Accept_Charset{};
	std::string_view m_Accept_Encoding{};
	std::string_view m_Accept_Language{};
	std::string_view m_Accept_Ranges{};
	std::string_view m_Authorization{};
	std::string_view m_Cache_Control{};
	std::string_view m_Connection{};
	std::string_view m_Cookie{};
	std::string_view m_Content_Length{};
	std::string_view m_Content_Type{};
	std::string_view m_Date{};
	std::string_view m_Expect{};
	std::string_view m_From{};
	std::string_view m_Host{};
	std::string_view m_If_Match{};
	std::string_view m_If_Modified_Since{};
	std::string_view m_If_None_Match{};
	std::string_view m_If_Range{};
	std::string_view m_If_Unmodified_Since{};
	std::string_view m_Max_Forwards{};
	std::string_view m_Pragma{};
	std::string_view m_Proxy_Authorization{};
	std::string_view m_Range{};
	std::string_view m_Referer{};
	std::string_view m_TE{};
	std::string_view m_Upgrade{};
	std::string_view m_User_Agent{};
	std::string_view m_Via{};
	std::string_view m_Warning{};
	std::string_view m_Body{};
	std::string_view m_Transfer_Encoding{};
	std::string_view m_para{};
	//因为Set_Cookie可能有多个，所以用vector单独装起来
	std::vector<std::string_view>m_Set_Cookie{}; 


	//记录每个string_view是否为空的状态，Set-Cookie在这里找不到，直接调用上面的get方法获取
	std::unique_ptr<std::string_view*[]>m_headerPtr{};

	std::string_view** m_headerbegin{};

	std::string_view** m_headerEnd{};




	enum http_header_num
	{
		Method = 0,
		Target = 1,
		Version = 2,
		Accept = 3,
		Accept_Charset = 4,
		Accept_Encoding = 5,
		Accept_Language = 6,
		Accept_Ranges = 7,
		Authorization = 8,
		Cache_Control = 9,
		Connection = 10,
		Cookie = 11,
		Content_Length = 12,
		Content_Type = 13,
		Date = 14,
		Expect = 15,
		From = 16,
		Host = 17,
		If_Match = 18,
		If_Modified_Since = 19,
		If_None_Match = 20,
		If_Range = 21,
		If_Unmodified_Since = 22,
		Max_Forwards = 23,
		Pragma = 24,
		Proxy_Authorization = 25,
		Range = 26,
		Referer = 27,
		TE = 28,
		Upgrade = 29,
		User_Agent = 30,
		Via = 31,
		Warning = 32,
		Body = 33,
		Transfer_Encoding = 34,
		para = 35,
		all_len = 36
	};

};



