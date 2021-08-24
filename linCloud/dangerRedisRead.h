#pragma once

#include "publicHeader.h"




struct DANGERREDISREAD
{
	DANGERREDISREAD(std::shared_ptr<io_context> ioc, shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort ,
		const unsigned int memorySize,const unsigned int checkSize);

	bool insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest);





private:
	std::shared_ptr<io_context> m_ioc;

	std::atomic<bool>m_queryStatus{ false };


	//query语句指针首位  长度  返回redis结果指针    回调处理函数 结果数量
	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_messageList;


	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_waitMessageList;


	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_redisRequest{};


	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_nextRedisRequest{};
	bool m_nextVaild{};
	ERRORMESSAGE m_nextEM{};

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_tempRedisRequest{};
	bool m_tempVaild{};
	ERRORMESSAGE m_tempEM{};


	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_lastRedisRequest{};
	bool m_lastVaild{};
	ERRORMESSAGE m_lastEM{};


	struct SINGLEMESSAGE;
	SAFELIST<std::shared_ptr<SINGLEMESSAGE>>m_messageResponse;

	std::shared_ptr<SINGLEMESSAGE>m_tempSINGLEMESSAGE;


	boost::system::error_code m_err;

	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint;

	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock;

	std::shared_ptr<std::function<void()>> m_unlockFun;

	std::string m_redisIP{};
	unsigned int m_redisPort{};
	const char **m_data{};


	std::vector<const char*>m_vec;        //readyQuery使用

	std::unique_ptr<char[]>m_ptr;
	unsigned int m_ptrLen{};
	unsigned int m_totalLen{};


	
	std::unique_ptr<char[]>m_receiveBuffer{ };
	unsigned int m_thisReceiveLen{};
	unsigned int m_memorySize{};
	unsigned int m_checkSize{};
	unsigned int m_receiveSize{};
	unsigned int m_receivePos{  };
	unsigned int m_lastReceivePos{  };     //  m_lastReceivePos在每次解析后才去变更

	ERRORMESSAGE m_errorMessage;


	std::atomic<bool>m_reciveStatus{ false };          //标志接收数据是否完成
	std::atomic<bool>m_readyNextStatus{ false };       //标志准备工作是否完成
	std::atomic<bool>m_insertMessage{ false };         //标志待处理消息是否插入成功


private:
	struct SINGLEMESSAGE
	{
		SINGLEMESSAGE(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> tempRequest, bool tempVaild,
			ERRORMESSAGE tempEM) :m_tempRequest(tempRequest), m_tempVaild(tempVaild), m_tempEM(tempEM) {}

		std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_tempRequest;
		bool m_tempVaild{};
		ERRORMESSAGE m_tempEM{};
	};





private:
	bool readyEndPoint();

	bool readySocket();

	void connect();

	bool readyQuery(const char *source, const unsigned int sourceLen);



	void startQuery();

	void query();



	void startReceive();

	void receive();

	bool checkRedisMessage(const char *source, const unsigned int sourceLen);

	void checkList();

	void tryCheckList();

	void readyNextRedisQuery();


	void handleMessage();
};
