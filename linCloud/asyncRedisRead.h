#pragma once


#include "publicHeader.h"


struct ASYNCREDISREAD
{
	ASYNCREDISREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int checkSize);



	//每次插入新请求时，做以下处理：
	//1 判断请求内容是否合法，不合法直接驳回
	//2 判断是否连接成功，不成功试图插入等待发送请求的队列，插入失败返回插入失败错误信息
	//3 连接成功的情况下，做以下处理：
	//（1） 判断当前请求状态是否为true，如果是，则说明在之前已经开始了请求处理，试图插入等待发送请求的队列，插入失败返回插入失败错误信息
	//（2） 当前请求状态为false，首先将请求状态置为true，独占请求处理，然后开始试图生成redis请求消息，如果不成功，则检查等待发送请求的队列是否存在元素，
	//      如果存在则试图生成redis请求消息，期间非法数据插入已经获取消息结果准备在异步等待期间处理的队列，如果全部为非法，则将请求状态置为false，加锁处理已经获取消息结果准备在异步等待期间处理的队列
	//（3） 如果生成成功，则进入startQuery函数处理，首次startQuery期间没有数据需要进行处理
	bool insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest);




private:
	std::shared_ptr<io_context> m_ioc{};
	std::unique_ptr<boost::asio::steady_timer>m_timer;
	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint;
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock;
	



	std::shared_ptr<std::function<void()>> m_unlockFun{};
	std::string m_redisIP;
	unsigned int m_redisPort{};
	const char **m_data{};





	std::unique_ptr<char[]>m_receiveBuffer;                    //接收消息的缓存
	unsigned int m_memorySize{};                          //接收消息内存块总大小
	unsigned int m_checkSize{};                           //每次检查的块大小
	unsigned int m_thisReceiveLen{};
	unsigned int m_receiveSize{};
	unsigned int m_receivePos{  };
	unsigned int m_lastReceivePos{  };     //  m_lastReceivePos在每次解析后才去变更



	std::unique_ptr<char[]>m_outRangeBuffer;              //保存边界位置的字符串
	unsigned int m_outRangeSize{};



	std::atomic<bool>m_connect{ false };                  //判断是否已经建立与redis端的连接
	std::atomic<bool>m_queryStatus{ false };              //判断是否已经启动处理流程，这里的queryStatus不仅仅是请求那么简单，而是包括请求，接收消息，将目前请求全部消化完毕的标志
	std::atomic<bool>m_writeStatus{ false };              //标记异步写入状态
	std::atomic<bool>m_readStatus{ false };               //标记异步读取状态
	std::atomic<bool>m_readyRequest{ false };             //准备请求标志状态
	std::atomic<bool>m_readyParse{ false };
	std::atomic<bool>m_messageEmpty{ true };             //标记m_messageList是否为空


	bool parseLoop{ false };
	bool parseStatus;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//query语句指针首位  长度  返回redis结果指针    回调处理函数  结果数量
	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_messageList;       //  等待发送请求的队列
	

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_tempRequest;


	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_waitMessageList;   //  等待获取处理结果的队列
	



	struct SINGLEMESSAGE;
	SAFELIST<std::shared_ptr<SINGLEMESSAGE>>m_messageResponse;                          //已经获取消息结果准备在异步等待期间处理的队列
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





	struct SINGLEMESSAGE
	{
		SINGLEMESSAGE(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> tempRequest, bool tempVaild,
			ERRORMESSAGE tempEM) :m_tempRequest(tempRequest), m_tempVaild(tempVaild), m_tempEM(tempEM) {}

		std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_tempRequest;
		bool m_tempVaild{};
		ERRORMESSAGE m_tempEM{};
	};


	////////////////////////////////////////////////////////////////////////////
	std::vector<const char*>m_vec;            //readyQuery使用



	std::unique_ptr<char[]>m_ptr;             //存储转换的请求
	unsigned int m_ptrLen{};
	unsigned int m_totalLen{};







	/////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_redisRequest{};

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_nextRedisRequest{};


	bool m_insertRequest{ false };              //  插入请求状态


private:
	bool readyEndPoint();

	bool readySocket();

	void firstConnect();

	void setConnectSuccess();

	void setConnectFail();


	////////////////////////////////////////////////////

	bool readyQuery(const char *source, const unsigned int sourceLen);

	bool tryCheckList();                 // 检查等待发送请求的队列，查看是否有可以发送的请求存在


	///////////////////////////////////////////////////
	void startQuery();




	////////////////////////////////////////////////////
	//对三大队列进行处理，将已经接收消息的数据重置
	void resetData();

	void tryReadyEndPoint();

	void tryReadySocket();

	void tryConnect();

	void tryResetTimer();

	

	void startReadyRequest();

	void startReadyParse();



	void startReceive();

	//从收到的结果中解析数据
	bool startParse();

	//解析数据特种版本，用于解析等待获取结果队列的情况
	bool StartParseWaitMessage();   


	//解析数据特种版本，用于解析本次情况的情况
	bool StartParseThisMessage();




	//在有需要时处理一次已经有结果的消息队列即可，避免重复操作
	void handleMessageResponse();     //处理已经有结果的队列


	void queryLoop();             //异步query

	void checkLoop();              //startReceive  startParse成功之后的情况处理

	void simpleReceive();          //只是简单的读取，不做任何事情 


	void simpleReceiveWaitMessage();          //只是简单的读取特种版本，不做任何事情 用于解析等待获取结果队列的情况

	void simpleReceiveThisMessage();          //只是简单的读取特种版本，不做任何事情 用于解析本次请求的情况
};
