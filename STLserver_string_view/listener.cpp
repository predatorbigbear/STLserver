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
		if (m_isHttp)
		{
			restartAccept<HTTPSERVER>();
		}
		else
		{
			restartAccept<HTTPSSERVER>();
		}
	}
}



void listener::getBackHTTPSERVICE(std::shared_ptr<HTTPSERVICE>& tempHs)
{
	if (tempHs)
	{
		m_httpServiceList->pop(tempHs->getListIter());
		m_httpServicePool->getBackElem(tempHs);
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
	if (m_isHttp)
	{
		restartAccept<HTTPSERVER>();
	}
	else
	{
		restartAccept<HTTPSSERVER>();
	}
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


