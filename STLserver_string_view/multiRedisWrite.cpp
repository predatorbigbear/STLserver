#include "multiRedisWrite.h"

MULTIREDISWRITE::MULTIREDISWRITE(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<ASYNCLOG> log,
	std::shared_ptr<std::function<void()>> unlockFun,
	std::shared_ptr<STLTimeWheel> timeWheel,
	const std::string& redisIP, const unsigned int redisPort,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
	:m_redisIP(redisIP), m_redisPort(redisPort), m_ioc(ioc), m_unlockFun(unlockFun), m_receiveBufferMaxSize(memorySize),
	m_outRangeMaxSize(outRangeMaxSize),
	m_commandMaxSize(commandSize), m_log(log), m_timeWheel(timeWheel), m_messageList(commandSize * 4)

{
	if (!ioc)
		throw std::runtime_error("io_context is nullptr");
	if (!log)
		throw std::runtime_error("log is nullptr");
	if (!timeWheel)
		throw std::runtime_error("timeWheel is nullptr");
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
	if (!memorySize)
		throw std::runtime_error("memorySize is invaild");
	if (!outRangeMaxSize)
		throw std::runtime_error("outRangeMaxSize is invaild");
	if (!commandSize)
		throw std::runtime_error("commandSize is invaild");

	m_receiveBuffer.reset(new char[memorySize]);


	m_outRangeBuffer.reset(new char[m_outRangeMaxSize]);

	

	m_waitMessageList.reset(new std::shared_ptr<redisMessageListTypeSW>[m_commandMaxSize]);
	m_waitMessageListMaxSize = m_commandMaxSize;

	firstConnect();
}


/*
	执行命令string_view集
	执行命令个数
	每条命令的词语个数（方便根据redis RESP进行拼接）
	获取结果次数  （因为比如一些事务操作可能不一定有结果返回）

	返回结果string_view
	每个结果的词语个数

	回调函数
	*/

	//插入请求，首先判断是否连接redis服务器成功，
	//如果没有连接，插入直接返回错误
	//连接成功的情况下，检查请求是否符合要求

	//先进行插入到m_waitMessageList中
	//如果没有正在进行的请求则直接发起请求，否则插入到待处理队列中，稍后进行流水线命令封装发射


	//   using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
	//     std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
	//     std::function<void(bool, enum ERRORMESSAGE) >> ;

bool MULTIREDISWRITE::insertRedisRequest(std::shared_ptr<redisResultTypeSW>& redisRequest)
{
	if (!redisRequest)
		return false;

	redisResultTypeSW& thisRequest{ *redisRequest };
	if (std::get<0>(thisRequest).get().empty() || !std::get<1>(thisRequest) || std::get<1>(thisRequest) > m_commandMaxSize
		|| !std::get<3>(thisRequest) || std::get<1>(thisRequest) != std::get<2>(thisRequest).get().size() ||
		std::accumulate(std::get<2>(thisRequest).get().cbegin(), std::get<2>(thisRequest).get().cend(), 0) != std::get<0>(thisRequest).get().size()
		)
		return false;

	if (!m_connect.load(std::memory_order_acquire))
	{
		return false;
	}
	if (!m_queryStatus.load(std::memory_order_acquire))
	{
		m_queryStatus.store(true, std::memory_order_release);


		//////////////////////////////////////////////////////////////////////

		std::vector<std::string_view>& sourceVec = std::get<0>(thisRequest).get();
		std::vector<unsigned int>& lenVec = std::get<2>(thisRequest).get();
		unsigned int thisCommandSize{ std::get<1>(thisRequest) };
		std::vector<std::string_view>::const_iterator souceBegin{ sourceVec.cbegin() }, souceEnd{ sourceVec.cend() };
		std::vector<unsigned int>::const_iterator lenBegin{ lenVec.cbegin() }, lenEnd{ lenVec.cend() };
		char* messageIter{};

		//计算本次所需要的空间大小
		static const int divisor{ 10 };
		int totalLen{}, everyLen{}, index{}, temp{}, thisStrLen{};
		do
		{
			everyLen = *lenBegin;
			if (everyLen)
			{
				totalLen += 1;

				temp = everyLen, thisStrLen = 1;
				while (temp /= divisor)
					++thisStrLen;
				totalLen += thisStrLen + 2;

				index = -1;
				while (++index != everyLen)
				{
					totalLen += 1;
					temp = souceBegin->size(), thisStrLen = 1;
					while (temp /= divisor)
						++thisStrLen;
					totalLen += thisStrLen + 2 + souceBegin->size() + 2;
					++souceBegin;
				}
			}
		} while (++lenBegin != lenEnd);

		if (totalLen > m_messageBufferMaxSize)
		{
			try
			{
				m_messageBuffer.reset(new char[totalLen]);
				m_messageBufferMaxSize = totalLen;
			}
			catch (const std::exception& e)
			{
				m_messageBufferMaxSize = 0;
				m_queryStatus.store(false, std::memory_order_release);

				return false;
			}
		}

		m_messageBufferNowSize = totalLen;


		///////////////////////////////////////////

		if (m_logMessageSize < thisCommandSize)
		{
			try
			{
				m_logMessage.reset(new std::string_view[thisCommandSize]);
				m_logMessageSize = thisCommandSize;
			}
			catch (const std::exception& e)
			{
				m_logMessageSize = 0;
				m_queryStatus.store(false, std::memory_order_release);

				return false;
			}
		}

		m_logMessageIter = m_logMessage.get();



		//////////////////////////////////////////////


		//copy生成到发送字符串buffer中

		souceBegin = sourceVec.cbegin(), lenBegin = lenVec.cbegin();

		messageIter = m_messageBuffer.get();

		//每条消息首尾位置
		const char* lMBegin{}, * LMEnd{};

		do
		{
			lMBegin = messageIter;

			everyLen = *lenBegin;
			if (everyLen)
			{
				*messageIter++ = '*';

				if (everyLen > 99999999)
					*messageIter++ = everyLen / 100000000 + '0';
				if (everyLen > 9999999)
					*messageIter++ = everyLen / 10000000 % 10 + '0';
				if (everyLen > 999999)
					*messageIter++ = everyLen / 1000000 % 10 + '0';
				if (everyLen > 99999)
					*messageIter++ = everyLen / 100000 % 10 + '0';
				if (everyLen > 9999)
					*messageIter++ = everyLen / 10000 % 10 + '0';
				if (everyLen > 999)
					*messageIter++ = everyLen / 1000 % 10 + '0';
				if (everyLen > 99)
					*messageIter++ = everyLen / 100 % 10 + '0';
				if (everyLen > 9)
					*messageIter++ = everyLen / 10 % 10 + '0';
				if (everyLen > 0)
					*messageIter++ = everyLen % 10 + '0';

				std::copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
				messageIter += HTTPRESPONSE::halfNewLineLen;

				index = -1;
				while (++index != everyLen)
				{
					*messageIter++ = '$';

					temp = souceBegin->size();

					if (temp > 99999999)
						*messageIter++ = temp / 100000000 + '0';
					if (temp > 9999999)
						*messageIter++ = temp / 10000000 % 10 + '0';
					if (temp > 999999)
						*messageIter++ = temp / 1000000 % 10 + '0';
					if (temp > 99999)
						*messageIter++ = temp / 100000 % 10 + '0';
					if (temp > 9999)
						*messageIter++ = temp / 10000 % 10 + '0';
					if (temp > 999)
						*messageIter++ = temp / 1000 % 10 + '0';
					if (temp > 99)
						*messageIter++ = temp / 100 % 10 + '0';
					if (temp > 9)
						*messageIter++ = temp / 10 % 10 + '0';
					if (temp > 0)
						*messageIter++ = temp % 10 + '0';

					std::copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
					messageIter += HTTPRESPONSE::halfNewLineLen;

					std::copy(souceBegin->cbegin(), souceBegin->cend(), messageIter);
					messageIter += souceBegin->size();

					std::copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
					messageIter += HTTPRESPONSE::halfNewLineLen;

					++souceBegin;
				}
			}

			LMEnd = messageIter;
			*m_logMessageIter++ = std::string_view(lMBegin, std::distance(lMBegin, LMEnd));

		} while (++lenBegin != lenEnd);


		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();


		try
		{
			*m_waitMessageListBegin++ = std::make_shared<redisMessageListTypeSW>(
				std::vector<std::string>{ std::get<0>(thisRequest).get().cbegin(), std::get<0>(thisRequest).get().cend()},
				std::get<1>(thisRequest),
				std::get<2>(thisRequest).get(),
				std::get<3>(thisRequest),
				std::ref(tempVec1), std::ref(tempVec2),
				std::bind(&MULTIREDISWRITE::logCallBack, this, std::placeholders::_1, std::placeholders::_2), nullptr);
		}
		catch (const std::exception& e)
		{
			return false;
		}

		m_waitMessageListEnd = m_waitMessageListBegin;

		m_waitMessageListBegin = m_waitMessageList.get();

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(thisRequest);

		m_commandCurrentSize = 0;

		m_logMessageIter = m_logMessage.get();
		//////////////////////////////////////////////////////
		//进入发送函数

		query();

		return true;
	}
	else
	{

		try
		{
			if (!m_messageList.try_enqueue(std::make_shared<redisMessageListTypeSW>(
				std::vector<std::string>{ std::get<0>(thisRequest).get().cbegin(), std::get<0>(thisRequest).get().cend()},
				std::get<1>(thisRequest),
				std::get<2>(thisRequest).get(),
				std::get<3>(thisRequest),
				std::ref(tempVec1), std::ref(tempVec2), nullptr, nullptr)))
				return false;
		}
		catch (const std::exception& e)
		{
			return false;
		}

		return true;
	}
}




bool MULTIREDISWRITE::readyEndPoint()
{
	try
	{
		m_endPoint.reset(new boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(m_redisIP), m_redisPort));
		return true;
	}
	catch (const std::exception& e)
	{
		return false;
	}
	catch (const boost::system::system_error& err)
	{
		return false;
	}
}



