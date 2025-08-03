#include "listener.h"


listener::listener(std::shared_ptr<IOcontextPool> ioPool,
	std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPoolMaster,
	std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPoolMaster,
	std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePoolMaster, std::shared_ptr<MULTISQLWRITESWPOOL>multiSqlWriteSWPoolMaster,
	const std::string &tcpAddress, const std::string &doc_root, std::shared_ptr<LOGPOOL> logPool,
	const std::shared_ptr<std::unordered_map<std::string_view, std::string>>fileMap,
	const int socketNum, const int timeOut, const unsigned int checkSecond, std::shared_ptr<STLTimeWheel> timeWheel,
	const bool isHttp , const char* cert , const char* privateKey 
	) :
	m_ioPool(ioPool), m_socketNum(socketNum), m_timeOut(timeOut), m_timeWheel(timeWheel),
	m_doc_root(doc_root), m_tcpAddress(tcpAddress), m_fileMap(fileMap),
	m_multiSqlReadSWPoolMaster(multiSqlReadSWPoolMaster),
	m_multiRedisReadPoolMaster(multiRedisReadPoolMaster),m_multiRedisWritePoolMaster(multiRedisWritePoolMaster),
	m_multiSqlWriteSWPoolMaster(multiSqlWriteSWPoolMaster), m_isHttp(isHttp)
{
	if (m_ioPool &&  !doc_root.empty() && !tcpAddress.empty() && logPool && m_timeOut > 0 && checkSecond && m_timeWheel
		&& m_multiSqlReadSWPoolMaster 
		&& m_multiRedisReadPoolMaster && m_multiRedisWritePoolMaster && multiSqlWriteSWPoolMaster
		)
	{
		m_logPool = logPool;
		m_log = m_logPool->getLogNext();

		//https情况
		if (!isHttp)
		{
			m_sslSontext.reset(new boost::asio::ssl::context(boost::asio::ssl::context::sslv23));
			m_sslSontext->set_options(
				boost::asio::ssl::context::default_workarounds
				| boost::asio::ssl::context::no_sslv2
				| boost::asio::ssl::context::single_dh_use);
			m_sslSontext->use_certificate_chain_file(cert);
			m_sslSontext->use_private_key_file(privateKey, boost::asio::ssl::context::pem);
		}

		m_startFunction.reset(new std::function<void()>(std::bind(&listener::reAccept, this)));
		m_clearFunction.reset(new std::function<void(std::shared_ptr<HTTPSERVICE>&)>(std::bind(&listener::getBackHTTPSERVICE, this, std::placeholders::_1)));
		m_timeOutTimer.reset(new boost::asio::steady_timer(*(m_ioPool->getIoNext())));
        // ulimit -a

		
		m_httpServicePool.reset(new FixedHTTPSERVICEPOOL(m_ioPool, m_doc_root, m_multiSqlReadSWPoolMaster,
			m_multiRedisReadPoolMaster ,m_multiRedisWritePoolMaster,
			m_multiSqlWriteSWPoolMaster,m_startFunction, m_logPool, m_timeWheel, m_fileMap,
			m_timeOut, m_clearFunction,
			m_socketNum));
		if (!m_httpServicePool->ready())
		{
			std::cout << "Fail to start m_httpServicePool\n";
			return;
		}
		
		m_checkTurn = m_timeOut / checkSecond;
		if (m_timeOut % checkSecond)
			++m_checkTurn;

		m_startcheckTime.reset(new std::function<void()>(std::bind(&listener::checkTimeOut, this)));
		m_httpServiceList.reset(new FIXEDTEMPLATESAFELIST<std::shared_ptr<HTTPSERVICE>>(m_startcheckTime, m_socketNum));
		if (!m_httpServiceList->ready())
		{
			std::cout << "Fail to start m_httpServiceList\n";
			return;
		}

		m_startAccept = true;
		
		startRun();
	}
	else
	{
		std::cout << "Fail to start listener\n";
	}
}



void listener::resetEndpoint()
{
	try
	{
		if (!m_tcpAddress.empty())
		{
			auto iter = std::find(m_tcpAddress.cbegin(), m_tcpAddress.cend(), ':');
			boost::asio::ip::address ad;
			if (iter != m_tcpAddress.cend())
			{
				error_code err;
				ad = boost::asio::ip::address_v4::any();
				if (err)
				{
					//m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
					resetEndpoint();
				}
				else
				{
					if (std::distance(iter, m_tcpAddress.cend()) > 1)
					{
						if (std::all_of(iter+1, m_tcpAddress.cend(), std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'),
							std::bind(std::less_equal<>(), std::placeholders::_1, '9'))))
						{
							int num{ 1 }, index{-1};
							m_endpoint.reset(new boost::asio::ip::tcp::endpoint(ad, std::accumulate(m_tcpAddress.crbegin(), std::make_reverse_iterator(iter + 1), 0,
								[&num, &index](auto &sum, auto const elem)
							{
								if (++index)num *= 10;
								return sum += (elem - '0')*num;
							})));
							resetAcceptor();
						}
					}
				}
			}
		}
	}
	catch (const std::exception &e)
	{
		//m_log->writeLog(__FILE__, __LINE__, e.what());
	}
}




