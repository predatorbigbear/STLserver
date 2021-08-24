#pragma once

#include "publicHeader.h"




struct REDISREAD
{
	REDISREAD( std::shared_ptr<io_context> ioc , std::shared_ptr<std::function<void()>>unlockFun , const std::string &redisIP,const unsigned int redisPort);

	bool insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest);

	std::shared_ptr<function<void()>> getRedisUnlock();






private:
	std::shared_ptr<io_context> m_ioc;

	std::atomic<bool>m_queryStatus{ false };

	std::unique_ptr<boost::asio::steady_timer>m_timer;


	//query语句指针首位  长度  返回redis结果指针   结果行数  结果列数   回调处理函数 结果数量
	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_messageList;


	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_redisRequest{};


	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_nextRedisRequest{};
	bool m_nextVaild{};
	ERRORMESSAGE m_nextEM{};

	std::shared_ptr<std::function<void()>>m_freeRedisResultFun;


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


	unsigned int m_receiveLen{ 65535 };
	std::unique_ptr<char[]>m_receiveBuffer{ new char[m_receiveLen] };
	unsigned int m_thisReceiveLen{};

	ERRORMESSAGE m_errorMessage;


private:
	bool readyEndPoint();

	bool readySocket();

	void connect();

	bool readyQuery(const char *source, const unsigned int sourceLen);

	void query();

	void receive();

	bool checkRedisMessage(const char *source,const unsigned int sourceLen);

	void freeResult();

	void checkList();

	void readyNextRedisQuery();
};
