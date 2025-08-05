#pragma once


#include <boost/asio/ssl.hpp>

#include "webService.h"
#include "FixedWebServicePool.h"
#include "fixedTemplateSafeList.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "STLTimeWheel.h"



#include<atomic>

// 

struct WEBSERVICELISTENER
{
	WEBSERVICELISTENER(std::shared_ptr<IOcontextPool> ioPool,
		std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolMaster,
		std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolMaster,
		std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePoolMaster, std::shared_ptr<MULTISQLWRITESWPOOL>multiSqlWriteSWPoolMaster,
		const std::string &tcpAddress, const std::string &doc_root , std::shared_ptr<LOGPOOL> logPool ,
		const std::shared_ptr<std::unordered_map<std::string_view, std::string>>fileMap,
		const int socketNum , const int timeOut, const unsigned int checkSecond, std::shared_ptr<STLTimeWheel> timeWheel,
		const char *cert , const char *privateKey
		);



private:
	std::shared_ptr<io_context> m_ioc{};
	std::unique_ptr<boost::asio::ip::tcp::acceptor> m_acceptor{};
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endpoint{};
	const std::string &m_doc_root;
	const std::string &m_tcpAddress;
	std::string m_tempAddress;
	boost::system::error_code m_err;
	int m_sendSize{};

	int m_size{};

	std::atomic<bool> m_startAccept{ true };
	
	
	std::shared_ptr<IOcontextPool> m_ioPool{};

	std::unique_ptr<boost::asio::ssl::context> m_sslContext{};
	
	
	std::unique_ptr<FixedWEBSERVICEPOOL> m_httpServicePool{};

	std::unique_ptr<FIXEDTEMPLATESAFELIST<std::shared_ptr<WEBSERVICE>>> m_httpServiceList{};

	std::shared_ptr<std::function<void(std::shared_ptr<WEBSERVICE>&)>>  m_clearFunction{};

	std::shared_ptr<std::function<void()>>m_startFunction{};

	std::shared_ptr<std::function<void()>>m_startcheckTime{};

	std::unique_ptr<boost::asio::steady_timer> m_timeOutTimer{};       //  这个用于检查超时，不要乱用

	std::shared_ptr<std::unordered_map<std::string_view, std::string>>m_fileMap{};


	std::shared_ptr<ASYNCLOG> m_log{};

	std::shared_ptr<LOGPOOL> m_logPool{};

	std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   主数据库写入连接池
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   主数据库读取连接池

	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   主redis写入连接池
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   主redis读取连接池

	std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //时间轮定时器


	int m_socketNum{};

	int m_timeOut{};

	//时间轮检查间隔
	unsigned int m_checkTurn{};

private:
	void resetEndpoint();

	void resetAcceptor();

	void openAcceptor();

	void bindAcceptor();

	void listenAcceptor();

	void startAccept();

	void handleStartAccept(std::shared_ptr<WEBSERVICE> httpServiceTemp,const boost::system::error_code &err);

	void getBackHTTPSERVICE(std::shared_ptr<WEBSERVICE> &tempHs);

	void restartAccept();

	void notifySocketPool();

	void startRun();

	void reAccept();

	void checkTimeOut();
};