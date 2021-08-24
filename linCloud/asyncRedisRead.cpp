#include "asyncRedisRead.h"

ASYNCREDISREAD::ASYNCREDISREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, const unsigned int redisPort,
	const unsigned int memorySize, const unsigned int checkSize)
	:m_redisIP(redisIP), m_redisPort(redisPort), m_ioc(ioc), m_unlockFun(unlockFun), m_memorySize(memorySize), m_checkSize(checkSize)
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
		if (!m_memorySize)
			throw std::runtime_error("memorySize is invaild");
		if (!checkSize || checkSize >= m_memorySize)
			throw std::runtime_error("checkSize is too small");
		m_timer.reset(new boost::asio::steady_timer(*m_ioc));
		m_receiveBuffer.reset(new char[m_memorySize]);
		firstConnect();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}



bool ASYNCREDISREAD::readyEndPoint()
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



bool ASYNCREDISREAD::readySocket()
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



void ASYNCREDISREAD::firstConnect()
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
					firstConnect();
				}
			});
		}
		else
		{
			std::cout << "connect dangerRedisRead success\n";
			m_connect.store(true);
			(*m_unlockFun)();
		}
	});
}



void ASYNCREDISREAD::setConnectSuccess()
{
	m_connect.store(true);
}



void ASYNCREDISREAD::setConnectFail()
{
	m_connect.store(false);
}




bool ASYNCREDISREAD::readyQuery(const char * source, const unsigned int sourceLen)
{
	try
	{
		m_totalLen = 0;

		unsigned int paraNum{}, len{}, totalLen{};

		const char *iterBegin{ source }, *iterEnd{ source + sourceLen }, *iterTempBegin{}, *iterTempEnd{};

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



bool ASYNCREDISREAD::tryCheckList()
{
	if(m_messageEmpty.load())
		return false;

	m_messageList.getMutex().lock();
	m_messageResponse.getMutex().lock();


	do
	{
		m_nextRedisRequest = *m_messageList.listSelf().begin();
		m_messageList.listSelf().pop_front();
		if (!readyQuery(std::get<0>(*m_nextRedisRequest), std::get<1>(*m_nextRedisRequest)))
		{
			try
			{
				if (!m_messageResponse.unsafeInsert(std::make_shared<SINGLEMESSAGE>(m_nextRedisRequest, false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR)))
					std::get<3>(*m_nextRedisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
			}
			catch (const std::exception &e)
			{
				std::get<3>(*m_nextRedisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
			}
			continue;
		}
		m_messageList.getMutex().unlock();
		m_messageResponse.getMutex().unlock();
		if(m_messageList.listSelf().empty())
			m_messageEmpty.store(true);
		return true;
	} while (!m_messageList.listSelf().empty());

	m_messageEmpty.store(true);
	m_nextRedisRequest.reset();
	m_messageList.getMutex().unlock();
	m_messageResponse.getMutex().unlock();
	return false;
}




//如果发送错误，err则首先判定为断线情况，那么启动重连机制，启动重连机制前，对三大队列进行处理，
//如果成功发送了，第一次无需做任何事情，因为请求还没有确认发送成功
void ASYNCREDISREAD::startQuery()
{
	m_writeStatus.store(false);   //第一次发送请求时这个可以注释掉，因为没有准备工作进行
	boost::asio::async_write(*m_sock, boost::asio::buffer(m_ptr.get(), m_totalLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		if (err)  //log
		{
			m_connect.store(false);  
			m_writeStatus.store(true);   //第一次发送请求时这个可以注释掉，因为没有准备工作进行

			resetData();
		}
		else
		{
			m_writeStatus.store(true);    //第一次发送请求时这个可以注释掉，因为没有准备工作进行

			startReceive();
		}
	});
}



void ASYNCREDISREAD::resetData()
{
	if (m_redisRequest)
	{
		std::get<3>(*m_redisRequest)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
		m_redisRequest.reset();
	}

	{
		if (!m_messageEmpty.load())
		{
			m_messageList.getMutex().lock();
			if (!m_messageList.listSelf().empty())
			{
				for (auto const &request : m_messageList.listSelf())
				{
					std::get<3>(*request)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
				}
				m_messageList.listSelf().clear();
			}
			m_messageList.getMutex().unlock();
			m_messageEmpty.store(true);
		}
	}


	{
		m_waitMessageList.getMutex().lock();
		if (!m_waitMessageList.listSelf().empty())
		{
			for (auto const &request : m_waitMessageList.listSelf())
			{
				std::get<3>(*request)(false, ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR);
			}
			m_waitMessageList.listSelf().clear();
		}
		m_waitMessageList.getMutex().unlock();
	}

	{
		m_messageResponse.getMutex().lock();
		if (!m_messageResponse.listSelf().empty())
		{
			for (auto const &request : m_messageResponse.listSelf())
			{
				std::get<3>(*request->m_tempRequest)(request->m_tempVaild, request->m_tempEM);
			}
			m_messageResponse.listSelf().clear();
		}
		m_messageResponse.getMutex().unlock();
	}

	m_thisReceiveLen = m_receivePos = m_lastReceivePos = 0;

	tryReadyEndPoint();
}




void ASYNCREDISREAD::tryReadyEndPoint()
{
	try
	{
		m_endPoint.reset(new boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(m_redisIP), m_redisPort));
		m_timer->expires_after(std::chrono::seconds(1));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{
				tryResetTimer();
			}
			else
			{
				tryReadySocket();
			}
		});
	}
	catch (const std::exception &e)
	{
		m_timer->expires_after(std::chrono::seconds(1));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{
				tryResetTimer();
			}
			else
			{
				tryReadyEndPoint();
			}
		});
	}
	catch (const boost::system::system_error &err)
	{
		m_timer->expires_after(std::chrono::seconds(1));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{
				tryResetTimer();
			}
			else
			{
				tryReadyEndPoint();
			}
		});
	}
}



void ASYNCREDISREAD::tryReadySocket()
{
	try
	{
		m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));
		m_timer->expires_after(std::chrono::seconds(1));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{
				tryResetTimer();
			}
			else
			{
				tryConnect();
			}
		});
	}
	catch (const std::exception &e)
	{
		m_timer->expires_after(std::chrono::seconds(1));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{
				tryResetTimer();
			}
			else
			{
				tryReadyEndPoint();
			}
		});
	}
	catch (const boost::system::system_error &err)
	{
		m_timer->expires_after(std::chrono::seconds(1));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)
			{
				tryResetTimer();
			}
			else
			{
				tryReadyEndPoint();
			}
		});
	}
}



void ASYNCREDISREAD::tryConnect()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code &err)
	{
		if (err)
		{
			tryReadyEndPoint();
		}
		else
		{
			m_queryStatus.store(false);
			m_connect.store(true);
		}
	});
}



void ASYNCREDISREAD::tryResetTimer()
{
	if (m_ioc)
	{
		m_timer.reset(new boost::asio::steady_timer(*m_ioc));
		tryReadyEndPoint();
	}
}




