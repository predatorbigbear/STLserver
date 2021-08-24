#include "redisRead.h"




REDISREAD::REDISREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string &redisIP, const unsigned int redisPort)
	:m_redisIP(redisIP),m_redisPort(redisPort), m_ioc(ioc),m_unlockFun(unlockFun)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is nullptr");
		if(!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if(!REGEXFUNCTION::isVaildIpv4(redisIP))
			throw std::runtime_error("redisIP is invaild");
		if (redisPort > 65535)
			throw std::runtime_error("redisPort is invaild");
		if(!readyEndPoint())
			throw std::runtime_error("ready endpont error");
		if (!readySocket())
			throw std::runtime_error("ready socket error");
		m_timer.reset(new boost::asio::steady_timer(*m_ioc));
		m_freeRedisResultFun.reset(new std::function<void()>(std::bind(&REDISREAD::checkList, this)));
		connect();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}


//插入首先判断参数是否合法，不合法则返回false
//第一次插入时如果插入失败，应该使用tryCheckList做特殊处理

bool REDISREAD::insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char**, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest)
{
	if (!redisRequest || !std::get<0>(*redisRequest) || !std::get<1>(*redisRequest) || !std::get<2>(*redisRequest) || !std::get<3>(*redisRequest) || !std::get<4>(*redisRequest))
		return false;
	if (!m_queryStatus.load())
	{
		m_queryStatus.store(true);       //直接进入处理函数

		m_redisRequest = redisRequest;

		m_data = std::get<2>(*m_redisRequest);

		//此时在readyQuery完成期间可能有新数据插入进来了,检查一下
		if (!readyQuery(std::get<0>(*m_redisRequest), std::get<1>(*m_redisRequest)))
		{
			checkList();                

			return false;
		}

		query();

		return true;
	}
	return m_messageList.insert(redisRequest);
}



std::shared_ptr<std::function<void()>> REDISREAD::getRedisUnlock()
{
	return m_freeRedisResultFun;
}




bool REDISREAD::readyEndPoint()
{
	try
	{
		m_endPoint.reset(new boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(m_redisIP), m_redisPort));
		return true;
	}
	catch (const std::exception &e)
	{
		return false;
	}
	catch (const boost::system::system_error &err)
	{
		return false;
	}
}



bool REDISREAD::readySocket()
{
	try
	{
		m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));
		return true;
	}
	catch (const std::exception &e)
	{
		return false;
	}
}


void REDISREAD::connect()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code &err)
	{
		if (err)
		{
			std::cout << err.value() << "  " << err.message() << '\n';
			m_timer->expires_after(std::chrono::seconds(1));
			m_timer->async_wait([this](const boost::system::error_code &err)
			{
				if (err)
				{
					//tryResetTimer();
				}
				else
				{
					connect();
				}
			});
		}
		else
		{
			std::cout << "connect redisRead success\n";
			(*m_unlockFun)();
		}
	});
}