bool MULTIREDISWRITE::readySocket()
{
	try
	{
		m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));
		return true;
	}
	catch (const std::exception& e)
	{
		return false;
	}
	catch (const boost::system::system_error& err)
	{
		return false;
	}
}



void MULTIREDISWRITE::firstConnect()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code& err)
	{
		if (err)
		{
			m_connectStatus = 1;

			ec = {};
			m_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

			static std::function<void()>socketTimeOut{ [this]() {shutdownLoop(); } };
			m_timeWheel->insert(socketTimeOut, 5);
		}
		else
		{
			std::cout << "connect multi RedisWrite success\n";
			setConnectSuccess();
			(*m_unlockFun)();
		}
	});
}



void MULTIREDISWRITE::reconnect()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code& err)
	{
		if (err)
		{
			m_connectStatus = 2;

			ec = {};
			m_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

			static std::function<void()>socketTimeOut{ [this]() {shutdownLoop(); } };
			m_timeWheel->insert(socketTimeOut, 5);
		}
		else
		{
			query();
		}
	});
}




void MULTIREDISWRITE::setConnectSuccess()
{
	m_connect.store(true, std::memory_order_release);
}




void MULTIREDISWRITE::setConnectFail()
{
	m_connect.store(false, std::memory_order_release);
}




void MULTIREDISWRITE::query()
{

	boost::asio::async_write(*m_sock, boost::asio::buffer(m_sendMessage, m_sendLen), [this](const boost::system::error_code& err, const std::size_t size)
	{
		if (err)  //log
		{
			m_log->writeLog(__FUNCTION__, __LINE__, err.what());
			m_connectStatus = 2;

			ec = {};
			m_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

			static std::function<void()>socketTimeOut{ [this]() {shutdownLoop(); } };
			m_timeWheel->insert(socketTimeOut, 5);
		}
		else
		{
			receive();
		}
	});
}



//接收成功的话进入处理函数
void MULTIREDISWRITE::receive()
{
	if (m_receiveBufferNowSize == m_receiveBufferMaxSize)
	{
		m_receiveBufferNowSize = 0;
		m_thisReceivePos = 0;
		m_thisReceiveLen = m_receiveBufferMaxSize;
	}
	else
	{
		m_thisReceivePos = m_receiveBufferNowSize;
		m_thisReceiveLen = m_receiveBufferMaxSize - m_receiveBufferNowSize;
	}
	m_sock->async_read_some(boost::asio::buffer(m_receiveBuffer.get() + m_thisReceivePos, m_thisReceiveLen), [this](const boost::system::error_code& err, const std::size_t size)
	{

		handlelRead(err, size);
	});
}




void MULTIREDISWRITE::resetReceiveBuffer()
{
	m_receiveBufferNowSize = m_lastPrasePos = 0;
}



//最后返回结果处理的时候，判断当前节点请求总数，如果是单个请求的话，那么可以细致区分返回具体信息
//反之应该返回一个联合查询成功标志

//首先判断m_jumpNode状态，如果处于m_jumpNode状态，那么获取需要跳过的结果数，开始累积跳过
//如果m_jumpNode状态为正常的话，那么获取本节点的命令数，已收集命令数，命令数为节点第二位，收集到满足次数之后返回
//对于数组，首先存储到临时buffer中，获取完毕后，一次性存储到节点vector结果集中，
//结束之后判断是否走到end节点，是的话获取完毕，返回true