//准备工作完成后检查写入状态与连接状态
void ASYNCREDISREAD::startReadyRequest()
{
	m_readyRequest.store(false);

	tryCheckList();

	m_insertRequest = m_waitMessageList.insert(m_redisRequest);

	m_readyRequest.store(true);

	if (m_readStatus.load() && m_connect.load())
	{
		if (m_insertRequest)
		{
			//进行发送请求步骤，在异步等待期间尝试解析上次得到的数据，如果有数据则将结果存起来，
			//之后一次性将获取到结果的队列数据处理了  m_lastReceivePos   m_receivePos，注意特殊情况，比如 m_lastReceivePos在m_receivePos后面
			if (m_nextRedisRequest)
			{
				//std::cout << "save request success and ready next request success\n";
				m_redisRequest = m_nextRedisRequest;

				m_nextRedisRequest.reset();

				queryLoop();
			}
			else  // 没有下一次请求了，循环接收数据直到本次请求获取到结果为止，然后检测等待请求队列是否有元素再作下一步处理
			{
				//std::cout << "save request success and ready next request fail\n";
				//parseLoop = true;	
				if (startParse())    //待优化
				{
					////////////////                         封装成一个函数
					checkLoop();
					/////////////////////////////////
				}
				else
				{
					//开启循环读取处理，循环读取，并在异步读取等待期间处理结果队列,读取成功    
					//std::cout << "no\n";
					simpleReceive();
				}
			}
		}
		else  //保存本次请求失败，循环接收数据直到本次请求获取到结果为止，然后检测等待请求队列是否有元素再作下一步处理，首先查看已有的等待获取结果队列是否有结果获取完，然后获取本次的结果，如果中途不行就接收到行为止
		{
			//std::cout << "save request fail\n";
			handleMessageResponse();

			//简易处理等待请求队列
			if (StartParseWaitMessage())
			{
				//简易处理本次请求
				if (StartParseThisMessage())
				{
					checkLoop();
				}
				else
				{
					//循环获取结果直到本次请求被处理
					simpleReceiveThisMessage();
				}
			}
			else
			{
				//循环获取结果直到等待请求队列结束
				simpleReceiveWaitMessage();
			}
		}
	}
}



void ASYNCREDISREAD::startReadyParse()
{
	m_readyParse.store(false);

	parseStatus = startParse();

	m_readyParse.store(true);

	if (m_writeStatus.load() && m_connect.load())
	{
		startReceive();
	}
}


//需要做的处理：判断当前读取位置与上次处理位置是否重合，如果重合并，则尝试进行解析，如果没有任何一个满足的解析出现，则重置数据（后续添加）
//第一次读取，在异步等待期间尝试将成功发送的请求尝试插入待处理队列，同时准备好下一个请求
void ASYNCREDISREAD::startReceive()
{
	m_readStatus.store(false);
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
		//无论下面成功与否，已经成功发送请求的消息应该处理到接收好消息为止，除非是短线意外情况
		//如果断线了，将有结果的请求调用回调函数，没有结果的或者是待请求队列的发送错误信息，调用回调函数
		if (err)
		{
			m_connect.store(false);
			m_readStatus.store(true);
			resetData();
		}
		else
		{
			m_receivePos += size;
			m_readStatus.store(true);
			if (m_readyRequest.load())
			{
		
				if (m_insertRequest)
				{
					//进行发送请求步骤，在异步等待期间尝试解析上次得到的数据，如果有数据则将结果存起来，
					//之后一次性将获取到结果的队列数据处理了  m_lastReceivePos   m_receivePos，注意特殊情况，比如 m_lastReceivePos在m_receivePos后面
					if (m_nextRedisRequest)
					{
						//std::cout << "save request success and ready next request success\n";
						m_redisRequest = m_nextRedisRequest;

						m_nextRedisRequest.reset();

						queryLoop();
					}
					else  // 没有下一次请求了，循环接收数据直到本次请求获取到结果为止，然后检测等待请求队列是否有元素再作下一步处理
					{
						//std::cout << "save request success and ready next request fail\n";
						//parseLoop = true;	
						if (startParse())
						{
							////////////////                         封装成一个函数
							checkLoop();
							/////////////////////////////////
						}
						else
						{
							//开启循环读取处理，循环读取，并在异步读取等待期间处理结果队列,读取成功    
							//std::cout << "no\n";
							simpleReceive();
						}

					}
				}
				else  //保存本次请求失败，循环接收数据直到本次请求获取到结果为止，然后检测等待请求队列是否有元素再作下一步处理
				{
					//首先处理一次已经有结果的消息队列,           //简易处理有结果消息队列
					//然后循环获取结果直到等待请求队列结束        //解析简易版           simpleReceive 等待请求队列处理版
					//最后循环获取结果直到本次请求被处理          //解析本次请求简易版   simpleReceive 本次请求处理版
					//执行checkLoop函数
					//simpleReceive需要做一个特异化版本
					//std::cout << "save request fail\n";

					//简易处理有结果消息队列
					handleMessageResponse();

					//简易处理等待请求队列
					if (StartParseWaitMessage())
					{
						//简易处理本次请求
						if (StartParseThisMessage())
						{
							checkLoop();
						}
						else
						{
							//循环获取结果直到本次请求被处理
							simpleReceiveThisMessage();
						}
					}
					else
					{
						//循环获取结果直到等待请求队列结束
						simpleReceiveWaitMessage();
					}
				}
				
			}
		}
	});
	startReadyRequest();
}




