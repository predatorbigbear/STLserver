#pragma once


#include "httpService.h"
#include "FixedHttpServicePool.h"
#include "fixedTemplateSafeList.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "STLTimeWheel.h"
#include "commonStruct.h"


#include <boost/asio/ssl.hpp>

#include<atomic>

// 

struct listener 
{
	listener(std::shared_ptr<IOcontextPool> ioPool,
		std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolMaster,
		std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolMaster,
		std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePoolMaster, std::shared_ptr<MULTISQLWRITESWPOOL>multiSqlWriteSWPoolMaster,
		const std::string &tcpAddress, const std::string &doc_root , std::shared_ptr<LOGPOOL> logPool ,
		const std::shared_ptr<std::unordered_map<std::string_view, std::string>>fileMap,
		const int socketNum , const int timeOut, const unsigned int checkSecond, std::shared_ptr<STLTimeWheel> timeWheel,
		const bool isHttp = true, const char* cert = nullptr, const char* privateKey = nullptr
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
	
	//是否为http  true为http  false为https
	const bool m_isHttp{};

	std::unique_ptr<boost::asio::ssl::context>m_sslSontext{};

	std::atomic<bool> m_startAccept{ true };
	
	
	std::shared_ptr<IOcontextPool> m_ioPool{};
	
	
	std::unique_ptr<FixedHTTPSERVICEPOOL> m_httpServicePool{};

	std::unique_ptr<FIXEDTEMPLATESAFELIST<std::shared_ptr<HTTPSERVICE>>> m_httpServiceList{};

	std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>&)>>  m_clearFunction{};

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

	template<typename T>
	void startAccept();

	template<typename T>
	void handleStartAccept(std::shared_ptr<HTTPSERVICE> httpServiceTemp,const boost::system::error_code &err);

	void getBackHTTPSERVICE(std::shared_ptr<HTTPSERVICE> &tempHs);

	template<typename T>
	void restartAccept();

	void notifySocketPool();

	void startRun();

	void reAccept();

	void checkTimeOut();
};


// 这里采用了对象池的操作，对象池在获取和归还对象的时候需要进行加锁操作。原本打算是对象池在对象不足的时候按需要进行分配新的对象，
// 但是从实际开发的角度来看，虽然在内存充足的情况下，对象可以接近无限地
//分配新的，但是应该认识到一点：带宽、数据库这些资源是有限的，用户连接多了，应该合理限制单机承载人数才可以确保用户使用体验，
// 否则如果一个网页体验大打折扣是没有意义的。因此目前的使用方式是对象池在
//初始化时一次性设定最大对象数量，一次性初始化好，当监听取不到新的socket资源时，就暂停监听，
// 利用回调函数通知对象池于下次对象归还时再通知开启监听。在没有对象可取的情况下再去尝试获取是没有任何意义的，
//因此在没有对象时，暂停监听，为归还让出锁资源，在下次归还时再重新监听。

//事实上尽快获取到对象的办法还包括在对象短缺的时候发送通知给超时处理类缩短超时时间，当然这点看有没有必要做
template<typename T>
inline void listener::startAccept()
{
	if constexpr (std::is_same_v<T, HTTPSERVER>)
	{
		try
		{
			std::shared_ptr<HTTPSERVICE> httpServiceTemp{  };
			m_httpServicePool->getNextBuffer(httpServiceTemp);

			if (httpServiceTemp && httpServiceTemp->m_buffer && httpServiceTemp->m_buffer->getSock())
			{
				m_acceptor->async_accept(*(httpServiceTemp->m_buffer->getSock()), [this, httpServiceTemp](const boost::system::error_code& err)
				{
					handleStartAccept<T>(httpServiceTemp, err);
				});
			}
			else
			{
				m_log->writeLog(__FILE__, __LINE__, "notifySocketPool  ");
				notifySocketPool();
			}
		}
		catch (const std::exception& e)
		{
			//m_log->writeLog(__FILE__, __LINE__, e.what());
			m_log->writeLog(__FILE__, __LINE__, "restartAccept  ");
			restartAccept<T>();
		}
	}
}

template<typename T>
inline void listener::handleStartAccept(std::shared_ptr<HTTPSERVICE> httpServiceTemp, const boost::system::error_code& err)
{

	if constexpr(std::is_same_v<T, HTTPSERVER>)
	{
		if (err)
		{
			m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
		}
		else
		{
			if (httpServiceTemp)
			{
				if (m_httpServiceList->insert(httpServiceTemp))
				{
					httpServiceTemp->setReady(httpServiceTemp);
				}
				else
				{
					m_log->writeLog(__FILE__, __LINE__, "!httpServiceTemp");
					m_httpServicePool->getBackElem(httpServiceTemp);
				}
			}
			restartAccept<T>();
		}
	}
}




template<typename T>
inline void listener::restartAccept()
{
	if constexpr (std::is_same_v<T, HTTPSERVER> || std::is_same_v<T, HTTPSSERVER>)
	{
		if (m_startAccept.load())
		{
			startAccept<T>();
		}
	}
}