//解析函数
//PS：所有return的地方调用之前需要将临时变量赋值给相关的类成员变量
// 临时变量操作速度快，当需要大量使用是在一开始设置临时变量，用类成员变量初始化，在最后退出时将值保存回去
bool MULTIREDISWRITE::parse(const int size)
{
	const char* iterBegin{ }, * iterEnd{ m_receiveBuffer.get() + m_receiveBufferNowSize }, * iterHalfBegin{ m_receiveBuffer.get() },
		* iterHalfEnd{ m_receiveBuffer.get() + m_receiveBufferMaxSize }, * iterFind{},        // 开始 结束  寻找节点
		* iterLenBegin{}, * iterLenEnd{},               //长度节点
		* iterStrBegin{}, * iterStrEnd{},               //本次SIMPLE_STRING首尾位置
		* errorBegin{}, * errorEnd{},
		* arrayNumBegin{}, * arrayNumEnd{};

	// m_lastPrasePos    m_jumpNode  m_waitMessageListBegin   m_commandTotalSize    m_commandCurrentSize返回时需要同步处理

	bool jumpNode{ m_jumpNode }, jump{ false };  //是否已经经过回环情况
	std::shared_ptr<redisMessageListTypeSW>* waitMessageListBegin = m_waitMessageListBegin;
	std::shared_ptr<redisMessageListTypeSW>* waitMessageListEnd = m_waitMessageListEnd;
	unsigned int commandTotalSize{ m_commandTotalSize }, commandCurrentSize{ m_commandCurrentSize };

	// debug  ////////////

	/////////////////////////////


	unsigned int len{};     //计算字符串长度使用
	int index{}, num{}, arrayNum{}, arrayIndex{};


	if (m_lastPrasePos == m_receiveBufferMaxSize)
		iterBegin = m_receiveBuffer.get();
	else
		iterBegin = m_receiveBuffer.get() + m_lastPrasePos;

	const std::string_view emptyView(nullptr, 0);

	//接收数据没有回环情况
	if (iterBegin < iterEnd)
	{
		////////////////////////////////

		while (iterBegin != iterEnd)
		{
			iterFind = iterBegin;
			redisMessageListTypeSW& thisRequest{ **waitMessageListBegin };
			switch (*iterFind)
			{
			case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING):
				//  REDISPARSETYPE::LOT_SIZE_STRING
				if (++iterFind == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				if (*iterFind == '-')
				{
					if (iterEnd - iterFind >= 4)
					{
						iterFind += 4;
						iterBegin = iterFind;
						////////////////////////////////////////////////////////////////////////////////////////////////////////////
						//本次结果返回无  -1，看情况处理

						if (!jumpNode)
						{
							try
							{
								std::get<4>(thisRequest).get().emplace_back(emptyView);
								std::get<5>(thisRequest).get().emplace_back(1);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}


						//只负责返回成功与否
						if (++commandCurrentSize == commandTotalSize)
						{
							//根据jumpNode状态 调用回调函数返回本次成功与失败结果
							if (jumpNode)
							{



							}
							else
							{
								if (commandTotalSize == 1)
									std::get<6>(thisRequest)(true, ERRORMESSAGE::NO_KEY);
								else
									std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_MULTI_OK);
							}

							jumpNode = false;
							if (++waitMessageListBegin != waitMessageListEnd)
							{
								commandTotalSize = std::get<1>(**waitMessageListBegin);
								commandCurrentSize = 0;
							}
						}
						//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

						continue;
					}

					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}
				else
				{
					iterLenBegin = iterFind;

					if ((iterLenEnd = std::find(iterLenBegin + 1, iterEnd, '\r')) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
					else
					{
						index = -1;
						num = 1;
						len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
						{
							if (++index)
								num *= 10;
							return sum += (ch - '0') * num;
						});
					}

					if (iterEnd - iterLenEnd < (4 + len))
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
					else
					{
						iterStrBegin = iterLenEnd + 2;
						iterStrEnd = iterStrBegin + len;

						//本次结果获取成功，看情况进行处理

						//////////////////////////////////////////////////////////////////////////////////////////////////////////////
						if (!jumpNode)
						{
							try
							{
								std::get<4>(thisRequest).get().emplace_back(std::string_view(iterStrBegin, len));
								std::get<5>(thisRequest).get().emplace_back(1);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}


						if (++commandCurrentSize == commandTotalSize)
						{
							//根据jumpNode状态 调用回调函数返回本次成功与失败结果
							if (jumpNode)
							{



							}
							else
							{
								if (commandTotalSize == 1)
									std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_SINGLE_OK);
								else
									std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_MULTI_OK);
							}

							jumpNode = false;
							if (++waitMessageListBegin != waitMessageListEnd)
							{
								commandTotalSize = std::get<1>(**waitMessageListBegin);
								commandCurrentSize = 0;
							}
						}


						///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
						iterBegin = iterStrEnd + 2;
						continue;
					}
				}

				break;

			case static_cast<int>(REDISPARSETYPE::INTERGER):
				// REDISPARSETYPE::INTERGER

				if ((iterLenEnd = std::find(iterFind + 1, iterEnd, '\r')) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				if (iterEnd - iterLenEnd < 2)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterLenBegin = iterFind + 1;

				iterFind = iterLenEnd + 2;
				iterBegin = iterFind;

				//结果iterLenBegin    iterLenEnd
				////////////////////////////////////////////////////////////////////////
				if (!jumpNode)
				{
					try
					{
						std::get<4>(thisRequest).get().emplace_back(std::string_view(iterLenBegin, iterLenEnd - iterLenBegin));
						std::get<5>(thisRequest).get().emplace_back(1);
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				///////////////////////////////////////////////////////////////////////////////////////////////////
				break;

			case static_cast<int>(REDISPARSETYPE::ERROR):
				//REDISPARSETYPE::ERROR

				errorBegin = iterBegin + 1;
				if ((errorEnd = std::search(errorBegin + 1, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterFind = errorEnd + 2;
				iterBegin = iterFind;
				//返回错误信息结果
				////////////////////////////////////////////////////////////////////////////////////////////

				if (!jumpNode)
				{
					try
					{
						
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}

				m_log->writeLog("redis error", *m_logMessageIter++, std::string_view(errorBegin, errorEnd - errorBegin));
				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				//////////////////////////////////////////////////////////////////////////////////////
				break;

			case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING):
				//REDISPARSETYPE::SIMPLE_STRING

				iterStrBegin = iterBegin + 1;
				if ((iterStrEnd = std::search(iterStrBegin + 1, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterFind = iterStrEnd + 2;
				iterBegin = iterFind;
				//返回错误信息结果
				//////////////////////////////////////////////////////////////////////////////////////////////////////////

				if (!jumpNode)
				{
					try
					{
						
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}

				++m_logMessageIter;
				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				//////////////////////////////////////////////////////////////////////////////////////////////
				break;


			case static_cast<int>(REDISPARSETYPE::ARRAY):
				//REDISPARSETYPE::ARRAY

				if (!jumpNode)
					m_arrayResult.clear();

				if (++iterFind == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				arrayNumBegin = iterFind;

				if ((arrayNumEnd = std::find(arrayNumBegin + 1, iterEnd, '\r')) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				index = -1;
				num = 1;
				arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto& sum, auto const ch)
				{
					if (++index)
						num *= 10;
					return sum += (ch - '0') * num;
				});

				if (iterEnd - arrayNumEnd < 2)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterFind = arrayNumEnd + 2;

				arrayIndex = -1;
				while (++arrayIndex != arrayNum)
				{
					if (++iterFind == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					if (*iterFind == '-')
					{
						if (iterEnd - iterFind < 4)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}

						iterFind += 4;
						///////////////////////   本次没有结果，考虑怎么返回比较方便  -1////////////////////

						if (!jumpNode)
						{
							try
							{
								m_arrayResult.emplace_back(emptyView);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}

						//////////////////////////////////////////////////////////////////////////////////////
						continue;
					}

					if (iterFind + 1 == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterLenBegin = iterFind;

					if ((iterLenEnd = std::find(iterLenBegin + 1, iterEnd, '\r')) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					index = -1;
					num = 1;
					len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
					{
						if (++index)
							num *= 10;
						return sum += (ch - '0') * num;
					});

					if (iterEnd - iterLenEnd < 4 + len)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterStrBegin = iterLenEnd + 2;
					iterStrEnd = iterStrBegin + len;

					iterFind = iterStrEnd + 2;

					//////////////////////////////////////////////  每次的有效结果

					if (!jumpNode)
					{
						try
						{
							m_arrayResult.emplace_back(std::string_view(iterStrBegin, len));
						}
						catch (const std::exception& e)
						{
							jumpNode = true;
						}
					}



					//////////////////////////////////////////////////////////////
				}
				iterBegin = iterFind;

				///////////////////////////////////////////////////////////////////////// 这里进行参数传递


				if (!jumpNode)
				{
					std::vector<std::string_view>& vec = std::get<4>(thisRequest).get();
					try
					{
						vec.swap(m_arrayResult);
						std::get<5>(thisRequest).get().emplace_back(vec.size());
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}



				///////////////////////////////////////////////////////////////////////////////////////
				break;
			default:

				break;
			}
		}
	}
	else      //接收数据回环情况
	{


		while (!jump)
		{
			iterFind = iterBegin;
			redisMessageListTypeSW& thisRequest{ **waitMessageListBegin };
			switch (*iterFind)
			{
			case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING):
				//  REDISPARSETYPE::LOT_SIZE_STRING
				if (++iterFind == iterHalfEnd)
				{
					if (iterHalfBegin == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
					else
					{
						iterFind = iterHalfBegin;
						jump = true;
					}
				}



				if (*iterFind == '-')
				{
					if (!jump)
					{
						if ((iterHalfEnd - iterFind) + (iterEnd - iterHalfBegin) >= 4)
						{
							if (iterHalfEnd - iterFind >= 4)
							{
								iterFind += 4;
								iterBegin = iterFind;
							}
							else
							{
								iterFind = iterHalfBegin + (4 - (iterHalfEnd - iterFind));
								iterBegin = iterFind;
								jump = true;
							}
							//本次结果返回无  -1，看情况处理

							///////////////////////////////////////////////////////


							if (!jumpNode)
							{
								try
								{
									std::get<4>(thisRequest).get().emplace_back(emptyView);
									std::get<5>(thisRequest).get().emplace_back(1);
								}
								catch (const std::exception& e)
								{
									jumpNode = true;
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//根据jumpNode状态 调用回调函数返回本次成功与失败结果
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(thisRequest)(true, ERRORMESSAGE::NO_KEY);
									else
										std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_MULTI_OK);
								}

								jumpNode = false;
								if (++waitMessageListBegin != waitMessageListEnd)
								{
									commandTotalSize = std::get<1>(**waitMessageListBegin);
									commandCurrentSize = 0;
								}
							}



							////////////////////////////////////////////////////////
							continue;
						}

						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
					else
					{
						if (iterEnd - iterFind >= 4)
						{
							iterFind += 4;
							iterBegin = iterFind;
							//本次结果返回无  -1，看情况处理
							//////////////////////////////////////////

							if (!jumpNode)
							{
								try
								{
									std::get<4>(thisRequest).get().emplace_back(emptyView);
									std::get<5>(thisRequest).get().emplace_back(1);
								}
								catch (const std::exception& e)
								{
									jumpNode = true;
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//根据jumpNode状态 调用回调函数返回本次成功与失败结果
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(thisRequest)(true, ERRORMESSAGE::NO_KEY);
									else
										std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_MULTI_OK);
								}

								jumpNode = false;
								if (++waitMessageListBegin != waitMessageListEnd)
								{
									commandTotalSize = std::get<1>(**waitMessageListBegin);
									commandCurrentSize = 0;
								}
							}


							///////////////////////////////////////////////////
							continue;
						}

						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}
				else
				{
					iterLenBegin = iterFind;

					if (!jump)
					{
						if ((iterLenEnd = std::find(iterLenBegin + 1, iterHalfEnd, '\r')) == iterHalfEnd)
						{
							if ((iterLenEnd = std::find(iterHalfBegin, iterEnd, '\r')) == iterEnd)
							{
								m_lastPrasePos = iterBegin - m_receiveBuffer.get();
								m_jumpNode = jumpNode;
								m_waitMessageListBegin = waitMessageListBegin;
								m_commandTotalSize = commandTotalSize;
								m_commandCurrentSize = commandCurrentSize;
								return false;
							}
							else
							{
								index = -1;
								num = 1;
								len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterHalfBegin), 0, [&index, &num](auto& sum, auto const ch)
								{
									if (++index)
										num *= 10;
									return sum += (ch - '0') * num;
								});
								len += std::accumulate(std::make_reverse_iterator(iterHalfEnd), std::make_reverse_iterator(iterLenBegin), len, [&index, &num](auto& sum, auto const ch)
								{
									if (++index)
										num *= 10;
									return sum += (ch - '0') * num;
								});
								jump = true;
							}
						}
						else
						{
							index = -1;
							num = 1;
							len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
							{
								if (++index)
									num *= 10;
								return sum += (ch - '0') * num;
							});
						}
					}
					else
					{
						if ((iterLenEnd = std::find(iterLenBegin + 1, iterEnd, '\r')) == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
						else
						{
							index = -1;
							num = 1;
							len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
							{
								if (++index)
									num *= 10;
								return sum += (ch - '0') * num;
							});
						}
					}



					if (!jump)
					{
						if (iterHalfEnd - iterLenEnd + (iterEnd - iterHalfBegin) < (4 + len))
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
						else
						{
							if (iterHalfEnd - iterLenEnd >= 2)
							{
								iterStrBegin = iterLenEnd + 2;
							}
							else
							{
								iterStrBegin = iterHalfBegin + (2 - (iterHalfEnd - iterLenEnd));
								jump = true;
							}


							if (!jump)
							{
								if (iterHalfEnd - iterStrBegin >= len)
								{
									iterStrEnd = iterStrBegin + len;
								}
								else
								{
									iterStrEnd = iterHalfBegin + (len - (iterHalfEnd - iterStrBegin));
									jump = true;
								}
							}
							else
							{
								iterStrEnd = iterStrBegin + len;
							}

							//本次结果获取成功，看情况进行处理，注意判断回环情况    iterStrBegin      iterStrEnd

							/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



							if (!jumpNode)
							{
								//需要回环处理拷贝
								if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
								{
									//因为这一套机制本质建立在危险的指针之上，因此这里为了安全，当发生数据回环情况需要拷贝保存的时候，判断长度是否在m_outRangeMaxSize之内，如果超过，宁愿判断为没有。
								//如果使用者觉得可以承受后果，那么可以在这里换成动态分配内存进行拷贝，但是要记住一点：一旦控制不好，在使用这段动态分配内存时第二次接收信息来到时一旦进行重新分配，
								//可能会触发空指针异常 程序直接崩溃
									if (len <= m_outRangeMaxSize)
									{
										std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
										std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

										try
										{
											std::get<4>(thisRequest).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
											std::get<5>(thisRequest).get().emplace_back(1);
										}
										catch (const std::exception& e)
										{
											jumpNode = true;
										}
									}
									else
									{
										try
										{
											std::get<4>(thisRequest).get().emplace_back(emptyView);
											std::get<5>(thisRequest).get().emplace_back(1);
										}
										catch (const std::exception& e)
										{
											jumpNode = true;
										}
									}
								}
								else
								{
									try
									{
										std::get<4>(thisRequest).get().emplace_back(std::string_view(iterStrBegin, len));
										std::get<5>(thisRequest).get().emplace_back(1);
									}
									catch (const std::exception& e)
									{
										jumpNode = true;
									}
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//根据jumpNode状态 调用回调函数返回本次成功与失败结果
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_SINGLE_OK);
									else
										std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_MULTI_OK);
								}

								jumpNode = false;
								if (++waitMessageListBegin != waitMessageListEnd)
								{
									commandTotalSize = std::get<1>(**waitMessageListBegin);
									commandCurrentSize = 0;
								}
							}



							///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

							if (!jump)
							{
								if (iterHalfEnd - iterStrEnd >= 2)
								{
									iterBegin = iterStrEnd + 2;
								}
								else
								{
									iterBegin = iterHalfBegin + (2 - (iterHalfEnd - iterStrEnd));
									jump = true;
								}
							}
							else
							{
								iterBegin = iterStrEnd + 2;
							}
						}
					}
					else
					{
						if (iterEnd - iterLenEnd < (4 + len))
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
						else
						{
							iterStrBegin = iterLenEnd + 2;
							iterStrEnd = iterStrBegin + len;

							//本次结果获取成功，看情况进行处理，注意判断回环情况

							/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


							if (!jumpNode)
							{
								//需要回环处理拷贝
								if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
								{
									//因为这一套机制本质建立在危险的指针之上，因此这里为了安全，当发生数据回环情况需要拷贝保存的时候，判断长度是否在m_outRangeMaxSize之内，如果超过，宁愿判断为没有。
								//如果使用者觉得可以承受后果，那么可以在这里换成动态分配内存进行拷贝，但是要记住一点：一旦控制不好，在使用这段动态分配内存时第二次接收信息来到时一旦进行重新分配，
								//可能会触发空指针异常 程序直接崩溃
									if (len <= m_outRangeMaxSize)
									{
										std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
										std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

										try
										{
											std::get<4>(thisRequest).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
											std::get<5>(thisRequest).get().emplace_back(1);
										}
										catch (const std::exception& e)
										{
											jumpNode = true;
										}
									}
									else
									{
										try
										{
											std::get<4>(thisRequest).get().emplace_back(emptyView);
											std::get<5>(thisRequest).get().emplace_back(1);
										}
										catch (const std::exception& e)
										{
											jumpNode = true;
										}
									}
								}
								else
								{
									try
									{
										std::get<4>(thisRequest).get().emplace_back(std::string_view(iterStrBegin, len));
										std::get<5>(thisRequest).get().emplace_back(1);
									}
									catch (const std::exception& e)
									{
										jumpNode = true;
									}
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//根据jumpNode状态 调用回调函数返回本次成功与失败结果
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_SINGLE_OK);
									else
										std::get<6>(thisRequest)(true, ERRORMESSAGE::REDIS_MULTI_OK);
								}

								jumpNode = false;
								if (++waitMessageListBegin != waitMessageListEnd)
								{
									commandTotalSize = std::get<1>(**waitMessageListBegin);
									commandCurrentSize = 0;
								}
							}



							////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

							iterBegin = iterStrEnd + 2;
						}
					}

					continue;
				}
				break;


			case static_cast<int>(REDISPARSETYPE::INTERGER):
				// REDISPARSETYPE::INTERGER
				iterLenBegin = iterFind + 1;

				if (iterHalfEnd == iterLenBegin)
				{
					iterLenBegin = iterHalfBegin;
					jump = true;
				}


				//////////////////////////////////////////////////////////////

				if (!jump)
				{
					if ((iterLenEnd = std::find(iterFind + 1, iterHalfEnd, '\r')) == iterHalfEnd)
					{
						if ((iterLenEnd = std::find(iterHalfBegin, iterEnd, '\r')) == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
						else
						{
							jump = true;
						}
					}
				}
				else
				{
					if ((iterLenEnd = std::find(iterFind + 1, iterEnd, '\r')) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}


				///////////////////////////////////////////////////////////////////////

				if (!jump)
				{
					if ((iterHalfEnd - iterLenEnd) + (iterEnd - iterHalfBegin) < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					if (iterHalfEnd - iterLenEnd >= 2)
					{
						iterFind = iterLenEnd + 2;
					}
					else
					{
						iterFind = iterHalfBegin + (2 - (iterHalfEnd - iterLenEnd));
					}
				}
				else
				{
					if (iterEnd - iterLenEnd < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterFind = iterLenEnd + 2;
				}
				iterBegin = iterFind;


				//结果iterLenBegin    iterLenEnd    注意回环情况处理

				/////////////////////////////////////////////////////////////////////////////////////////


				if (!jumpNode)
				{
					//需要回环处理拷贝
					if (iterLenBegin < iterHalfEnd && iterLenEnd>iterHalfBegin)
					{
						//因为这一套机制本质建立在危险的指针之上，因此这里为了安全，当发生数据回环情况需要拷贝保存的时候，判断长度是否在m_outRangeMaxSize之内，如果超过，宁愿判断为没有。
					//如果使用者觉得可以承受后果，那么可以在这里换成动态分配内存进行拷贝，但是要记住一点：一旦控制不好，在使用这段动态分配内存时第二次接收信息来到时一旦进行重新分配，
					//可能会触发空指针异常 程序直接崩溃
						if (len <= m_outRangeMaxSize)
						{
							std::copy(iterLenBegin, iterHalfEnd, m_outRangeBuffer.get());
							std::copy(iterHalfBegin, iterLenEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterLenBegin));

							try
							{
								std::get<4>(thisRequest).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
								std::get<5>(thisRequest).get().emplace_back(1);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
						else
						{
							try
							{
								std::get<4>(thisRequest).get().emplace_back(emptyView);
								std::get<5>(thisRequest).get().emplace_back(1);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
					}
					else
					{
						try
						{
							std::get<4>(thisRequest).get().emplace_back(std::string_view(iterLenBegin, len));
							std::get<5>(thisRequest).get().emplace_back(1);
						}
						catch (const std::exception& e)
						{
							jumpNode = true;
						}
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				break;


			case static_cast<int>(REDISPARSETYPE::ERROR):
				errorBegin = iterBegin + 1;
				if (errorBegin == iterHalfEnd)
				{
					errorBegin = iterHalfBegin;
					jump = true;
				}


				if (!jump)
				{
					if ((errorEnd = std::search(errorBegin + 1, iterHalfEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterHalfEnd)
					{
						if (*(iterHalfEnd - 1) == '\r' && *iterHalfBegin == '\n')
						{
							errorEnd = iterHalfEnd - 1;
							jump = true;
						}
						else if ((errorEnd = std::search(iterHalfBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}
				}
				else
				{
					if ((errorEnd = std::search(iterHalfBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}


				if (!jump)
				{
					if ((iterHalfEnd - errorEnd) + (iterEnd - iterHalfBegin) < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					if (iterHalfEnd - errorEnd >= 2)
						iterFind = errorEnd + 2;
					else
						iterFind = iterHalfBegin + (2 - (iterHalfEnd - errorEnd));
				}
				else
				{
					if (iterEnd - iterHalfBegin < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterFind = iterEnd + 2;
				}
				iterBegin = iterFind;

				//返回错误信息结果   errorBegin      errorEnd 注意回环情况处理
				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				if (!jumpNode)
				{
					//需要回环处理拷贝
					if (errorBegin < iterHalfEnd && errorEnd>iterHalfBegin)
					{
						len = iterHalfEnd - errorBegin + errorEnd - iterHalfBegin;
						//因为这一套机制本质建立在危险的指针之上，因此这里为了安全，当发生数据回环情况需要拷贝保存的时候，判断长度是否在m_outRangeMaxSize之内，如果超过，宁愿判断为没有。
					//如果使用者觉得可以承受后果，那么可以在这里换成动态分配内存进行拷贝，但是要记住一点：一旦控制不好，在使用这段动态分配内存时第二次接收信息来到时一旦进行重新分配，
					//可能会触发空指针异常 程序直接崩溃
						m_log->writeLog("redis error", *m_logMessageIter++, std::string_view(errorBegin, iterHalfEnd - errorBegin),
							std::string_view(iterHalfBegin, errorEnd - iterHalfBegin));
						if (len <= m_outRangeMaxSize)
						{
							

							try
							{
								
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
						else
						{
							try
							{
								
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
					}
					else
					{
						len = errorEnd - errorBegin;

						m_log->writeLog("redis error", *m_logMessageIter++, std::string_view(errorBegin, len));
						try
						{
							
						}
						catch (const std::exception& e)
						{
							jumpNode = true;
						}
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				break;

			case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING):
				iterStrBegin = iterBegin + 1;
				if (iterStrBegin == iterHalfEnd)
				{
					iterStrBegin = iterHalfBegin;
					jump = true;
				}


				if (!jump)
				{
					if ((iterStrEnd = std::search(iterStrBegin + 1, iterHalfEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterHalfEnd)
					{
						if (*(iterHalfEnd - 1) == '\r' && *iterHalfBegin == '\n')
						{
							iterStrEnd = iterHalfEnd - 1;
							jump = true;
						}
						else if ((iterStrEnd = std::search(iterHalfBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}
				}
				else
				{
					if ((iterStrEnd = std::search(iterHalfBegin, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}


				if (!jump)
				{
					if ((iterHalfEnd - iterStrEnd) + (iterEnd - iterHalfBegin) < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					if (iterHalfEnd - iterStrEnd >= 2)
						iterFind = iterStrEnd + 2;
					else
						iterFind = iterHalfBegin + (2 - (iterHalfEnd - iterStrEnd));
				}
				else
				{
					if (iterEnd - iterHalfBegin < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterFind = iterEnd + 2;
				}
				iterBegin = iterFind;

				//返回信息结果   iterStrBegin      iterStrEnd 注意回环情况处理   需要计算len
				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				if (!jumpNode)
				{
					//需要回环处理拷贝
					if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
					{
						len = iterHalfEnd - iterStrBegin + iterStrEnd - iterHalfBegin;
						//因为这一套机制本质建立在危险的指针之上，因此这里为了安全，当发生数据回环情况需要拷贝保存的时候，判断长度是否在m_outRangeMaxSize之内，如果超过，宁愿判断为没有。
					//如果使用者觉得可以承受后果，那么可以在这里换成动态分配内存进行拷贝，但是要记住一点：一旦控制不好，在使用这段动态分配内存时第二次接收信息来到时一旦进行重新分配，
					//可能会触发空指针异常 程序直接崩溃
						if (len <= m_outRangeMaxSize)
						{
							

							try
							{
								
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
						else
						{
							try
							{
								
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
					}
					else
					{
						len = iterStrEnd - iterStrBegin;

						try
						{
							
						}
						catch (const std::exception& e)
						{
							jumpNode = true;
						}
					}
				}

				++m_logMessageIter;
				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}



				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				break;

				/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			case static_cast<int>(REDISPARSETYPE::ARRAY):
				//REDISPARSETYPE::ARRAY

				if (!jumpNode)
					m_arrayResult.clear();

				++iterFind;
				if (iterFind == iterHalfEnd)
				{
					iterFind = iterHalfBegin;
					jump = true;
				}

				arrayNumBegin = iterFind;

				if (!jump)
				{
					if ((arrayNumEnd = std::find(arrayNumBegin + 1, iterHalfEnd, '\r')) == iterHalfEnd)
					{
						if ((arrayNumEnd = std::find(iterHalfBegin, iterEnd, '\r')) == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							return false;
						}
						jump = true;
					}
				}
				else
				{
					if ((arrayNumEnd = std::find(arrayNumBegin + 1, iterEnd, '\r')) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}


				index = -1;
				num = 1;
				if (!jump)
				{
					arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto& sum, auto const ch)
					{
						if (++index)
							num *= 10;
						return sum += (ch - '0') * num;
					});
				}
				else
				{
					if (arrayNumBegin < iterHalfEnd)
					{
						arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(iterHalfBegin), 0, [&index, &num](auto& sum, auto const ch)
						{
							if (++index)
								num *= 10;
							return sum += (ch - '0') * num;
						});
						arrayNum += std::accumulate(std::make_reverse_iterator(iterHalfEnd), std::make_reverse_iterator(arrayNumBegin), arrayNum, [&index, &num](auto& sum, auto const ch)
						{
							if (++index)
								num *= 10;
							return sum += (ch - '0') * num;
						});
					}
					else
					{
						arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto& sum, auto const ch)
						{
							if (++index)
								num *= 10;
							return sum += (ch - '0') * num;
						});
					}
				}



				if (!jump)
				{
					if (iterHalfEnd - arrayNumEnd + (iterEnd - iterHalfBegin) < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}
				else
				{
					if (iterEnd - arrayNumEnd < 2)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
				}


				if (!jump)
				{
					if (iterHalfEnd - arrayNumEnd >= 2)
					{
						iterFind = arrayNumEnd + 2;
					}
					else
					{
						iterFind = iterHalfBegin + 2 - (iterHalfEnd - arrayNumEnd);
						jump = true;
					}
				}
				else
				{
					iterFind = arrayNumEnd + 2;
				}


				arrayIndex = -1;
				while (++arrayIndex != arrayNum)
				{
					if (!jump)
					{
						if (iterFind == iterHalfEnd)
						{
							iterFind = iterHalfBegin;
							jump = true;
						}
						else
						{
							++iterFind;
						}
					}
					else
					{
						if (++iterFind == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}



					if (*iterFind == '-')
					{
						if (!jump)
						{
							if (iterHalfEnd - iterFind + iterEnd - iterHalfBegin < 4)
							{
								m_lastPrasePos = iterBegin - m_receiveBuffer.get();
								m_jumpNode = jumpNode;
								m_waitMessageListBegin = waitMessageListBegin;
								m_commandTotalSize = commandTotalSize;
								m_commandCurrentSize = commandCurrentSize;
								return false;
							}
						}
						else
						{
							if (iterEnd - iterFind < 4)
							{
								m_lastPrasePos = iterBegin - m_receiveBuffer.get();
								m_jumpNode = jumpNode;
								m_waitMessageListBegin = waitMessageListBegin;
								m_commandTotalSize = commandTotalSize;
								m_commandCurrentSize = commandCurrentSize;
								return false;
							}
						}


						if (!jump)
						{
							if (iterHalfEnd - iterFind >= 4)
								iterFind += 4;
							else
							{
								iterFind = iterHalfBegin + 4 - (iterHalfEnd - iterFind);
								jump = true;
							}
						}
						else
						{
							iterFind += 4;
						}

						///////////////////////   本次没有结果，考虑怎么返回比较方便  -1  //////////////////////////////////////////////////////

						if (!jumpNode)
						{
							try
							{
								m_arrayResult.emplace_back(emptyView);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}

						///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
						continue;
					}


					if (!jump)
					{
						if (iterFind < iterHalfEnd)
							++iterFind;
						if (iterFind == iterHalfEnd)
						{
							iterFind = iterHalfBegin;
							jump = true;
						}
					}
					else
					{
						if (iterFind + 1 == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}

					iterLenBegin = iterFind;

					if (!jump)
					{
						if ((iterLenEnd = std::find(iterLenBegin + 1, iterHalfEnd, '\r')) == iterHalfEnd)
						{
							if ((iterLenEnd = std::find(iterHalfBegin, iterEnd, '\r')) == iterEnd)
							{
								m_lastPrasePos = iterBegin - m_receiveBuffer.get();
								m_jumpNode = jumpNode;
								m_waitMessageListBegin = waitMessageListBegin;
								m_commandTotalSize = commandTotalSize;
								m_commandCurrentSize = commandCurrentSize;
								return false;
							}
							jump = true;
						}
					}
					else
					{
						if ((iterLenEnd = std::find(iterLenBegin + 1, iterEnd, '\r')) == iterEnd)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}


					index = -1;
					num = 1;
					if (!jump)
					{
						len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
						{
							if (++index)
								num *= 10;
							return sum += (ch - '0') * num;
						});
					}
					else
					{
						if (iterLenBegin < iterHalfEnd)
						{
							len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterHalfBegin), 0, [&index, &num](auto& sum, auto const ch)
							{
								if (++index)
									num *= 10;
								return sum += (ch - '0') * num;
							});
							len += std::accumulate(std::make_reverse_iterator(iterHalfEnd), std::make_reverse_iterator(iterLenBegin), len, [&index, &num](auto& sum, auto const ch)
							{
								if (++index)
									num *= 10;
								return sum += (ch - '0') * num;
							});
						}
						else
						{
							len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
							{
								if (++index)
									num *= 10;
								return sum += (ch - '0') * num;
							});
						}
					}


					if (!jump)
					{
						if (iterHalfEnd - iterLenEnd + iterEnd - iterHalfBegin < (4 + len))
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}
					else
					{
						if (iterEnd - iterLenEnd < (4 + len))
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}
					}

					if (!jump)
					{
						if (iterHalfEnd - iterLenEnd >= 2)
							iterStrBegin = iterLenEnd + 2;
						else
						{
							iterStrBegin = iterHalfBegin + 2 - (iterHalfEnd - iterLenEnd);
							jump = true;
						}
					}
					else
					{
						iterStrBegin = iterLenEnd + 2;
					}


					if (!jump)
					{
						if (iterHalfEnd - iterStrBegin >= len)
							iterStrEnd = iterStrBegin + len;
						else
						{
							iterStrEnd = iterHalfBegin + len - (iterHalfEnd - iterStrBegin);
							jump = true;
						}
					}
					else
					{
						iterStrEnd = iterStrBegin + len;
					}


					if (!jump)
					{
						if (iterHalfEnd - iterStrEnd >= 2)
							iterFind = iterStrEnd + 2;
						else
						{
							iterFind = iterHalfBegin + 2 - (iterHalfEnd - iterStrEnd);
							jump = true;
						}
					}
					else
					{
						iterFind = iterStrEnd + 2;
					}


					//////////////////////////////////////////////  每次的有效结果，注意回环处理    iterStrBegin      iterStrEnd   len////////////////////


					if (!jumpNode)
					{
						//需要回环处理拷贝
						if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
						{
							//因为这一套机制本质建立在危险的指针之上，因此这里为了安全，当发生数据回环情况需要拷贝保存的时候，判断长度是否在m_outRangeMaxSize之内，如果超过，宁愿判断为没有。
						//如果使用者觉得可以承受后果，那么可以在这里换成动态分配内存进行拷贝，但是要记住一点：一旦控制不好，在使用这段动态分配内存时第二次接收信息来到时一旦进行重新分配，
						//可能会触发空指针异常 程序直接崩溃
							if (len <= m_outRangeMaxSize)
							{
								std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
								std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

								try
								{
									m_arrayResult.emplace_back(std::string_view(m_outRangeBuffer.get(), len));
								}
								catch (const std::exception& e)
								{
									jumpNode = true;
								}
							}
							else
							{
								try
								{
									m_arrayResult.emplace_back(emptyView);
								}
								catch (const std::exception& e)
								{
									jumpNode = true;
								}
							}
						}
						else
						{
							try
							{
								m_arrayResult.emplace_back(std::string_view(iterStrBegin, len));
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}
					}

					/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


				}
				iterBegin = iterFind;

				//////////////////////////////////////////////////////////////////////////////////////////


				if (!jumpNode)
				{
					std::vector<std::string_view>& vec = std::get<4>(thisRequest).get();
					try
					{
						vec.swap(m_arrayResult);
						std::get<5>(thisRequest).get().emplace_back(vec.size());
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				////////////////////////////////////////////////////////////////////////////////////////////////////

				break;

			default:


				break;
			}
		}




		////////////////////////////////////////

		while (iterBegin != iterEnd)
		{
			iterFind = iterBegin;
			redisMessageListTypeSW& thisRequest{ **waitMessageListBegin };
			switch (*iterFind)
			{
			case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING):
				//  REDISPARSETYPE::LOT_SIZE_STRING
				if (++iterFind == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				if (*iterFind == '-')
				{
					if (iterEnd - iterFind >= 4)
					{
						iterFind += 4;
						iterBegin = iterFind;
						////////////////////////////////////////////////////////////////////////////////////////////////////////////
						//本次结果返回无  -1，看情况处理

						if (!jumpNode)
						{
							try
							{
								std::get<4>(thisRequest).get().emplace_back(emptyView);
								std::get<5>(thisRequest).get().emplace_back(1);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}


						if (++commandCurrentSize == commandTotalSize)
						{
							//根据jumpNode状态 调用回调函数返回本次成功与失败结果
							if (jumpNode)
							{



							}
							else
							{
								std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
							}

							jumpNode = false;
							if (++waitMessageListBegin != waitMessageListEnd)
							{
								commandTotalSize = std::get<1>(**waitMessageListBegin);
								commandCurrentSize = 0;
							}
						}
						//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

						continue;
					}

					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}
				else
				{
					iterLenBegin = iterFind;

					if ((iterLenEnd = std::find(iterLenBegin + 1, iterEnd, '\r')) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
					else
					{
						index = -1;
						num = 1;
						len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
						{
							if (++index)
								num *= 10;
							return sum += (ch - '0') * num;
						});
					}

					if (iterEnd - iterLenEnd < (4 + len))
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}
					else
					{
						iterStrBegin = iterLenEnd + 2;
						iterStrEnd = iterStrBegin + len;

						//本次结果获取成功，看情况进行处理

						//////////////////////////////////////////////////////////////////////////////////////////////////////////////
						if (!jumpNode)
						{
							try
							{
								std::get<4>(thisRequest).get().emplace_back(std::string_view(iterStrBegin, len));
								std::get<5>(thisRequest).get().emplace_back(1);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}


						if (++commandCurrentSize == commandTotalSize)
						{
							//根据jumpNode状态 调用回调函数返回本次成功与失败结果
							if (jumpNode)
							{



							}
							else
							{
								std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
							}

							jumpNode = false;
							if (++waitMessageListBegin != waitMessageListEnd)
							{
								commandTotalSize = std::get<1>(**waitMessageListBegin);
								commandCurrentSize = 0;
							}
						}


						///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
						iterBegin = iterStrEnd + 2;
						continue;
					}
				}

				break;

			case static_cast<int>(REDISPARSETYPE::INTERGER):
				// REDISPARSETYPE::INTERGER

				if ((iterLenEnd = std::find(iterFind + 1, iterEnd, '\r')) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				if (iterEnd - iterLenEnd < 2)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterLenBegin = iterFind + 1;

				iterFind = iterLenEnd + 2;
				iterBegin = iterFind;

				//结果iterLenBegin    iterLenEnd
				////////////////////////////////////////////////////////////////////////
				if (!jumpNode)
				{
					try
					{
						std::get<4>(thisRequest).get().emplace_back(std::string_view(iterLenBegin, iterLenEnd - iterLenBegin));
						std::get<5>(thisRequest).get().emplace_back(1);
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				///////////////////////////////////////////////////////////////////////////////////////////////////
				break;

			case static_cast<int>(REDISPARSETYPE::ERROR):
				//REDISPARSETYPE::ERROR

				errorBegin = iterBegin + 1;
				if ((errorEnd = std::search(errorBegin + 1, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterFind = errorEnd + 2;
				iterBegin = iterFind;
				//返回错误信息结果
				////////////////////////////////////////////////////////////////////////////////////////////

				if (!jumpNode)
				{
					try
					{
						
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}

				m_log->writeLog("redis error", *m_logMessageIter++, std::string_view(errorBegin, errorEnd - errorBegin));
				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				//////////////////////////////////////////////////////////////////////////////////////
				break;

			case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING):
				//REDISPARSETYPE::SIMPLE_STRING

				iterStrBegin = iterBegin + 1;
				if ((iterStrEnd = std::search(iterStrBegin + 1, iterEnd, STATICSTRING::smallNewline, STATICSTRING::smallNewline + STATICSTRING::smallNewlineLen)) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterFind = iterStrEnd + 2;
				iterBegin = iterFind;
				//返回错误信息结果
				//////////////////////////////////////////////////////////////////////////////////////////////////////////

				if (!jumpNode)
				{
					try
					{
						
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}

				++m_logMessageIter;
				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}


				//////////////////////////////////////////////////////////////////////////////////////////////
				break;


			case static_cast<int>(REDISPARSETYPE::ARRAY):
				//REDISPARSETYPE::ARRAY

				if (!jumpNode)
					m_arrayResult.clear();

				if (++iterFind == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				arrayNumBegin = iterFind;

				if ((arrayNumEnd = std::find(arrayNumBegin + 1, iterEnd, '\r')) == iterEnd)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				index = -1;
				num = 1;
				arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto& sum, auto const ch)
				{
					if (++index)
						num *= 10;
					return sum += (ch - '0') * num;
				});

				if (iterEnd - arrayNumEnd < 2)
				{
					m_lastPrasePos = iterBegin - m_receiveBuffer.get();
					m_jumpNode = jumpNode;
					m_waitMessageListBegin = waitMessageListBegin;
					m_commandTotalSize = commandTotalSize;
					m_commandCurrentSize = commandCurrentSize;
					return false;
				}

				iterFind = arrayNumEnd + 2;

				arrayIndex = -1;
				while (++arrayIndex != arrayNum)
				{
					if (++iterFind == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					if (*iterFind == '-')
					{
						if (iterEnd - iterFind < 4)
						{
							m_lastPrasePos = iterBegin - m_receiveBuffer.get();
							m_jumpNode = jumpNode;
							m_waitMessageListBegin = waitMessageListBegin;
							m_commandTotalSize = commandTotalSize;
							m_commandCurrentSize = commandCurrentSize;
							return false;
						}

						iterFind += 4;
						///////////////////////   本次没有结果，考虑怎么返回比较方便  -1////////////////////

						if (!jumpNode)
						{
							try
							{
								m_arrayResult.emplace_back(emptyView);
							}
							catch (const std::exception& e)
							{
								jumpNode = true;
							}
						}

						//////////////////////////////////////////////////////////////////////////////////////
						continue;
					}

					if (iterFind + 1 == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterLenBegin = iterFind;

					if ((iterLenEnd = std::find(iterLenBegin + 1, iterEnd, '\r')) == iterEnd)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					index = -1;
					num = 1;
					len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto& sum, auto const ch)
					{
						if (++index)
							num *= 10;
						return sum += (ch - '0') * num;
					});

					if (iterEnd - iterLenEnd < 4 + len)
					{
						m_lastPrasePos = iterBegin - m_receiveBuffer.get();
						m_jumpNode = jumpNode;
						m_waitMessageListBegin = waitMessageListBegin;
						m_commandTotalSize = commandTotalSize;
						m_commandCurrentSize = commandCurrentSize;
						return false;
					}

					iterStrBegin = iterLenEnd + 2;
					iterStrEnd = iterStrBegin + len;

					iterFind = iterStrEnd + 2;

					//////////////////////////////////////////////  每次的有效结果

					if (!jumpNode)
					{
						try
						{
							m_arrayResult.emplace_back(std::string_view(iterStrBegin, len));
						}
						catch (const std::exception& e)
						{
							jumpNode = true;
						}
					}



					//////////////////////////////////////////////////////////////
				}
				iterBegin = iterFind;

				///////////////////////////////////////////////////////////////////////// 这里进行参数传递


				if (!jumpNode)
				{
					std::vector<std::string_view>& vec = std::get<4>(thisRequest).get();
					try
					{
						vec.swap(m_arrayResult);
						std::get<5>(thisRequest).get().emplace_back(vec.size());
					}
					catch (const std::exception& e)
					{
						jumpNode = true;
					}
				}


				if (++commandCurrentSize == commandTotalSize)
				{
					//根据jumpNode状态 调用回调函数返回本次成功与失败结果
					if (jumpNode)
					{



					}
					else
					{
						std::get<6>(thisRequest)(true, ERRORMESSAGE::OK);
					}

					jumpNode = false;
					if (++waitMessageListBegin != waitMessageListEnd)
					{
						commandTotalSize = std::get<1>(**waitMessageListBegin);
						commandCurrentSize = 0;
					}
				}



				///////////////////////////////////////////////////////////////////////////////////////
				break;
			default:

				break;
			}
		}

	}

	//  如果已经处理完整个队列，则返回true,	
	m_lastPrasePos = iterBegin - m_receiveBuffer.get();
	m_jumpNode = jumpNode;
	m_waitMessageListBegin = waitMessageListBegin;
	m_commandTotalSize = commandTotalSize;
	m_commandCurrentSize = commandCurrentSize;

	if (m_waitMessageListBegin == m_waitMessageListEnd)
		return true;
	return false;
}




void MULTIREDISWRITE::handlelRead(const boost::system::error_code& err, const std::size_t size)
{
	if (err)
	{
		m_log->writeLog(__FILE__, __LINE__, err.what());

		m_connectStatus = 2;

		ec = {};
		m_sock->shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

		static std::function<void()>socketTimeOut{ [this]() {shutdownLoop(); } };
		m_timeWheel->insert(socketTimeOut, 5);
	}
	else
	{
		if (size)
		{
			m_receiveBufferNowSize += size;

			//每次进行解析之前需要保存当前解析节点位置，失败时进行比对，如果整个buffer都被塞满了依然还是没有变化，则将当前处理队列的节点全部返回错误，然后检查请求队列，生成新的请求消息，
			//因为这个算法快速的关键在于极致利用原buffer数据

			m_waitMessageListBeforeParse = m_waitMessageListBegin;

			if (!parse(size))
			{
				// 
				m_waitMessageListAfterParse = m_waitMessageListBegin;

				if (m_waitMessageListBeforeParse == m_waitMessageListAfterParse)
				{
					if (m_receiveBufferNowSize == m_lastPrasePos)
					{
						// 对m_waitMessageListBegin 到 m_waitMessageListEnd 统统返回解析错误状态

						do
						{

							std::get<6>(**m_waitMessageListBegin)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
						} while (++m_waitMessageListBegin != m_waitMessageListEnd);


						readyMessage();
					}
					else
					{
						receive();
					}
				}
				else
				{
					receive();
				}
			}
			else
			{
				// 全部解析完全了，判断m_waitMessageListBegin == m_waitMessageListEnd
				//设置一个while循环来判断
				//加锁首先直接尝试取commandSize个元素，如果队列为空，则退出，设置处理状态为否
				//如果获取到元素，则遍历计算需要的空间为多少
				//尝试分配空间，分配失败则continue处理
				//成功则copy组合，进入发送命令阶段循环


				readyMessage();
			}
		}
		else
		{
			receive();
		}
	}
}


//pipline流水线命令封装函数
//设置一个while循环来判断
//加锁首先直接尝试取commandSize个元素，如果队列为空，则退出，设置处理状态为否
//如果获取到元素，则遍历计算需要的空间为多少
//尝试分配空间，分配失败则continue处理
//成功则copy组合，进入发送命令阶段循环
void MULTIREDISWRITE::readyMessage()
{

	//先将需要用到的关键变量复位，再尝试生成新的请求消息
	std::vector<std::string>::const_iterator souceBegin, souceEnd;
	std::vector<unsigned int>::const_iterator lenBegin, lenEnd;
	char* messageIter{}, * LMBegin{}, * LMEnd{};

	//计算本次所需要的空间大小
	static const int divisor{ 10 };
	int totalLen{}, everyLen{}, index{}, temp{}, thisStrLen{}, everyCommandNum{};

	while (true)
	{
		m_waitMessageListBegin = m_waitMessageList.get();
		m_waitMessageListEnd = m_waitMessageList.get() + m_commandMaxSize;


		//从m_messageList中获取元素到m_waitMessageList中


		do
		{
			if (!m_messageList.try_dequeue(*m_waitMessageListBegin))
				break;
			++m_waitMessageListBegin;
		} while (m_waitMessageListBegin != m_waitMessageListEnd);
		if (m_waitMessageListBegin == m_waitMessageList.get())
		{
			m_queryStatus.store(false, std::memory_order_release);
			break;
		}


		//尝试计算总命令个数  命令字符串所需要总长度,不用检查非空问题
		m_waitMessageListEnd = m_waitMessageListBegin;
		m_waitMessageListBegin = m_waitMessageList.get();
		totalLen = 0;
		everyCommandNum = 0;

		do
		{
			redisMessageListTypeSW& request{ **m_waitMessageListBegin };

			std::vector<std::string>& sourceVec = std::get<0>(request);
			std::vector<unsigned int>& lenVec = std::get<2>(request);
			everyCommandNum += std::get<1>(request);
			souceBegin = sourceVec.cbegin(), souceEnd = sourceVec.cend();
			lenBegin = lenVec.cbegin(), lenEnd = lenVec.cend();


			do
			{
				everyLen = *lenBegin;
				if (everyLen)
				{
					totalLen += 1;

					temp = everyLen, thisStrLen = 1;
					while (temp /= divisor)
						++thisStrLen;
					totalLen += thisStrLen + 2;

					index = -1;
					while (++index != everyLen)
					{
						totalLen += 1;
						temp = souceBegin->size(), thisStrLen = 1;
						while (temp /= divisor)
							++thisStrLen;
						totalLen += thisStrLen + 2 + souceBegin->size() + 2;
						++souceBegin;
					}
				}
			} while (++lenBegin != lenEnd);
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);



		//计算总长度完毕，尝试进行分配空间
		//分配失败时候记得遍历处理，返回错误
		if (totalLen > m_messageBufferMaxSize)
		{
			try
			{
				m_messageBuffer.reset(new char[totalLen]);
				m_messageBufferMaxSize = totalLen;
			}
			catch (const std::exception& e)
			{
				m_messageBufferMaxSize = 0;

				m_waitMessageListBegin = m_waitMessageList.get();
				do
				{
					std::get<6>(**m_waitMessageListBegin)(false, ERRORMESSAGE::STD_BADALLOC);

				} while (++m_waitMessageListBegin != m_waitMessageListEnd);
				continue;
			}
		}

		m_messageBufferNowSize = totalLen;

		//////////////////////////////////////////////////////////////////////////

		if (m_logMessageSize < everyCommandNum)
		{
			try
			{
				m_logMessage.reset(new std::string_view[everyCommandNum]);
				m_logMessageSize = everyCommandNum;
			}
			catch (const std::exception& e)
			{
				m_logMessageSize = 0;

				m_waitMessageListBegin = m_waitMessageList.get();
				do
				{
					std::get<6>(**m_waitMessageListBegin)(false, ERRORMESSAGE::STD_BADALLOC);

				} while (++m_waitMessageListBegin != m_waitMessageListEnd);
				continue;
			}
		}

		m_logMessageIter = m_logMessage.get();






		/////////生成请求命令字符串

		m_waitMessageListBegin = m_waitMessageList.get();
		messageIter = m_messageBuffer.get();
		do
		{
			redisMessageListTypeSW& request{ **m_waitMessageListBegin };

			std::vector<std::string>& sourceVec = std::get<0>(request);
			std::vector<unsigned int>& lenVec = std::get<2>(request);
			souceBegin = sourceVec.cbegin(), souceEnd = sourceVec.cend();
			lenBegin = lenVec.cbegin(), lenEnd = lenVec.cend();

			souceBegin = sourceVec.cbegin(), lenBegin = lenVec.cbegin();



			do
			{
				LMBegin = messageIter;
				everyLen = *lenBegin;
				if (everyLen)
				{
					*messageIter++ = '*';

					if (everyLen > 99999999)
						*messageIter++ = everyLen / 100000000 + '0';
					if (everyLen > 9999999)
						*messageIter++ = everyLen / 10000000 % 10 + '0';
					if (everyLen > 999999)
						*messageIter++ = everyLen / 1000000 % 10 + '0';
					if (everyLen > 99999)
						*messageIter++ = everyLen / 100000 % 10 + '0';
					if (everyLen > 9999)
						*messageIter++ = everyLen / 10000 % 10 + '0';
					if (everyLen > 999)
						*messageIter++ = everyLen / 1000 % 10 + '0';
					if (everyLen > 99)
						*messageIter++ = everyLen / 100 % 10 + '0';
					if (everyLen > 9)
						*messageIter++ = everyLen / 10 % 10 + '0';
					if (everyLen > 0)
						*messageIter++ = everyLen % 10 + '0';

					std::copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
					messageIter += HTTPRESPONSE::halfNewLineLen;

					index = -1;
					while (++index != everyLen)
					{
						*messageIter++ = '$';

						temp = souceBegin->size();

						if (temp > 99999999)
							*messageIter++ = temp / 100000000 + '0';
						if (temp > 9999999)
							*messageIter++ = temp / 10000000 % 10 + '0';
						if (temp > 999999)
							*messageIter++ = temp / 1000000 % 10 + '0';
						if (temp > 99999)
							*messageIter++ = temp / 100000 % 10 + '0';
						if (temp > 9999)
							*messageIter++ = temp / 10000 % 10 + '0';
						if (temp > 999)
							*messageIter++ = temp / 1000 % 10 + '0';
						if (temp > 99)
							*messageIter++ = temp / 100 % 10 + '0';
						if (temp > 9)
							*messageIter++ = temp / 10 % 10 + '0';
						if (temp > 0)
							*messageIter++ = temp % 10 + '0';

						std::copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
						messageIter += HTTPRESPONSE::halfNewLineLen;

						std::copy(souceBegin->cbegin(), souceBegin->cend(), messageIter);
						messageIter += souceBegin->size();

						std::copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
						messageIter += HTTPRESPONSE::halfNewLineLen;

						++souceBegin;
					}
				}
				LMEnd = messageIter;
				*m_logMessageIter++ = std::string_view(LMBegin, std::distance(LMBegin, LMEnd));

			} while (++lenBegin != lenEnd);
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);



		////   准备发送

		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(**m_waitMessageListBegin);

		m_commandCurrentSize = 0;

		m_logMessageIter = m_logMessage.get();
		//////////////////////////////////////////////////////
		//进入发送函数

		query();

		break;
	}
}



void MULTIREDISWRITE::shutdownLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_sock->shutdown(boost::asio::socket_base::shutdown_send, ec);
		m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
	}
	else
	{
		m_sock->cancel(ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
}


void MULTIREDISWRITE::cancelLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_sock->cancel(ec);
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
	else
	{
		m_sock->close(ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {closeLoop(); }, 5);
	}
}


void MULTIREDISWRITE::closeLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_sock->close(ec);
		m_timeWheel->insert([this]() {closeLoop(); }, 5);
	}
	else
	{
		resetSocket();
	}
}


void MULTIREDISWRITE::resetSocket()
{
	m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));
	m_sock->set_option(boost::asio::socket_base::keep_alive(true), ec);

	if (m_connectStatus == 1)
		firstConnect();
	else if (m_connectStatus == 2)
		reconnect();
}


//回调函数记录log使用
//因为这个类仅仅执行非读取命令且不需要获取返回结果时使用
//因此仅仅需要考虑redis  SIMPLE_STRING  ERROR  以及readyMessage中申请空间失败   parse失败的情况
//其中ERROR情况已经在解析函数做了log处理
void MULTIREDISWRITE::logCallBack(bool result, ERRORMESSAGE em)
{
	if (!m_waitMessageListBegin || !*m_waitMessageListBegin)
		return;

	m_log->writeLog("redis error", std::get<0>(**m_waitMessageListBegin));
}
