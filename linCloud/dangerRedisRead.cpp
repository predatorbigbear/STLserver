#include "dangerRedisRead.h"


//每一次情况都应该需要考虑断线
DANGERREDISREAD::DANGERREDISREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, const unsigned int redisPort,
	const unsigned int memorySize,
	const unsigned int checkSize)
	:m_redisIP(redisIP), m_redisPort(redisPort), m_ioc(ioc), m_unlockFun(unlockFun),m_memorySize(memorySize),m_checkSize(checkSize)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is nullptr");
		if (!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (!REGEXFUNCTION::isVaildIpv4(redisIP))
			throw std::runtime_error("redisIP is invaild");
		if (redisPort > 65535)
			throw std::runtime_error("redisPort is invaild");
		if (!readyEndPoint())
			throw std::runtime_error("ready endpont error");
		if (!readySocket())
			throw std::runtime_error("ready socket error");
		if(!m_memorySize)
			throw std::runtime_error("memorySize is invaild");
		if(!checkSize || checkSize>= m_memorySize)
			throw std::runtime_error("checkSize is too small");
		m_receiveBuffer.reset(new char[m_memorySize]);
		connect();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}



bool DANGERREDISREAD::insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char**, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest)
{
	if (!redisRequest || !std::get<0>(*redisRequest) || !std::get<1>(*redisRequest) || !std::get<2>(*redisRequest) || !std::get<3>(*redisRequest) || !std::get<4>(*redisRequest))
		return false;
	if (!m_queryStatus.load())
	{
		m_queryStatus.store(true);       //直接进入处理函数

		m_redisRequest = redisRequest;

		m_data = std::get<2>(*m_redisRequest);

		//尝试查看是否有请求
		if (!readyQuery(std::get<0>(*m_redisRequest), std::get<1>(*m_redisRequest)))
		{
			if (!m_messageList.empty())
				tryCheckList();
			else
				m_queryStatus.store(false);

			return false;
		}

		//如果请求准备成功，第一次不用判断是否存在需要处理的返回消息
		startQuery();

		return true;
	}
	return m_messageList.insert(redisRequest);
}



bool DANGERREDISREAD::readyEndPoint()
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


bool DANGERREDISREAD::readySocket()
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
	catch (const boost::system::system_error &err)
	{
		return false;
	}
}



void DANGERREDISREAD::connect()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code &err)
	{
		if (err)
		{
			std::cout << err.value() << "  " << err.message() << '\n';
		}
		else
		{
			std::cout << "connect dangerRedisRead success\n";
			(*m_unlockFun)();
		}
	});
}



bool DANGERREDISREAD::readyQuery(const char * source, const unsigned int sourceLen)
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

		vector<const char*>::const_iterator iter{ m_vec.cbegin() };

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



//query之后将准备工作标志置位，receive成功时检查准备工作标志为true再做事情，否则不做任何处理
//在进行receive之前，将receive标志位置位，在准备工作完成后检查receive是否成功了，如未成功，则不做任何处理，如果成功了，那么在这里做处理

void DANGERREDISREAD::startQuery()
{
	boost::asio::async_write(*m_sock, boost::asio::buffer(m_ptr.get(), m_totalLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		if (err)  //log
		{
			try
			{
				if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_redisRequest, false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR)))
					std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
			}
			catch (const std::exception &e)
			{
				std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
			}
			if (!m_messageList.empty())
				tryCheckList();
			else
				m_queryStatus.store(false);
		}
		else
		{
			m_reciveStatus.store(false);
			startReceive();              
			//如果发送请求成功，在准备下一次请求之前将本次请求放进待处理队列中，如果待处理队列插入失败，那么应该设置一个标志位，等待本次消息处理完成再继续下一步，
			//如果发送请求成功，插入待处理队列失败的话，那么应该启动循环接收，将本次发送成功的消息处理完毕再继续下一步行动
			//首先判断待处理队列是否为空，如果不为空，应该首先将待处理队列的消息全部处理完毕，获取队列任务数，然后处理，当数目为0时即为本次应该处理的数据了，等该数据也接受处理完毕后，
			//再继续准备下一步redis接收工作，开启新的query
			//反之将代理队列数设置为0，直接监听等待本次消息处理完毕之后准备下一步redis接收工作，开启新的query
			//
			//插入成功则准备下一步redis接收工作
			if (!m_waitMessageList.insert(m_redisRequest))
			{
				//m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_redisRequest, false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR));
			}
			else
			{
				readyNextRedisQuery();
			}
		}
	});
}




void DANGERREDISREAD::query()
{
	boost::asio::async_write(*m_sock, boost::asio::buffer(m_ptr.get(), m_totalLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		if (err)  //log
		{
			try
			{
				if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_redisRequest, false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR)))
					std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
			}
			catch (const std::exception &e)
			{
				std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
			}
			checkList();
		}
		else
		{
			receive();
			readyNextRedisQuery();
		}
	});
}