bool REDISREAD::readyQuery(const char * source, const unsigned int sourceLen)
{
	try
	{
		unsigned int paraNum{}, len{}, totalLen{};

		const char *iterBegin{ source }, *iterEnd{ source + sourceLen }, *iterTempBegin, *iterTempEnd;

		do
		{
			if ((iterTempBegin = std::find_if(iterBegin, iterEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '))) == iterEnd)
				break;

			iterTempEnd = std::find(iterTempBegin + 1, iterEnd, ' ');

			++paraNum;

			len = std::distance(iterTempBegin, iterTempEnd);
			totalLen += len + 5;
			do
			{
				len /= 10;
				++totalLen;
			} while (len);

			m_vec.emplace_back(iterTempBegin);
			m_vec.emplace_back(iterTempEnd);

			iterBegin = iterTempEnd;
		} while (iterBegin != iterEnd);

		if (!paraNum)
			return false;

		len = paraNum;
		do
		{
			len /= 10;
			++totalLen;
		} while (len);
		totalLen += 3;

		if (m_ptrLen < totalLen)
		{
			m_ptr.reset(new char[totalLen]);
			m_ptrLen = totalLen;
		}

		char *uptr{ m_ptr.get() };

		*uptr++ = '*';
		if (paraNum > 99999999)
			*uptr++ = paraNum / 100000000 + '0';
		if (paraNum > 9999999)
			*uptr++ = paraNum / 10000000 % 10 + '0';
		if (paraNum > 999999)
			*uptr++ = paraNum / 1000000 % 10 + '0';
		if (paraNum > 99999)
			*uptr++ = paraNum / 100000 % 10 + '0';
		if (paraNum > 9999)
			*uptr++ = paraNum / 10000 % 10 + '0';
		if (paraNum > 999)
			*uptr++ = paraNum / 1000 % 10 + '0';
		if (paraNum > 99)
			*uptr++ = paraNum / 100 % 10 + '0';
		if (paraNum > 9)
			*uptr++ = paraNum / 10 % 10 + '0';
		*uptr++ = paraNum % 10 + '0';

		std::copy(STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen, uptr);
		uptr += STATICSTRING::smallNewlineLen;

		std::vector<const char*>::const_iterator iter{ m_vec.cbegin() };

		do
		{
			*uptr++ = '$';

			len = distance(*iter, *(iter + 1));
			if (len > 99999999)
				*uptr++ = len / 100000000 + '0';
			if (len > 9999999)
				*uptr++ = len / 10000000 % 10 + '0';
			if (len > 999999)
				*uptr++ = len / 1000000 % 10 + '0';
			if (len > 99999)
				*uptr++ = len / 100000 % 10 + '0';
			if (len > 9999)
				*uptr++ = len / 10000 % 10 + '0';
			if (len > 999)
				*uptr++ = len / 1000 % 10 + '0';
			if (len > 99)
				*uptr++ = len / 100 % 10 + '0';
			if (len > 9)
				*uptr++ = len / 10 % 10 + '0';
			*uptr++ = len % 10 + '0';
			std::copy(STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen, uptr);
			uptr += STATICSTRING::smallNewlineLen;
			std::copy(*iter, *(iter + 1), uptr);
			uptr += distance(*iter, *(iter + 1));
			std::copy(STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen, uptr);
			uptr += STATICSTRING::smallNewlineLen;

			iter += 2;
		} while (iter != m_vec.cend());

		m_totalLen = totalLen;
		m_vec.clear();

		return true;
	}
	catch (const std::exception &e)   //std::bad_alloc
	{
		m_vec.clear();
		return false;
	}
}



void REDISREAD::query()
{
	boost::asio::async_write(*m_sock, boost::asio::buffer(m_ptr.get(), m_totalLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		if (err)  //log
		{
			(std::get<3>(*m_redisRequest))(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
		}
		else
		{
			receive();
			readyNextRedisQuery();
		}
	});
}




void REDISREAD::receive()
{
	m_sock->async_read_some(boost::asio::buffer(m_receiveBuffer.get(), m_receiveLen), [this](const boost::system::error_code &err,const std::size_t size)
	{
		if (err)
		{
			(std::get<3>(*m_redisRequest))(false, ERRORMESSAGE::REDIS_ASYNC_READ_ERROR);
		}
		else
		{
			if(!checkRedisMessage(m_receiveBuffer.get(), size))
				(std::get<3>(*m_redisRequest))(false, ERRORMESSAGE::CHECK_REDIS_MESSAGE_ERROR);
			else
			{
				const char **m_data = std::get<2>(*m_redisRequest);
				
				switch (m_errorMessage)
				{
				case ERRORMESSAGE::SIMPLE_STRING:
					*m_data++ = m_receiveBuffer.get() + 1;
					*m_data++ = m_receiveBuffer.get() + size -2;
					break;
				case ERRORMESSAGE::LOT_SIZE_STRING:
					*m_data++ = std::find(m_receiveBuffer.get(), m_receiveBuffer.get() + size, '\n') + 1;
					*m_data++ = m_receiveBuffer.get() + size - 2;
					break;
				case ERRORMESSAGE::ERROR:
					*m_data++ = m_receiveBuffer.get() + 1;
					*m_data++ = m_receiveBuffer.get() + size - 2;
					break;
					default:
						*m_data++ = m_receiveBuffer.get() + 1;
						*m_data++ = m_receiveBuffer.get() + size - 2;
						break;
				}
				(std::get<3>(*m_redisRequest))(true, m_errorMessage);
			}
		}
	});
}



bool REDISREAD::checkRedisMessage(const char *source, const unsigned int sourceLen)
{
	if (!source || sourceLen < 2)
		return false;

	switch (*source)
	{
	case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING):
		m_errorMessage = ERRORMESSAGE::SIMPLE_STRING;
		break;

	case static_cast<int>(REDISPARSETYPE::ARRAY):
		m_errorMessage = ERRORMESSAGE::ARRAY;
		break;

	case static_cast<int>(REDISPARSETYPE::ERROR):
		m_errorMessage = ERRORMESSAGE::ERROR;
		break;

	case static_cast<int>(REDISPARSETYPE::INTERGER):
		m_errorMessage = ERRORMESSAGE::INTERGER;
		break;

	case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING):
		m_errorMessage = ERRORMESSAGE::LOT_SIZE_STRING;
		break;

	default:
		return false;
	}


	return true;
}




void REDISREAD::freeResult()
{
	checkList();
}



void REDISREAD::checkList()
{
	if (m_nextRedisRequest)
	{
		m_redisRequest = m_nextRedisRequest;
		m_nextRedisRequest.reset();

		if (m_nextVaild)
		{
			m_data = std::get<2>(*m_redisRequest);
			query();
		}
		else
		{
			(std::get<3>(*m_redisRequest))(false, m_nextEM);
		}
	}
	else
	{
		if (m_messageList.popTop(m_redisRequest))
		{
			m_data = std::get<2>(*m_redisRequest);

			if (!readyQuery(std::get<0>(*m_redisRequest), std::get<1>(*m_redisRequest)))
			{
				(std::get<3>(*m_redisRequest))(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
			}
			else
			{
				query();
			}
		}
		else
			m_queryStatus.store(false);
	}
}




void REDISREAD::readyNextRedisQuery()
{
	if (!m_nextRedisRequest)
	{
		if (m_messageList.popTop(m_nextRedisRequest))
		{
			if (!readyQuery(std::get<0>(*m_nextRedisRequest), std::get<1>(*m_nextRedisRequest)))
			{
				//  记录下错误
				m_nextVaild = false;
				m_nextEM = ERRORMESSAGE::REDIS_READY_QUERY_ERROR;
				m_nextRedisRequest.reset();
			}
			else
			{
				m_nextVaild = true;
			}
		}
		else
		{
			m_nextRedisRequest.reset();
		}
	}
}