bool ASYNCREDISREAD::startParse()
{
	handleMessageResponse();


	m_waitMessageList.getMutex().lock();
	if (m_waitMessageList.listSelf().empty())
	{
		m_waitMessageList.getMutex().unlock();
		return false;
	}

	int receiveLen = m_receivePos - m_lastReceivePos;
	if (receiveLen)    //  
	{
		//buffer没有超出
		if (receiveLen > 0)              
		{
			const char *iterBegin{ m_receiveBuffer.get() + m_lastReceivePos }, *iterEnd{ m_receiveBuffer.get() + m_receivePos };
			const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{};
			int thisLength{};
			bool success{ true };
			const char **m_data{};
			int index{ -1 }, num{ 1 };
			REDISPARSETYPE redisType{};
			while (iterBegin != iterEnd && success)
			{
				switch (*iterBegin)
				{
					case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
						redisType = REDISPARSETYPE::LOT_SIZE_STRING;
						if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
						{
							success = false;
							break;
						}
						if (*(iterBegin + 1) == '-')
						{
							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
							iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
							m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							m_waitMessageList.unsafePop();
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							continue;
						}

						//计算长度
						index = -1, num = 1;
						thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1),0, [&index, &num](auto &sum,auto const ch)
						{
							if (++index)num *= 10;
							return sum += (ch - '0')*num;
						});

						lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
						if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
						{
							success = false;
							break;
						}
						lssEnd= lssbegin+ thisLength;
						iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
						m_lastReceivePos = iterBegin - m_receiveBuffer.get();
						//处理结果,启动回调函数
						m_tempRequest = *(m_waitMessageList.listSelf().begin());
						m_data = std::get<2>(*m_tempRequest);
						*m_data++ = lssbegin;
						*m_data++ = lssEnd;
						(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
						m_waitMessageList.unsafePop();
						if (m_waitMessageList.listSelf().empty())
						{
							success = false;
							break;
						}
						break;
						case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
							redisType = REDISPARSETYPE::SIMPLE_STRING;
							break;

							case static_cast<int>(REDISPARSETYPE::ERROR) :
								redisType = REDISPARSETYPE::ERROR;
								break;

								case static_cast<int>(REDISPARSETYPE::INTERGER) :
									redisType = REDISPARSETYPE::INTERGER;
									break;

									case static_cast<int>(REDISPARSETYPE::ARRAY) :
										redisType = REDISPARSETYPE::ARRAY;
										break;

				default:
					redisType = REDISPARSETYPE::UNKNOWN_TYPE;
					break;
				}
			}


			success = m_waitMessageList.listSelf().empty();
			m_waitMessageList.getMutex().unlock();
			return success;
		}
		else  //   m_lastReceivePos  到buffer末尾   buffer开头到m_receivePos
		{
			receiveLen = m_memorySize - m_lastReceivePos + m_receivePos;
			if (m_lastReceivePos == m_memorySize)   //从0到m_receivePos
			{
				m_lastReceivePos = 0;
				const char *iterBegin{ m_receiveBuffer.get()  }, *iterEnd{ m_receiveBuffer.get() + m_receivePos };
				const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{};
				int thisLength{};
				bool success{ true };
				const char **m_data{};
				int index{ -1 }, num{ 1 };
				REDISPARSETYPE redisType{};
				while (iterBegin != iterEnd && success)
				{
					switch (*iterBegin)
					{
						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							redisType = REDISPARSETYPE::LOT_SIZE_STRING;
							if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
							{
								success = false;
								break;
							}
							if (*(iterBegin + 1) == '-')
							{
								m_tempRequest = *(m_waitMessageList.listSelf().begin());
								(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
								iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
								m_waitMessageList.unsafePop();
								if (m_waitMessageList.listSelf().empty())
								{
									success = false;
									break;
								}
								continue;
							}
							index = -1, num = 1;
							thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
							{
								if (++index)num *= 10;
								return sum += (ch - '0')*num;
							});
							lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
							if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
							{
								success = false;
								break;
							}
							lssEnd = lssbegin + thisLength;
							iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
							m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							//处理结果,启动回调函数
							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							m_data = std::get<2>(*m_tempRequest);
							*m_data++ = lssbegin;
							*m_data++ = lssEnd;
							(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
							m_waitMessageList.unsafePop();
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							break;
							case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
								redisType = REDISPARSETYPE::SIMPLE_STRING;
								break;

								case static_cast<int>(REDISPARSETYPE::ERROR) :
									redisType = REDISPARSETYPE::ERROR;
									break;

									case static_cast<int>(REDISPARSETYPE::INTERGER) :
										redisType = REDISPARSETYPE::INTERGER;
										break;

										case static_cast<int>(REDISPARSETYPE::ARRAY) :
											redisType = REDISPARSETYPE::ARRAY;
											break;

										default:
											redisType = REDISPARSETYPE::UNKNOWN_TYPE;
											break;
					}
				}


				success = m_waitMessageList.listSelf().empty();
				m_waitMessageList.getMutex().unlock();
				return success;
			}
			else    //从m_lastReceivePos到m_memorySize  从0到m_receivePos   最为复杂情况
			{
				const char *iterBegin{ m_receiveBuffer.get() + m_lastReceivePos }, *iterEnd{ m_receiveBuffer.get() + m_memorySize }, *newIterBegin{}, *newIterEnd{};
				const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{}, *newLineBegin{}, *newLineEnd{};
				int thisLength{};
				bool success{ true }, jump{false};
				const char **m_data{};
				int index{ -1 }, num{ 1 }, outLen{}, leftLen{}, rightLen{};
				REDISPARSETYPE redisType{};
				while (iterBegin != iterEnd && success && !jump)
				{
					switch (*iterBegin)
					{
						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							redisType = REDISPARSETYPE::LOT_SIZE_STRING;
							lengthBegin = iterBegin + 1;
							if (lengthBegin == iterEnd)
							{
								lengthBegin = m_receiveBuffer.get();
								jump = true;
							}
							if (!jump)    //获取长度
							{
								if ((lengthEnd = std::find(lengthBegin, iterEnd, '\r')) != iterEnd)
								{
									if (*(iterBegin + 1) == '-')
									{
										m_tempRequest = *(m_waitMessageList.listSelf().begin());
										(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
										iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
										m_lastReceivePos = iterBegin - m_receiveBuffer.get();
										m_waitMessageList.unsafePop();
										if (m_waitMessageList.listSelf().empty())
										{
											success = false;
											break;
										}
										continue;
									}
									index = -1, num = 1;
									thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(lengthBegin), 0, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
								}
								else if((lengthEnd = std::find(m_receiveBuffer.get(), m_receiveBuffer.get() + m_receivePos, '\r')) != (m_receiveBuffer.get() + m_receivePos))
								{
									if (std::distance(iterBegin, iterEnd) > 1)
									{
										if (*(iterBegin + 1) == '-')
										{
											m_tempRequest = *(m_waitMessageList.listSelf().begin());
											(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
											iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
											m_lastReceivePos = iterBegin - m_receiveBuffer.get();
											m_waitMessageList.unsafePop();
											if (m_waitMessageList.listSelf().empty())
											{
												success = false;
												break;
											}
											continue;
										}
									}
									else
									{
										//第一位
										if (*(m_receiveBuffer.get()) == '-')
										{
											m_tempRequest = *(m_waitMessageList.listSelf().begin());
											m_data = std::get<2>(*m_tempRequest);
											*m_data++ = lssbegin;
											*m_data++ = lssEnd;
											(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
											iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
											m_lastReceivePos = iterBegin - m_receiveBuffer.get();
											m_waitMessageList.unsafePop();
											if (m_waitMessageList.listSelf().empty())
											{
												success = false;
												break;
											}
											continue;
										}
									}
									index = -1, num = 1;
									thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(const_cast<const char*>(m_receiveBuffer.get())), 0, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
									thisLength = std::accumulate(std::make_reverse_iterator(iterEnd), std::make_reverse_iterator(lengthBegin), thisLength, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else    //获取长度
							{
								if ((lengthEnd = std::find(lengthBegin, const_cast<const char*>(m_receiveBuffer.get() + m_receivePos), '\r')) == (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								index = -1, num = 1;
								thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(lengthBegin), 0, [&index, &num](auto &sum, auto const ch)
								{
									if (++index)num *= 10;
									return sum += (ch - '0')*num;
								});
							}

							lssbegin = lengthEnd;
							if (!jump)  //获取字符串首位置
							{
								if ((lssbegin = std::find_if(lssbegin, iterEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) != iterEnd)
								{


								}
								else if ((lssbegin = std::find_if(m_receiveBuffer.get(), m_receiveBuffer.get() + m_receivePos, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) != (m_receiveBuffer.get() + m_receivePos))
								{
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if ((lssbegin = std::find_if(lssbegin, const_cast<const char*>(m_receiveBuffer.get() + m_receivePos), std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) == (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
							}

							lssEnd = lssbegin;
							//获取字符串尾部
							if (!jump)
							{
								if (lssEnd + thisLength < (m_receiveBuffer.get()+m_memorySize))
								{
									lssEnd = lssbegin + thisLength;
								}
								else if(lssEnd + thisLength - (m_receiveBuffer.get() + m_memorySize) < m_receivePos)
								{
									lssEnd = m_receiveBuffer.get() + (lssEnd + thisLength - (m_receiveBuffer.get() + m_memorySize));
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if (lssEnd + thisLength > (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								lssEnd += thisLength;
							}

							//寻找最后的\r\n

							newLineBegin = lssEnd;
							if (!jump)
							{
								if (newLineBegin + 2 < (m_receiveBuffer.get() + m_memorySize))
								{
									newLineEnd = newLineBegin + 2;
								}
								else if (newLineBegin + 2 - (m_receiveBuffer.get() + m_memorySize) < m_receivePos)
								{
									newLineEnd = m_receiveBuffer.get() + (newLineBegin + 2 - (m_receiveBuffer.get() + m_memorySize));
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if (newLineBegin + 2 > (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								newLineEnd = newLineBegin + 2;
							}


							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							m_data = std::get<2>(*m_tempRequest);
							if ((lssbegin - m_receiveBuffer.get()) > m_receivePos)
							{
								try
								{
									leftLen = m_memorySize - (lssbegin - m_receiveBuffer.get()), rightLen = lssEnd - m_receiveBuffer.get();
									outLen = leftLen + rightLen;
									if (m_outRangeSize < outLen)
									{
										m_outRangeBuffer.reset(new char[outLen]);
										m_outRangeSize = outLen;
									}
									std::copy(const_cast<char*>(lssbegin), const_cast<char*>(iterEnd), m_outRangeBuffer.get());
									std::copy(m_receiveBuffer.get(), const_cast<char*>(lssEnd), m_outRangeBuffer.get() + leftLen);
									*m_data++ = m_outRangeBuffer.get();
									*m_data++ = m_outRangeBuffer.get() + outLen;
								}
								catch (const std::exception &e)
								{
									m_outRangeSize = 0;
									*m_data++ = nullptr;
									*m_data++ = nullptr;
								}
							}
							else
							{
								*m_data++ = lssbegin;
								*m_data++ = lssEnd;
							}


							/*
							*m_data++ = lssbegin;
							if ((lssbegin - m_receiveBuffer.get()) > m_receivePos)
							{
								*m_data++ = m_receiveBuffer.get() + m_memorySize;
								*m_data++ = m_receiveBuffer.get();
							}
							*m_data++ = lssEnd;
							*/
							(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
							m_waitMessageList.unsafePop();
							if (!jump)
							{
								iterBegin = newLineEnd;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							}
							else
							{
								newIterBegin = newLineEnd;
								newIterEnd = m_receiveBuffer.get() + m_receivePos;
								m_lastReceivePos = newIterBegin - m_receiveBuffer.get();
							}
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							break;
							case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
								redisType = REDISPARSETYPE::SIMPLE_STRING;
								break;

								case static_cast<int>(REDISPARSETYPE::ERROR) :
									redisType = REDISPARSETYPE::ERROR;
									break;

									case static_cast<int>(REDISPARSETYPE::INTERGER) :
										redisType = REDISPARSETYPE::INTERGER;
										break;

										case static_cast<int>(REDISPARSETYPE::ARRAY) :
											redisType = REDISPARSETYPE::ARRAY;
											break;

										default:
											redisType = REDISPARSETYPE::UNKNOWN_TYPE;
											break;
					}
				}


				if (jump && success)
				{
					iterBegin = newIterBegin, iterEnd = newIterEnd;
					while (iterBegin != iterEnd && success)
					{
						switch (*iterBegin)
						{
							case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
								redisType = REDISPARSETYPE::LOT_SIZE_STRING;
								if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
								{
									success = false;
									break;
								}
								if (*(iterBegin + 1) == '-')
								{
									m_tempRequest = *(m_waitMessageList.listSelf().begin());
									(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
									iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
									m_lastReceivePos = iterBegin - m_receiveBuffer.get();
									m_waitMessageList.unsafePop();
									if (m_waitMessageList.listSelf().empty())
									{
										success = false;
										break;
									}
									continue;
								}
								index = -1, num = 1;
								thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
								{
									if (++index)num *= 10;
									return sum += (ch - '0')*num;
								});
								lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
								if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
								{
									success = false;
									break;
								}
								lssEnd = lssbegin + thisLength;
								iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
								//处理结果,启动回调函数
								m_tempRequest = *(m_waitMessageList.listSelf().begin());
								m_data = std::get<2>(*m_tempRequest);
								*m_data++ = lssbegin;
								*m_data++ = lssEnd;
								(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
								m_waitMessageList.unsafePop();
								if (m_waitMessageList.listSelf().empty())
								{
									success = false;
									break;
								}
								break;
								case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
									redisType = REDISPARSETYPE::SIMPLE_STRING;
									break;

									case static_cast<int>(REDISPARSETYPE::ERROR) :
										redisType = REDISPARSETYPE::ERROR;
										break;

										case static_cast<int>(REDISPARSETYPE::INTERGER) :
											redisType = REDISPARSETYPE::INTERGER;
											break;

											case static_cast<int>(REDISPARSETYPE::ARRAY) :
												redisType = REDISPARSETYPE::ARRAY;
												break;

											default:
												redisType = REDISPARSETYPE::UNKNOWN_TYPE;
												break;
						}
					}
				}

				success = m_waitMessageList.listSelf().empty();
				m_waitMessageList.getMutex().unlock();
				return success;
			}

////////////////////////////////////////////////////////////////////////////////////////////////
		}
	}

	m_waitMessageList.getMutex().unlock();
	return false;
}



bool ASYNCREDISREAD::StartParseWaitMessage()
{
	m_waitMessageList.getMutex().lock();
	if (m_waitMessageList.listSelf().empty())
	{
		m_waitMessageList.getMutex().unlock();
		return true;
	}

	int receiveLen = m_receivePos - m_lastReceivePos;
	if (receiveLen)    //  
	{
		//buffer没有超出
		if (receiveLen > 0)
		{
			const char *iterBegin{ m_receiveBuffer.get() + m_lastReceivePos }, *iterEnd{ m_receiveBuffer.get() + m_receivePos };
			const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{};
			int thisLength{};
			bool success{ true };
			const char **m_data{};
			int index{ -1 }, num{ 1 };
			REDISPARSETYPE redisType{};
			while (iterBegin != iterEnd && success)
			{
				switch (*iterBegin)
				{
					case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
						redisType = REDISPARSETYPE::LOT_SIZE_STRING;
						if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
						{
							success = false;
							break;
						}
						if (*(iterBegin + 1) == '-')
						{
							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
							iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
							m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							m_waitMessageList.unsafePop();
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							continue;
						}
						index = -1, num = 1;
						thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
						{
							if (++index)num *= 10;
							return sum += (ch - '0')*num;
						});
						lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
						if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
						{
							success = false;
							break;
						}
						lssEnd = lssbegin + thisLength;
						iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
						m_lastReceivePos = iterBegin - m_receiveBuffer.get();
						//处理结果,启动回调函数
						m_tempRequest = *(m_waitMessageList.listSelf().begin());
						m_data = std::get<2>(*m_tempRequest);
						*m_data++ = lssbegin;
						*m_data++ = lssEnd;
						(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
						m_waitMessageList.unsafePop();
						if (m_waitMessageList.listSelf().empty())
						{
							success = false;
							break;
						}
						break;
						case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
							redisType = REDISPARSETYPE::SIMPLE_STRING;
							break;

							case static_cast<int>(REDISPARSETYPE::ERROR) :
								redisType = REDISPARSETYPE::ERROR;
								break;

								case static_cast<int>(REDISPARSETYPE::INTERGER) :
									redisType = REDISPARSETYPE::INTERGER;
									break;

									case static_cast<int>(REDISPARSETYPE::ARRAY) :
										redisType = REDISPARSETYPE::ARRAY;
										break;

									default:
										redisType = REDISPARSETYPE::UNKNOWN_TYPE;
										break;
				}
			}

			success = m_waitMessageList.listSelf().empty();
			m_waitMessageList.getMutex().unlock();
			return success;
		}
		else  //   m_lastReceivePos  到buffer末尾   buffer开头到m_receivePos
		{
			receiveLen = m_memorySize - m_lastReceivePos + m_receivePos;
			if (m_lastReceivePos == m_memorySize)   //从0到m_receivePos
			{
				m_lastReceivePos = 0;
				const char *iterBegin{ m_receiveBuffer.get() }, *iterEnd{ m_receiveBuffer.get() + m_receivePos };
				const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{};
				int thisLength{};
				bool success{ true };
				const char **m_data{};
				int index{ -1 }, num{ 1 };
				REDISPARSETYPE redisType{};
				while (iterBegin != iterEnd && success)
				{
					switch (*iterBegin)
					{
						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							redisType = REDISPARSETYPE::LOT_SIZE_STRING;
							if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
							{
								success = false;
								break;
							}
							if (*(iterBegin + 1) == '-')
							{
								m_tempRequest = *(m_waitMessageList.listSelf().begin());
								(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
								iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
								m_waitMessageList.unsafePop();
								if (m_waitMessageList.listSelf().empty())
								{
									success = false;
									break;
								}
								continue;
							}
							index = -1, num = 1;
							thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
							{
								if (++index)num *= 10;
								return sum += (ch - '0')*num;
							});
							lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
							if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
							{
								success = false;
								break;
							}
							lssEnd = lssbegin + thisLength;
							iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
							m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							//处理结果,启动回调函数
							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							m_data = std::get<2>(*m_tempRequest);
							*m_data++ = lssbegin;
							*m_data++ = lssEnd;
							(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
							m_waitMessageList.unsafePop();
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							break;
							case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
								redisType = REDISPARSETYPE::SIMPLE_STRING;
								break;

								case static_cast<int>(REDISPARSETYPE::ERROR) :
									redisType = REDISPARSETYPE::ERROR;
									break;

									case static_cast<int>(REDISPARSETYPE::INTERGER) :
										redisType = REDISPARSETYPE::INTERGER;
										break;

										case static_cast<int>(REDISPARSETYPE::ARRAY) :
											redisType = REDISPARSETYPE::ARRAY;
											break;

										default:
											redisType = REDISPARSETYPE::UNKNOWN_TYPE;
											break;
					}
				}

				success = m_waitMessageList.listSelf().empty();
				m_waitMessageList.getMutex().unlock();
				return success;
			}
			else    //从m_lastReceivePos到m_memorySize  从0到m_receivePos   最为复杂情况
			{
				const char *iterBegin{ m_receiveBuffer.get() + m_lastReceivePos }, *iterEnd{ m_receiveBuffer.get() + m_memorySize }, *newIterBegin{}, *newIterEnd{};
				const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{}, *newLineBegin{}, *newLineEnd{};
				int thisLength{};
				bool success{ true }, jump{ false };
				const char **m_data{};
				int index{ -1 }, num{ 1 }, outLen{}, leftLen{}, rightLen{};
				REDISPARSETYPE redisType{};
				while (iterBegin != iterEnd && success && !jump)
				{
					switch (*iterBegin)
					{
						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							redisType = REDISPARSETYPE::LOT_SIZE_STRING;
							lengthBegin = iterBegin + 1;
							if (lengthBegin == iterEnd)
							{
								lengthBegin = m_receiveBuffer.get();
								jump = true;
							}
							if (!jump)    //获取长度
							{
								if ((lengthEnd = std::find(lengthBegin, iterEnd, '\r')) != iterEnd)
								{
									if (*(iterBegin + 1) == '-')
									{
										m_tempRequest = *(m_waitMessageList.listSelf().begin());
										(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
										iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
										m_lastReceivePos = iterBegin - m_receiveBuffer.get();
										m_waitMessageList.unsafePop();
										if (m_waitMessageList.listSelf().empty())
										{
											success = false;
											break;
										}
										continue;
									}
									index = -1, num = 1;
									thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(lengthBegin), 0, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
								}
								else if ((lengthEnd = std::find(m_receiveBuffer.get(), m_receiveBuffer.get() + m_receivePos, '\r')) != (m_receiveBuffer.get() + m_receivePos))
								{
									if (std::distance(iterBegin, iterEnd) > 1)
									{
										if (*(iterBegin + 1) == '-')
										{
											m_tempRequest = *(m_waitMessageList.listSelf().begin());
											(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
											iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
											m_lastReceivePos = iterBegin - m_receiveBuffer.get();
											m_waitMessageList.unsafePop();
											if (m_waitMessageList.listSelf().empty())
											{
												success = false;
												break;
											}
											continue;
										}
									}
									else
									{
										//第一位
										if (*(m_receiveBuffer.get()) == '-')
										{
											m_tempRequest = *(m_waitMessageList.listSelf().begin());
											(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
											iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
											m_lastReceivePos = iterBegin - m_receiveBuffer.get();
											m_waitMessageList.unsafePop();
											if (m_waitMessageList.listSelf().empty())
											{
												success = false;
												break;
											}
											continue;
										}
									}
									index = -1, num = 1;
									thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(const_cast<const char*>(m_receiveBuffer.get())), 0, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
									thisLength = std::accumulate(std::make_reverse_iterator(iterEnd), std::make_reverse_iterator(lengthBegin), thisLength, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else    //获取长度
							{
								if ((lengthEnd = std::find(lengthBegin, const_cast<const char*>(m_receiveBuffer.get() + m_receivePos), '\r')) == (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								index = -1, num = 1;
								thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(lengthBegin), 0, [&index, &num](auto &sum, auto const ch)
								{
									if (++index)num *= 10;
									return sum += (ch - '0')*num;
								});
							}

							lssbegin = lengthEnd;
							if (!jump)  //获取字符串首位置
							{
								if ((lssbegin = std::find_if(lssbegin, iterEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) != iterEnd)
								{


								}
								else if ((lssbegin = std::find_if(m_receiveBuffer.get(), m_receiveBuffer.get() + m_receivePos, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) != (m_receiveBuffer.get() + m_receivePos))
								{
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if ((lssbegin = std::find_if(lssbegin, const_cast<const char*>(m_receiveBuffer.get() + m_receivePos), std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) == (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
							}

							lssEnd = lssbegin;
							//获取字符串尾部
							if (!jump)
							{
								if (lssEnd + thisLength < (m_receiveBuffer.get() + m_memorySize))
								{
									lssEnd = lssbegin + thisLength;
								}
								else if (lssEnd + thisLength - (m_receiveBuffer.get() + m_memorySize) < m_receivePos)
								{
									lssEnd = m_receiveBuffer.get() + (lssEnd + thisLength - (m_receiveBuffer.get() + m_memorySize));
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if (lssEnd + thisLength > (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								lssEnd += thisLength;
							}

							//寻找最后的\r\n

							newLineBegin = lssEnd;
							if (!jump)
							{
								if (newLineBegin + 2 < (m_receiveBuffer.get() + m_memorySize))
								{
									newLineEnd = newLineBegin + 2;
								}
								else if (newLineBegin + 2 - (m_receiveBuffer.get() + m_memorySize) < m_receivePos)
								{
									newLineEnd = m_receiveBuffer.get() + (newLineBegin + 2 - (m_receiveBuffer.get() + m_memorySize));
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if (newLineBegin + 2 > (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								newLineEnd = newLineBegin + 2;
							}


							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							m_data = std::get<2>(*m_tempRequest);
							if ((lssbegin - m_receiveBuffer.get()) > m_receivePos)
							{
								try
								{
									leftLen = m_memorySize - (lssbegin - m_receiveBuffer.get()), rightLen = lssEnd - m_receiveBuffer.get();
									outLen = leftLen + rightLen;
									if (m_outRangeSize < outLen)
									{
										m_outRangeBuffer.reset(new char[outLen]);
										m_outRangeSize = outLen;
									}
									std::copy(const_cast<char*>(lssbegin), const_cast<char*>(iterEnd), m_outRangeBuffer.get());
									std::copy(m_receiveBuffer.get(), const_cast<char*>(lssEnd), m_outRangeBuffer.get() + leftLen);
									*m_data++ = m_outRangeBuffer.get();
									*m_data++ = m_outRangeBuffer.get() + outLen;
								}
								catch (const std::exception &e)
								{
									m_outRangeSize = 0;
									*m_data++ = nullptr;
									*m_data++ = nullptr;
								}
							}
							else
							{
								*m_data++ = lssbegin;
								*m_data++ = lssEnd;
							}




							/*
							*m_data++ = lssbegin;
							if ((lssbegin - m_receiveBuffer.get()) > m_receivePos)
							{
								*m_data++ = m_receiveBuffer.get() + m_memorySize;
								*m_data++ = m_receiveBuffer.get();
							}
							*m_data++ = lssEnd;
							*/
							(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
							m_waitMessageList.unsafePop();
							if (!jump)
							{
								iterBegin = newLineEnd;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							}
							else
							{
								newIterBegin = newLineEnd;
								newIterEnd = m_receiveBuffer.get() + m_receivePos;
								m_lastReceivePos = newIterBegin - m_receiveBuffer.get();
							}
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							break;
							case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
								redisType = REDISPARSETYPE::SIMPLE_STRING;
								break;

								case static_cast<int>(REDISPARSETYPE::ERROR) :
									redisType = REDISPARSETYPE::ERROR;
									break;

									case static_cast<int>(REDISPARSETYPE::INTERGER) :
										redisType = REDISPARSETYPE::INTERGER;
										break;

										case static_cast<int>(REDISPARSETYPE::ARRAY) :
											redisType = REDISPARSETYPE::ARRAY;
											break;

										default:
											redisType = REDISPARSETYPE::UNKNOWN_TYPE;
											break;
					}
				}


				if (jump && success)
				{
					iterBegin = newIterBegin, iterEnd = newIterEnd;
					while (iterBegin != iterEnd && success)
					{
						switch (*iterBegin)
						{
							case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
								redisType = REDISPARSETYPE::LOT_SIZE_STRING;
								if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
								{
									success = false;
									break;
								}
								if (*(iterBegin + 1) == '-')
								{
									m_tempRequest = *(m_waitMessageList.listSelf().begin());
									(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
									iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
									m_lastReceivePos = iterBegin - m_receiveBuffer.get();
									m_waitMessageList.unsafePop();
									if (m_waitMessageList.listSelf().empty())
									{
										success = false;
										break;
									}
									continue;
								}
								index = -1, num = 1;
								thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
								{
									if (++index)num *= 10;
									return sum += (ch - '0')*num;
								});
								lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
								if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
								{
									success = false;
									break;
								}
								lssEnd = lssbegin + thisLength;
								iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
								//处理结果,启动回调函数
								m_tempRequest = *(m_waitMessageList.listSelf().begin());
								m_data = std::get<2>(*m_tempRequest);
								*m_data++ = lssbegin;
								*m_data++ = lssEnd;
								(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
								m_waitMessageList.unsafePop();
								if (m_waitMessageList.listSelf().empty())
								{
									success = false;
									break;
								}
								break;
								case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
									redisType = REDISPARSETYPE::SIMPLE_STRING;
									break;

									case static_cast<int>(REDISPARSETYPE::ERROR) :
										redisType = REDISPARSETYPE::ERROR;
										break;

										case static_cast<int>(REDISPARSETYPE::INTERGER) :
											redisType = REDISPARSETYPE::INTERGER;
											break;

											case static_cast<int>(REDISPARSETYPE::ARRAY) :
												redisType = REDISPARSETYPE::ARRAY;
												break;

											default:
												redisType = REDISPARSETYPE::UNKNOWN_TYPE;
												break;
						}
					}
				}

				success = m_waitMessageList.listSelf().empty();
				m_waitMessageList.getMutex().unlock();
				return success;
			}

		}
	}

	m_waitMessageList.getMutex().unlock();
	return false;
}




bool ASYNCREDISREAD::StartParseThisMessage()
{
	int receiveLen = m_receivePos - m_lastReceivePos;
	if (receiveLen)    //  
	{
		//buffer没有超出
		if (receiveLen > 0)
		{
			const char *iterBegin{ m_receiveBuffer.get() + m_lastReceivePos }, *iterEnd{ m_receiveBuffer.get() + m_receivePos };
			const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{};
			int thisLength{};
			bool success{ true };
			const char **m_data{};
			int index{ -1 }, num{ 1 };
			REDISPARSETYPE redisType{};
			while (iterBegin != iterEnd && success)
			{
				switch (*iterBegin)
				{
					case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
						redisType = REDISPARSETYPE::LOT_SIZE_STRING;
						if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
						{
							success = false;
							break;
						}
						if (*(iterBegin + 1) == '-')
						{
							m_tempRequest = *(m_waitMessageList.listSelf().begin());
							(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
							iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
							m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							m_waitMessageList.unsafePop();
							if (m_waitMessageList.listSelf().empty())
							{
								success = false;
								break;
							}
							continue;
						}
						index = -1, num = 1;
						thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
						{
							if (++index)num *= 10;
							return sum += (ch - '0')*num;
						});
						lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
						if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
						{
							success = false;
							break;
						}
						lssEnd = lssbegin + thisLength;
						iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
						m_lastReceivePos = iterBegin - m_receiveBuffer.get();
						//处理结果,启动回调函数
						m_data = std::get<2>(*m_redisRequest);
						*m_data++ = lssbegin;
						*m_data++ = lssEnd;
						(std::get<3>(*m_redisRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
						m_redisRequest.reset();
						success = false;
						break;
						case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
							redisType = REDISPARSETYPE::SIMPLE_STRING;
							break;

							case static_cast<int>(REDISPARSETYPE::ERROR) :
								redisType = REDISPARSETYPE::ERROR;
								break;

								case static_cast<int>(REDISPARSETYPE::INTERGER) :
									redisType = REDISPARSETYPE::INTERGER;
									break;

									case static_cast<int>(REDISPARSETYPE::ARRAY) :
										redisType = REDISPARSETYPE::ARRAY;
										break;

									default:
										redisType = REDISPARSETYPE::UNKNOWN_TYPE;
										break;
				}
			}


			return !m_redisRequest;
		}
		else  //   m_lastReceivePos  到buffer末尾   buffer开头到m_receivePos
		{
			receiveLen = m_memorySize - m_lastReceivePos + m_receivePos;
			if (m_lastReceivePos == m_memorySize)   //从0到m_receivePos
			{
				m_lastReceivePos = 0;
				const char *iterBegin{ m_receiveBuffer.get() }, *iterEnd{ m_receiveBuffer.get() + m_receivePos };
				const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{};
				int thisLength{};
				bool success{ true };
				const char **m_data{};
				int index{ -1 }, num{ 1 };
				REDISPARSETYPE redisType{};
				while (iterBegin != iterEnd && success)
				{
					switch (*iterBegin)
					{
						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							redisType = REDISPARSETYPE::LOT_SIZE_STRING;
							if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
							{
								success = false;
								break;
							}
							if (*(iterBegin + 1) == '-')
							{
								m_tempRequest = *(m_waitMessageList.listSelf().begin());
								(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
								iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
								m_waitMessageList.unsafePop();
								if (m_waitMessageList.listSelf().empty())
								{
									success = false;
									break;
								}
								continue;
							}
							index = -1, num = 1;
							thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
							{
								if (++index)num *= 10;
								return sum += (ch - '0')*num;
							});
							lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
							if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
							{
								success = false;
								break;
							}
							lssEnd = lssbegin + thisLength;
							iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
							m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							//处理结果,启动回调函数
							m_data = std::get<2>(*m_redisRequest);
							*m_data++ = lssbegin;
							*m_data++ = lssEnd;
							(std::get<3>(*m_redisRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
							m_redisRequest.reset();
							success = false;
							break;
							case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
								redisType = REDISPARSETYPE::SIMPLE_STRING;
								break;

								case static_cast<int>(REDISPARSETYPE::ERROR) :
									redisType = REDISPARSETYPE::ERROR;
									break;

									case static_cast<int>(REDISPARSETYPE::INTERGER) :
										redisType = REDISPARSETYPE::INTERGER;
										break;

										case static_cast<int>(REDISPARSETYPE::ARRAY) :
											redisType = REDISPARSETYPE::ARRAY;
											break;

										default:
											redisType = REDISPARSETYPE::UNKNOWN_TYPE;
											break;
					}
				}

				return !m_redisRequest;
			}
			else    //从m_lastReceivePos到m_memorySize  从0到m_receivePos   最为复杂情况
			{
				const char *iterBegin{ m_receiveBuffer.get() + m_lastReceivePos }, *iterEnd{ m_receiveBuffer.get() + m_memorySize }, *newIterBegin{}, *newIterEnd{};
				const char *iterFindBegin{ iterBegin }, *iterFindEnd{}, *lengthBegin{}, *lengthEnd{}, *lssbegin{}, *lssEnd{}, *newLineBegin{}, *newLineEnd{};
				int thisLength{};
				bool success{ true }, jump{ false };
				const char **m_data{};
				int index{ -1 }, num{ 1 }, outLen{}, leftLen{}, rightLen{};
				REDISPARSETYPE redisType{};
				while (iterBegin != iterEnd && success && !jump)
				{
					switch (*iterBegin)
					{
						case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
							redisType = REDISPARSETYPE::LOT_SIZE_STRING;
							lengthBegin = iterBegin + 1;
							if (lengthBegin == iterEnd)
							{
								lengthBegin = m_receiveBuffer.get();
								jump = true;
							}
							if (!jump)    //获取长度
							{
								if ((lengthEnd = std::find(lengthBegin, iterEnd, '\r')) != iterEnd)
								{
									if (*(iterBegin + 1) == '-')
									{
										m_tempRequest = *(m_waitMessageList.listSelf().begin());
										(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
										iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
										m_lastReceivePos = iterBegin - m_receiveBuffer.get();
										m_waitMessageList.unsafePop();
										if (m_waitMessageList.listSelf().empty())
										{
											success = false;
											break;
										}
										continue;
									}
									index = -1, num = 1;
									thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(lengthBegin), 0, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
								}
								else if ((lengthEnd = std::find(m_receiveBuffer.get(), m_receiveBuffer.get() + m_receivePos, '\r')) != (m_receiveBuffer.get() + m_receivePos))
								{
									if (std::distance(iterBegin, iterEnd) > 1)
									{
										if (*(iterBegin + 1) == '-')
										{
											m_tempRequest = *(m_waitMessageList.listSelf().begin());
											(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
											iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
											m_lastReceivePos = iterBegin - m_receiveBuffer.get();
											m_waitMessageList.unsafePop();
											if (m_waitMessageList.listSelf().empty())
											{
												success = false;
												break;
											}
											continue;
										}
									}
									else
									{
										//第一位
										if (*(m_receiveBuffer.get()) == '-')
										{
											m_tempRequest = *(m_waitMessageList.listSelf().begin());
											(std::get<3>(*m_tempRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
											iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
											m_lastReceivePos = iterBegin - m_receiveBuffer.get();
											m_waitMessageList.unsafePop();
											if (m_waitMessageList.listSelf().empty())
											{
												success = false;
												break;
											}
											continue;
										}
									}
									index = -1, num = 1;
									thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(const_cast<const char*>(m_receiveBuffer.get())), 0, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
									thisLength = std::accumulate(std::make_reverse_iterator(iterEnd), std::make_reverse_iterator(lengthBegin), thisLength, [&index, &num](auto &sum, auto const ch)
									{
										if (++index)num *= 10;
										return sum += (ch - '0')*num;
									});
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else    //获取长度
							{
								if ((lengthEnd = std::find(lengthBegin, const_cast<const char*>(m_receiveBuffer.get() + m_receivePos), '\r')) == (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								index = -1, num = 1;
								thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(lengthBegin), 0, [&index, &num](auto &sum, auto const ch)
								{
									if (++index)num *= 10;
									return sum += (ch - '0')*num;
								});
							}

							lssbegin = lengthEnd;
							if (!jump)  //获取字符串首位置
							{
								if ((lssbegin = std::find_if(lssbegin, iterEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) != iterEnd)
								{


								}
								else if ((lssbegin = std::find_if(m_receiveBuffer.get(), m_receiveBuffer.get() + m_receivePos, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) != (m_receiveBuffer.get() + m_receivePos))
								{
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if ((lssbegin = std::find_if(lssbegin, const_cast<const char*>(m_receiveBuffer.get() + m_receivePos), std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')))) == (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
							}

							lssEnd = lssbegin;
							//获取字符串尾部
							if (!jump)
							{
								if (lssEnd + thisLength < (m_receiveBuffer.get() + m_memorySize))
								{
									lssEnd = lssbegin + thisLength;
								}
								else if (lssEnd + thisLength - (m_receiveBuffer.get() + m_memorySize) < m_receivePos)
								{
									lssEnd = m_receiveBuffer.get() + (lssEnd + thisLength - (m_receiveBuffer.get() + m_memorySize));
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if (lssEnd + thisLength > (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								lssEnd += thisLength;
							}

							//寻找最后的\r\n

							newLineBegin = lssEnd;
							if (!jump)
							{
								if (newLineBegin + 2 < (m_receiveBuffer.get() + m_memorySize))
								{
									newLineEnd = newLineBegin + 2;
								}
								else if (newLineBegin + 2 - (m_receiveBuffer.get() + m_memorySize) < m_receivePos)
								{
									newLineEnd = m_receiveBuffer.get() + (newLineBegin + 2 - (m_receiveBuffer.get() + m_memorySize));
									jump = true;
								}
								else
								{
									success = false;
									break;
								}
							}
							else
							{
								if (newLineBegin + 2 > (m_receiveBuffer.get() + m_receivePos))
								{
									success = false;
									break;
								}
								newLineEnd = newLineBegin + 2;
							}

							m_data = std::get<2>(*m_redisRequest);
							if ((lssbegin - m_receiveBuffer.get()) > m_receivePos)
							{
								try
								{
									leftLen = m_memorySize - (lssbegin - m_receiveBuffer.get()), rightLen = lssEnd - m_receiveBuffer.get();
									outLen = leftLen + rightLen;
									if (m_outRangeSize < outLen)
									{
										m_outRangeBuffer.reset(new char[outLen]);
										m_outRangeSize = outLen;
									}
									std::copy(const_cast<char*>(lssbegin), const_cast<char*>(iterEnd), m_outRangeBuffer.get());
									std::copy(m_receiveBuffer.get(), const_cast<char*>(lssEnd), m_outRangeBuffer.get() + leftLen);
									*m_data++ = m_outRangeBuffer.get();
									*m_data++ = m_outRangeBuffer.get() + outLen;
								}
								catch (const std::exception &e)
								{
									m_outRangeSize = 0;
									*m_data++ = nullptr;
									*m_data++ = nullptr;
								}
							}
							else
							{
								*m_data++ = lssbegin;
								*m_data++ = lssEnd;
							}


							/*
							*m_data++ = lssbegin;
							if ((lssbegin - m_receiveBuffer.get()) > m_receivePos)
							{
								*m_data++ = m_receiveBuffer.get() + m_memorySize;
								*m_data++ = m_receiveBuffer.get();
							}
							*m_data++ = lssEnd;
							*/

							(std::get<3>(*m_redisRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
							if (!jump)
							{
								iterBegin = newLineEnd;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
							}
							else
							{
								newIterBegin = newLineEnd;
								newIterEnd = m_receiveBuffer.get() + m_receivePos;
								m_lastReceivePos = newIterBegin - m_receiveBuffer.get();
							}
							m_redisRequest.reset();
							success = false;
							break;
							case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
								redisType = REDISPARSETYPE::SIMPLE_STRING;
								break;

								case static_cast<int>(REDISPARSETYPE::ERROR) :
									redisType = REDISPARSETYPE::ERROR;
									break;

									case static_cast<int>(REDISPARSETYPE::INTERGER) :
										redisType = REDISPARSETYPE::INTERGER;
										break;

										case static_cast<int>(REDISPARSETYPE::ARRAY) :
											redisType = REDISPARSETYPE::ARRAY;
											break;

										default:
											redisType = REDISPARSETYPE::UNKNOWN_TYPE;
											break;
					}
				}


				if (jump && success)
				{
					iterBegin = newIterBegin, iterEnd = newIterEnd;
					while (iterBegin != iterEnd && success)
					{
						switch (*iterBegin)
						{
							case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
								redisType = REDISPARSETYPE::LOT_SIZE_STRING;
								if ((lengthEnd = std::search(iterBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
								{
									success = false;
									break;
								}
								if (*(iterBegin + 1) == '-')
								{
									m_tempRequest = *(m_waitMessageList.listSelf().begin());
									(std::get<3>(*m_tempRequest))(false, ERRORMESSAGE::NO_KEY);
									iterBegin = lengthEnd + STATICSTRING::smallNewlineLen;
									m_lastReceivePos = iterBegin - m_receiveBuffer.get();
									m_waitMessageList.unsafePop();
									if (m_waitMessageList.listSelf().empty())
									{
										success = false;
										break;
									}
									continue;
								}
								index = -1, num = 1;
								thisLength = std::accumulate(std::make_reverse_iterator(lengthEnd), std::make_reverse_iterator(iterBegin + 1), 0, [&index, &num](auto &sum, auto const ch)
								{
									if (++index)num *= 10;
									return sum += (ch - '0')*num;
								});
								lssbegin = lengthEnd + STATICSTRING::smallNewlineLen;
								if (distance(lssbegin, iterEnd) < (thisLength + STATICSTRING::smallNewlineLen))
								{
									success = false;
									break;
								}
								lssEnd = lssbegin + thisLength;
								iterBegin = lssEnd + STATICSTRING::smallNewlineLen;
								m_lastReceivePos = iterBegin - m_receiveBuffer.get();
								//处理结果,启动回调函数
								m_data = std::get<2>(*m_redisRequest);
								*m_data++ = lssbegin;
								*m_data++ = lssEnd;
								(std::get<3>(*m_redisRequest))(true, ERRORMESSAGE::LOT_SIZE_STRING);
								m_redisRequest.reset();
								success = false;
								break;
								case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
									redisType = REDISPARSETYPE::SIMPLE_STRING;
									break;

									case static_cast<int>(REDISPARSETYPE::ERROR) :
										redisType = REDISPARSETYPE::ERROR;
										break;

										case static_cast<int>(REDISPARSETYPE::INTERGER) :
											redisType = REDISPARSETYPE::INTERGER;
											break;

											case static_cast<int>(REDISPARSETYPE::ARRAY) :
												redisType = REDISPARSETYPE::ARRAY;
												break;

											default:
												redisType = REDISPARSETYPE::UNKNOWN_TYPE;
												break;
						}
					}
				}

				return !m_redisRequest;
			}
		}
	}
	return false;
}






void ASYNCREDISREAD::handleMessageResponse()
{
	m_messageResponse.getMutex().lock();
	if (!m_messageResponse.listSelf().empty())
	{
		std::forward_list<std::shared_ptr<SINGLEMESSAGE>>::const_iterator iter{ m_messageResponse.listSelf().cbegin() };
		do
		{
			(std::get<3>(*(*iter)->m_tempRequest))((*iter)->m_tempVaild, (*iter)->m_tempEM);
		} while (++iter != m_messageResponse.listSelf().cend());
		m_messageResponse.unsafeClear();
	}
	m_messageResponse.getMutex().unlock();
}









void ASYNCREDISREAD::queryLoop()
{
	m_writeStatus.store(false);   //第一次发送请求时这个可以注释掉，因为没有准备工作进行
	boost::asio::async_write(*m_sock, boost::asio::buffer(m_ptr.get(), m_totalLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		if (err)  //log
		{
			m_connect.store(false);
			m_writeStatus.store(true);   //第一次发送请求时这个可以注释掉，因为没有准备工作进行
			resetData();
		}
		else
		{
			m_writeStatus.store(true);    //第一次发送请求时这个可以注释掉，因为没有准备工作进行

			if (m_readyParse.load())
			{
				startReceive();
			}
		}
	});
	startReadyParse();
}



//检查待请求队列是否为空，不为则尝试准备请求，准备全部失败则将处理结果返回
void ASYNCREDISREAD::checkLoop()
{
	if (m_messageEmpty.load())
	{
		m_queryStatus.store(false);

	}
	else  //循环读取处理
	{
		//发起请求，如果待请求队列有数据，尝试准备请求，如果成功，则进行请求，接收处理，如果失败则进行清除
		if (tryCheckList())
		{
			m_redisRequest = m_nextRedisRequest;

			m_nextRedisRequest.reset();

			startQuery();
		}
		else
		{
			handleMessageResponse();

			checkLoop();
		}
	}
}



void ASYNCREDISREAD::simpleReceive()
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
		//无论下面成功与否，已经成功发送请求的消息应该处理到接收好消息为止，除非是短线意外情况
		//如果断线了，将有结果的请求调用回调函数，没有结果的或者是待请求队列的发送错误信息，调用回调函数
		if (err)
		{
			m_connect.store(false);
			resetData();
		}
		else
		{
			m_receivePos += size;
		
			if (startParse())
			{
				////////////////                         封装成一个函数
				checkLoop();
				/////////////////////////////////
			}
			else
			{
				//开启循环读取处理，循环读取，并在异步读取等待期间处理结果队列,读取成功    
				//std::cout << "no\n";
				simpleReceive();
			}
		}
	});
}

void ASYNCREDISREAD::simpleReceiveWaitMessage()
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
		//无论下面成功与否，已经成功发送请求的消息应该处理到接收好消息为止，除非是短线意外情况
		//如果断线了，将有结果的请求调用回调函数，没有结果的或者是待请求队列的发送错误信息，调用回调函数
		if (err)
		{
			m_connect.store(false);
			resetData();
		}
		else
		{
			m_receivePos += size;

			if (StartParseWaitMessage())
			{
				if(StartParseThisMessage())
				{
					checkLoop();
				}
				else
				{
					simpleReceiveThisMessage();
				}
			}
			else
			{
				//开启循环读取处理，循环读取，并在异步读取等待期间处理结果队列,读取成功    
				//std::cout << "no\n";
				simpleReceiveWaitMessage();
			}
		}
	});
}



void ASYNCREDISREAD::simpleReceiveThisMessage()
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
		//无论下面成功与否，已经成功发送请求的消息应该处理到接收好消息为止，除非是短线意外情况
		//如果断线了，将有结果的请求调用回调函数，没有结果的或者是待请求队列的发送错误信息，调用回调函数
		if (err)
		{
			m_connect.store(false);
			resetData();
		}
		else
		{
			m_receivePos += size;

			if (StartParseThisMessage())
			{
				checkLoop();
			}
			else
			{
				simpleReceiveThisMessage();
			}
		}
	});
}




//每次插入新请求时，做以下处理：
	//1 判断请求内容是否合法，不合法直接驳回
	//2 判断是否连接成功，不成功直接返回错误（因为连接失败时要对队列进行处理）
	//3 连接成功的情况下，做以下处理：
	//（1） 判断当前请求状态是否为true，如果是，则说明在之前已经开始了请求处理，试图插入等待发送请求的队列，插入失败返回插入失败错误信息
	//（2） 当前请求状态为false，首先将请求状态置为true，独占请求处理，然后开始试图生成redis请求消息，如果不成功，则检查等待发送请求的队列是否存在元素，
	//      如果存在则试图生成redis请求消息，期间非法数据插入已经获取消息结果准备在异步等待期间处理的队列，如果全部为非法，则将请求状态置为false，加锁处理已经获取消息结果准备在异步等待期间处理的队列
	//（3） 如果生成成功，则进入startQuery函数处理，首次startQuery期间没有数据需要进行处理
bool ASYNCREDISREAD::insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char**, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest)
{
	if (!redisRequest || !std::get<0>(*redisRequest) || !std::get<1>(*redisRequest) || !std::get<2>(*redisRequest) || !std::get<3>(*redisRequest))
		return false;
	if (m_connect.load())
	{
		if (m_queryStatus.load())
		{
			if (m_messageList.insert(redisRequest))
			{
				m_messageEmpty.store(false);
				return true;
			}
			return false;
		}
		else
		{
			m_queryStatus.store(true);

			if (readyQuery(std::get<0>(*redisRequest), std::get<1>(*redisRequest)))
			{
				m_redisRequest = redisRequest;

				startQuery();
			}
			else
			{
				//redisRequest请求本身无法生成有效的请求信息，如果可以从队列中获取新请求，那么将该失败请求结果缓存起来，否则返回false
				if (tryCheckList())
				{
					try
					{
						if (!m_messageResponse.insert(std::make_shared<SINGLEMESSAGE>(redisRequest, false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR)))
							std::get<3>(*redisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
					}
					catch (const std::exception &e)
					{
						std::get<3>(*redisRequest)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
					}

					m_redisRequest = m_nextRedisRequest;

					m_nextRedisRequest.reset();

					startQuery();
				}
				else
				{
					m_queryStatus.store(false);

					return false;
				}
			}
		}
	}
	else
	{
		return false;
	}

	return true;
}