void DANGERREDISREAD::startReceive()
{
	if (m_memorySize - m_receivePos < m_checkSize)
	{
		if (!(m_memorySize - m_receivePos))
		{
			m_receivePos = 0;
			m_receiveSize = m_checkSize;
		}
		else
			m_receiveSize = m_memorySize - m_receivePos;
	}
	else
		m_receiveSize = m_checkSize;
	m_sock->async_read_some(boost::asio::buffer(m_receiveBuffer.get() + m_receivePos, m_receiveSize), [this](const boost::system::error_code &err, const std::size_t size)
	{

		m_receivePos += size;
		m_reciveStatus.store(true);
		//无论下面成功与否，已经成功发送请求的消息应该处理到接收好消息为止，除非是短线意外情况
		//如果断线了，将有结果的请求调用回调函数，没有结果的或者是待请求队列的发送错误信息，调用回调函数


		/*
		if (err)
		{
			if(!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_redisRequest, false, ERRORMESSAGE::REDIS_ASYNC_READ_ERROR)))
				std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_READ_ERROR);
			//checkList();
		}
		else
		{
			//checkList();
		}
		*/
	});
}


void DANGERREDISREAD::receive()
{
	if (m_memorySize - m_receivePos < m_checkSize)
	{
		if (!(m_memorySize - m_receivePos))
			m_receivePos = 0;
		else
			m_receiveSize = m_memorySize - m_receivePos;
	}
	else
		m_receiveSize = m_checkSize;
	m_sock->async_read_some(boost::asio::buffer(m_receiveBuffer.get()+ m_receivePos, m_receiveSize), [this](const boost::system::error_code &err, const std::size_t size)
	{
		m_receivePos += size;
		if (err)
		{
			try
			{
				if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_redisRequest, false, ERRORMESSAGE::REDIS_ASYNC_READ_ERROR)))
					std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_READ_ERROR);
			}
			catch (const std::exception &e)
			{
				std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_READ_ERROR);
			}
			checkList();
		}
		else
		{
			checkList();
		}
	});
}



bool DANGERREDISREAD::checkRedisMessage(const char * source, const unsigned int sourceLen)
{
	if (!source || sourceLen < 2)
		return false;

	switch (*source)
	{
		case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING):

			m_errorMessage = ERRORMESSAGE::SIMPLE_STRING;
			break;

			case static_cast<int>(REDISPARSETYPE::ARRAY) :
				m_errorMessage = ERRORMESSAGE::ARRAY;
				break;

				case static_cast<int>(REDISPARSETYPE::ERROR) :
					m_errorMessage = ERRORMESSAGE::ERROR;
					break;

					case static_cast<int>(REDISPARSETYPE::INTERGER) :
						m_errorMessage = ERRORMESSAGE::INTERGER;
						break;

						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							m_errorMessage = ERRORMESSAGE::LOT_SIZE_STRING;
							break;

						default:
							return false;
	}


	return true;
}



void DANGERREDISREAD::checkList()
{
	if (m_nextRedisRequest)
	{
		m_redisRequest = m_nextRedisRequest;
		m_nextRedisRequest.reset();

		if (m_nextVaild)
		{
			m_data = std::get<2>(*m_redisRequest);
			query();
			//处理信息

			handleMessage();

			while (m_messageResponse.popTop(m_tempSINGLEMESSAGE))
			{
				(std::get<3>(*(m_tempSINGLEMESSAGE->m_tempRequest)))(m_tempSINGLEMESSAGE->m_tempVaild, m_tempSINGLEMESSAGE->m_tempEM);
			}
		}
		else
		{
			checkList();
		}
	}
	else
	{
		bool success{ false };

		while (m_messageList.popTop(m_nextRedisRequest))
		{
			if (!readyQuery(std::get<0>(*m_nextRedisRequest), std::get<1>(*m_nextRedisRequest)))
			{
				try
				{
					if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_nextRedisRequest, false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR)))
						std::get<3>(*m_nextRedisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
				}
				catch (const std::exception &e)
				{
					std::get<3>(*m_nextRedisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
				}
				continue;
			}

			success = true;
			break;
		}
		if (success)
		{
			m_redisRequest = m_nextRedisRequest;
			m_nextRedisRequest.reset();
			query();
			//处理信息

			handleMessage();



			while (m_messageResponse.popTop(m_tempSINGLEMESSAGE))
			{
				(std::get<3>(*(m_tempSINGLEMESSAGE->m_tempRequest)))(m_tempSINGLEMESSAGE->m_tempVaild, m_tempSINGLEMESSAGE->m_tempEM);
			}
		}
		else
		{
			//处理信息

			handleMessage();




			while (m_messageResponse.popTop(m_tempSINGLEMESSAGE))
			{
				(std::get<3>(*(m_tempSINGLEMESSAGE->m_tempRequest)))(m_tempSINGLEMESSAGE->m_tempVaild, m_tempSINGLEMESSAGE->m_tempEM);
			}
			m_nextRedisRequest.reset();
			m_queryStatus.store(false);
		}
	}
}