void listener::resetAcceptor()
{
	try
	{
		m_acceptor.reset(new boost::asio::ip::tcp::acceptor(*(m_ioPool->getIoNext())));
		openAcceptor();
	}
	catch (const std::exception &e)
	{
		//m_log->writeLog(__FILE__, __LINE__, e.what());
		resetAcceptor();
	}
}



void listener::openAcceptor()
{
	error_code err;
	m_acceptor->open(m_endpoint->protocol(), err);
	if (err)
	{
		//m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
		resetEndpoint();
	}
	else
	{
		m_acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		bindAcceptor();
	}
}



void listener::bindAcceptor()
{
	error_code err;
	m_acceptor->bind(*m_endpoint,err);
	if (err)
	{
		//m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
		resetEndpoint();
	}
	else
	{
		listenAcceptor();
	}
}



void listener::listenAcceptor()
{
	error_code err;
	m_acceptor->listen(numeric_limits<int>::max(), err);
	if (err)
	{
		//m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
		resetEndpoint();
	}
	else
	{
		restartAccept();
	}
}


// 这里采用了对象池的操作，对象池在获取和归还对象的时候需要进行加锁操作。原本打算是对象池在对象不足的时候按需要进行分配新的对象，
// 但是从实际开发的角度来看，虽然在内存充足的情况下，对象可以接近无限地
//分配新的，但是应该认识到一点：带宽、数据库这些资源是有限的，用户连接多了，应该合理限制单机承载人数才可以确保用户使用体验，
// 否则如果一个网页体验大打折扣是没有意义的。因此目前的使用方式是对象池在
//初始化时一次性设定最大对象数量，一次性初始化好，当监听取不到新的socket资源时，就暂停监听，
// 利用回调函数通知对象池于下次对象归还时再通知开启监听。在没有对象可取的情况下再去尝试获取是没有任何意义的，
//因此在没有对象时，暂停监听，为归还让出锁资源，在下次归还时再重新监听。

//事实上尽快获取到对象的办法还包括在对象短缺的时候发送通知给所有处理类缩短超时时间，当然这点看有没有必要做


void listener::startAccept()
{
	try
	{
		std::shared_ptr<HTTPSERVICE> httpServiceTemp{  };
		m_httpServicePool->getNextBuffer(httpServiceTemp);

		if (httpServiceTemp && httpServiceTemp->m_buffer && httpServiceTemp->m_buffer->getSock())
		{
			m_acceptor->async_accept(*(httpServiceTemp->m_buffer->getSock()), [this, httpServiceTemp](const boost::system::error_code &err)
			{
				handleStartAccept(httpServiceTemp, err);
			});
		}
		else
		{
			m_log->writeLog(__FILE__, __LINE__, "notifySocketPool  ");
			notifySocketPool();
		}
	}
	catch (const std::exception &e)
	{
		//m_log->writeLog(__FILE__, __LINE__, e.what());
		m_log->writeLog(__FILE__, __LINE__, "restartAccept  ");
		restartAccept();
	}
}




void listener::handleStartAccept(std::shared_ptr<HTTPSERVICE> httpServiceTemp, const boost::system::error_code &err)
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
		restartAccept();
	}
}


void listener::getBackHTTPSERVICE(std::shared_ptr<HTTPSERVICE> &tempHs)
{
	if (tempHs )
	{
		m_httpServiceList->pop(tempHs->getListIter());
		m_httpServicePool->getBackElem(tempHs);
	}
}


void listener::restartAccept()
{
	if (m_startAccept.load())
	{
		startAccept();
	}

}


void listener::notifySocketPool()
{
	m_startAccept.store(false);
	m_httpServicePool->setNotifyAccept();
}



void listener::startRun()
{
	std::cout << "start listener\n";
	resetEndpoint();
	checkTimeOut();
}


void listener::reAccept()
{
	m_startAccept.store(true);
	restartAccept();
}


void listener::checkTimeOut()
{
	static std::function<void()>socketTimeOut{ [this]() {m_httpServiceList->check(); } };
	//在插入时间轮定时器失败的情况下，用listen层的定时器进行处理，双重保险
	//将所有socket对象的统一超时处理并入时间轮定时器中处理
	if (!m_timeWheel->insert(socketTimeOut, m_checkTurn))
	{
		m_timeOutTimer->expires_after(std::chrono::seconds(m_timeOut));
		m_timeOutTimer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{


			}
			else
			{
				m_httpServiceList->check();
			}
		});
	}
	
}