//第一次进来尝试，只要有可以进行发送查询的请求的可能，就延迟到异步等待时处理
void DANGERREDISREAD::tryCheckList()
{
	bool success{ false };
	while (m_messageList.popTop(m_redisRequest))
	{
		if (!readyQuery(std::get<0>(*m_redisRequest), std::get<1>(*m_redisRequest)))
		{
			try
			{
				if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_redisRequest, false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR)))
					std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
			}
			catch (const std::exception &e)
			{
				std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
			}
			continue;
		}

		success = true;
	}

	if (success)
	{
		startQuery();

		while (m_messageResponse.popTop(m_tempSINGLEMESSAGE))
		{
			(std::get<3>(*(m_tempSINGLEMESSAGE->m_tempRequest)))(m_tempSINGLEMESSAGE->m_tempVaild, m_tempSINGLEMESSAGE->m_tempEM);
		}
	}
	else
	{
		while (m_messageResponse.popTop(m_tempSINGLEMESSAGE))
		{
			(std::get<3>(*(m_tempSINGLEMESSAGE->m_tempRequest)))(m_tempSINGLEMESSAGE->m_tempVaild, m_tempSINGLEMESSAGE->m_tempEM);
		}
		m_queryStatus.store(false);
	}
}



void DANGERREDISREAD::readyNextRedisQuery()
{
	if (!m_nextRedisRequest)
	{
		bool success{ false };
		while(m_messageList.popTop(m_nextRedisRequest))
		{
			if (!readyQuery(std::get<0>(*m_nextRedisRequest), std::get<1>(*m_nextRedisRequest)))
			{
				try
				{
					if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(m_nextRedisRequest, false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR)))
						std::get<3>(*m_nextRedisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
				}
				catch (const std::exception &e)
				{
					std::get<3>(*m_nextRedisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
				}
				continue;
			}

			success = true;
			break;
		} 
		if (success)
		{
			m_nextVaild = true;
		}
		else
		{
			m_nextRedisRequest.reset();
		}
	}
}



void DANGERREDISREAD::handleMessage()
{
	if (m_lastReceivePos != m_receivePos)
	{
		if (m_lastReceivePos < m_receivePos)
		{
			
		}
		else
		{
		
		}
	}
}











/*
			const char *receiveBuffer{ m_receiveBuffer.get() + m_receivePos };
			m_receivePos += size;
			decltype(m_redisRequest)redisRequest{ m_redisRequest };
			if (!checkRedisMessage(receiveBuffer, size))
			{
			//	if (m_lastRedisRequest)
			//	{
			//		(std::get<3>(*m_lastRedisRequest))(m_lastVaild, m_lastEM);
			//	}
				m_lastRedisRequest = m_redisRequest;
				m_lastVaild = false;
				m_lastEM = ERRORMESSAGE::CHECK_REDIS_MESSAGE_ERROR;
			}
			else
			{
				const char **m_data = std::get<2>(*m_redisRequest);

				switch (m_errorMessage)
				{
				case ERRORMESSAGE::SIMPLE_STRING:
					*m_data++ = receiveBuffer + 1;
					*m_data++ = receiveBuffer + size - 2;
				//	if (m_lastRedisRequest)
				//	{
				//		(std::get<3>(*m_lastRedisRequest))(m_lastVaild, m_lastEM);
				//	}
					m_lastRedisRequest = m_redisRequest;
					m_lastVaild = true;
					m_lastEM = ERRORMESSAGE::SIMPLE_STRING;
					break;
				case ERRORMESSAGE::LOT_SIZE_STRING:
					*m_data++ = std::find(receiveBuffer, receiveBuffer + size, '\n') + 1;
					*m_data++ = receiveBuffer + size - 2;
				//	if (m_lastRedisRequest)
				//	{
				//		(std::get<3>(*m_lastRedisRequest))(m_lastVaild, m_lastEM);
				//	}
					m_lastRedisRequest = m_redisRequest;
					m_lastVaild = true;
					m_lastEM = ERRORMESSAGE::LOT_SIZE_STRING;
					break;
				case ERRORMESSAGE::ERROR:
					*m_data++ = receiveBuffer + 1;
					*m_data++ = receiveBuffer + size - 2;
					*m_data++ = std::find(receiveBuffer, receiveBuffer + size, '\n') + 1;
					*m_data++ = receiveBuffer + size - 2;
				//	if (m_lastRedisRequest)
				//	{
				//		(std::get<3>(*m_lastRedisRequest))(m_lastVaild, m_lastEM);
				//	}
					m_lastRedisRequest = m_redisRequest;
					m_lastVaild = true;
					m_lastEM = ERRORMESSAGE::ERROR;
					break;
				default:
					*m_data++ = receiveBuffer + 1;
					*m_data++ = receiveBuffer + size - 2;
				//	if (m_lastRedisRequest)
				//	{
				//		(std::get<3>(*m_lastRedisRequest))(m_lastVaild, m_lastEM);
				//	}
					m_lastRedisRequest = m_redisRequest;
					m_lastVaild = true;
					m_lastEM = ERRORMESSAGE::REDIS_DEFAULT;
					break;
				}
			}
			*/