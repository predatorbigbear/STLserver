#include "multiRedisWrite.h"

MULTIREDISWRITE::MULTIREDISWRITE(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, 
	const unsigned int redisPort, const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
	:m_redisIP(redisIP), m_redisPort(redisPort), m_ioc(ioc), m_unlockFun(unlockFun), m_receiveBufferMaxSize(memorySize), m_outRangeMaxSize(outRangeMaxSize),
	m_commandMaxSize(commandSize)
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
	if (!memorySize)
		throw std::runtime_error("memorySize is invaild");
	if (!outRangeMaxSize)
		throw std::runtime_error("outRangeMaxSize is invaild");
	if (!commandSize)
		throw std::runtime_error("commandSize is invaild");

	m_timer.reset(new boost::asio::steady_timer(*m_ioc));
	m_receiveBuffer.reset(new char[memorySize]);


	m_outRangeBuffer.reset(new char[m_outRangeMaxSize]);


	m_waitMessageList.reset(new std::shared_ptr<redisWriteTypeSW>[m_commandMaxSize]);
	m_waitMessageListMaxSize = m_commandMaxSize;

	m_charMemoryPool.reset();

	firstConnect();
}




bool MULTIREDISWRITE::insertRedisRequest(std::shared_ptr<redisWriteTypeSW> redisRequest)
{
	if (!redisRequest || std::get<0>(*redisRequest).get().empty() || !std::get<1>(*redisRequest) || std::get<1>(*redisRequest) > m_commandMaxSize
		|| std::get<1>(*redisRequest) != std::get<2>(*redisRequest).get().size() ||
		std::accumulate(std::get<2>(*redisRequest).get().cbegin(), std::get<2>(*redisRequest).get().cend(), 0) != std::get<0>(*redisRequest).get().size()
		)
	{
		return false;
	}

	m_mutex.lock();
	if (!m_connect)
	{
		m_mutex.unlock();
		return false;
	}
	if (!m_queryStatus)
	{
		m_queryStatus = true;
		m_mutex.unlock();

		//////////////////////////////////////////////////////////////////////

		std::vector<std::string_view> &sourceVec{ std::get<0>(*redisRequest).get() };
		std::vector<unsigned int> &lenVec{ std::get<2>(*redisRequest).get() };
		std::vector<std::string_view>::const_iterator souceBegin{ sourceVec.cbegin() }, souceEnd{ sourceVec.cend() };
		std::vector<unsigned int>::const_iterator lenBegin{ lenVec.cbegin() }, lenEnd{ lenVec.cend() };
		char *messageIter{};

		//计算本次所需要的空间大小
		int totalLen{}, everyLen{}, index{}, divisor{ 10 }, temp{}, thisStrLen{};
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



		////////////////////////////////////////////

		if (totalLen > m_messageBufferMaxSize)
		{
			try
			{
				m_messageBuffer.reset(new char[totalLen]);
				m_messageBufferMaxSize = totalLen;
			}
			catch (const std::exception &e)
			{
				m_messageBufferMaxSize = 0;
				m_mutex.lock();
				m_queryStatus = false;
				m_mutex.unlock();
				return false;
			}
		}

		m_messageBufferNowSize = totalLen;


		//copy生成到发送字符串buffer中

		souceBegin = sourceVec.cbegin(), lenBegin = lenVec.cbegin();

		messageIter = m_messageBuffer.get();


		do
		{
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
		} while (++lenBegin != lenEnd);

		
		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		*m_waitMessageListBegin++ = redisRequest;

		m_waitMessageListEnd = m_waitMessageListBegin;

		m_waitMessageListBegin = m_waitMessageList.get();

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(*redisRequest);

		m_commandCurrentSize = 0;
		//////////////////////////////////////////////////////
		//进入发送函数

		query();

		return true;
	}
	else
	{
		m_mutex.unlock();

		std::vector<std::string_view> &sourceVec{ std::get<0>(*redisRequest).get() };
		std::vector<std::string_view>::iterator souceBegin{ sourceVec.begin() }, souceEnd{ sourceVec.end() };

		unsigned int bufferLen{ std::accumulate(souceBegin,souceEnd,0,[](auto &sum,auto const sw)
		{
			return sum += sw.size();
		}) };

		if (bufferLen)
		{
			char *buffer{ m_charMemoryPool.getMemory<char*>(bufferLen) };

			if (!buffer)
				return false;

			while (souceBegin != souceEnd)
			{
				if (!souceBegin->empty())
				{
					std::copy(souceBegin->cbegin(), souceBegin->cend(), buffer);
					std::string_view sw(buffer, souceBegin->size());
					souceBegin->swap(sw);
					buffer += souceBegin->size();
					++souceBegin;
				}
			}
		}

		m_messageList.lock();

		if (!m_messageList.unsafeInsert(redisRequest))
		{
			m_messageList.unlock();
			return false;
		}

		m_messageList.unlock();

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
	catch (const std::exception &e)
	{
		return false;
	}
	catch (const boost::system::system_error &err)
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
	catch (const std::exception &e)
	{
		return false;
	}
	catch (const boost::system::system_error &err)
	{
		return false;
	}
}




void MULTIREDISWRITE::firstConnect()
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
			std::cout << "connect multi RedisWrite success\n";
			setConnectSuccess();
			(*m_unlockFun)();
		}
	});
}



void MULTIREDISWRITE::setConnectSuccess()
{
	m_mutex.lock();
	m_connect = true;
	m_mutex.unlock();
}




void MULTIREDISWRITE::setConnectFail()
{
	m_mutex.lock();
	m_connect = false;
	m_mutex.unlock();
}




void MULTIREDISWRITE::query()
{
	boost::asio::async_write(*m_sock, boost::asio::buffer(m_sendMessage, m_sendLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		if (err)  //log
		{

		}
		else
		{
			receive();
		}
	});
}




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
	m_sock->async_read_some(boost::asio::buffer(m_receiveBuffer.get() + m_thisReceivePos, m_thisReceiveLen), [this](const boost::system::error_code &err, const std::size_t size)
	{
		handlelRead(err, size);
	});
}



void MULTIREDISWRITE::resetReceiveBuffer()
{
	m_receiveBufferNowSize = m_lastPrasePos = 0;
}



void MULTIREDISWRITE::handlelRead(const boost::system::error_code & err, const std::size_t size)
{
	if (err)
	{
		std::cout << "err\n";
	}
	else
	{

		
		if (size)
		{
			m_receiveBufferNowSize += size;

			//每次进行解析之前需要保存当前解析节点位置，失败时进行比对，如果整个buffer都被塞满了依然还是没有变化，则将当前处理队列的节点全部返回错误，然后检查请求队列，生成新的请求消息，
			//因为这个算法快速的关键在于极致利用原buffer数据

			m_waitMessageListBeforeParse = m_waitMessageListBegin;


			if (!prase(size))
			{
				// 
				m_waitMessageListAfterParse = m_waitMessageListBegin;

				if (m_waitMessageListBeforeParse == m_waitMessageListAfterParse)
				{
					if (m_receiveBufferNowSize == m_lastPrasePos)
					{
						// 对m_waitMessageListBegin 到 m_waitMessageListEnd 统统返回解析错误状态

						std::shared_ptr<redisWriteTypeSW> request;
						do
						{
							request = *m_waitMessageListBegin;
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




bool MULTIREDISWRITE::prase(const int size)
{
	const char *iterBegin{ }, *iterEnd{ m_receiveBuffer.get() + m_receiveBufferNowSize }, *iterHalfBegin{ m_receiveBuffer.get() },
		*iterHalfEnd{ m_receiveBuffer.get() + m_receiveBufferMaxSize }, *iterFind{},        // 开始 结束  寻找节点
		*iterLenBegin{}, *iterLenEnd{},               //长度节点
		*iterStrBegin{}, *iterStrEnd{},               //本次SIMPLE_STRING首尾位置
		*errorBegin{}, *errorEnd{},
		*arrayNumBegin{}, *arrayNumEnd{};

	// m_lastPrasePos    m_jumpNode  m_waitMessageListBegin   m_commandTotalSize    m_commandCurrentSize返回时需要同步处理

	bool jumpNode{ m_jumpNode }, jump{ false };  //是否已经经过回环情况
	std::shared_ptr<redisWriteTypeSW> *waitMessageListBegin = m_waitMessageListBegin;
	std::shared_ptr<redisWriteTypeSW> *waitMessageListEnd = m_waitMessageListEnd;
	unsigned int commandTotalSize{ m_commandTotalSize }, commandCurrentSize{ m_commandCurrentSize };

	// debug  ////////////

	/////////////////////////////


	unsigned int len{};     //计算字符串长度使用
	int index{}, num{}, arrayNum{}, arrayIndex{};


	if (m_lastPrasePos == m_receiveBufferMaxSize)
		iterBegin = m_receiveBuffer.get();
	else
		iterBegin = m_receiveBuffer.get() + m_lastPrasePos;


	//接收数据没有回环情况
	if (iterBegin < iterEnd)
	{
		////////////////////////////////

		while (iterBegin != iterEnd)
		{
			iterFind = iterBegin;
			switch (*iterFind)
			{
				case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
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
																					   
																				   }
																				   catch (const std::exception &e)
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
																			   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																			   {
																				   if (++index)
																					   num *= 10;
																				   return sum += (ch - '0')*num;
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
																					  
																				   }
																				   catch (const std::exception &e)
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

																	   case static_cast<int>(REDISPARSETYPE::INTERGER) :
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
																															   
																														   }
																														   catch (const std::exception &e)
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

																													   case static_cast<int>(REDISPARSETYPE::ERROR) :
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
																															   catch (const std::exception &e)
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

																														   case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
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
																																   catch (const std::exception &e)
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


																															   case static_cast<int>(REDISPARSETYPE::ARRAY) :
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
																																   arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto &sum, auto const ch)
																																   {
																																	   if (++index)
																																		   num *= 10;
																																	   return sum += (ch - '0')*num;
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
																																				  
																																			   }
																																			   catch (const std::exception &e)
																																			   {
																																				   jumpNode = true;
																																			   }
																																		   }

																																		   //////////////////////////////////////////////////////////////////////////////////////
																																		   continue;
																																	   }

																																	   if (++iterFind == iterEnd)
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
																																	   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																																	   {
																																		   if (++index)
																																			   num *= 10;
																																		   return sum += (ch - '0')*num;
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
																																			   
																																		   }
																																		   catch (const std::exception &e)
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
																																	  
																																	   try
																																	   {
																																		   
																																	   }
																																	   catch (const std::exception &e)
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
			switch (*iterFind)
			{
				case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
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
																						   
																					   }
																					   catch (const std::exception &e)
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
																						   
																					   }
																					   catch (const std::exception &e)
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
																					   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterHalfBegin), 0, [&index, &num](auto &sum, auto const ch)
																					   {
																						   if (++index)
																							   num *= 10;
																						   return sum += (ch - '0')*num;
																					   });
																					   len += std::accumulate(std::make_reverse_iterator(iterHalfEnd), std::make_reverse_iterator(iterLenBegin), len, [&index, &num](auto &sum, auto const ch)
																					   {
																						   if (++index)
																							   num *= 10;
																						   return sum += (ch - '0')*num;
																					   });
																					   jump = true;
																				   }
																			   }
																			   else
																			   {
																				   index = -1;
																				   num = 1;
																				   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																				   {
																					   if (++index)
																						   num *= 10;
																					   return sum += (ch - '0')*num;
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
																				   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																				   {
																					   if (++index)
																						   num *= 10;
																					   return sum += (ch - '0')*num;
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
																								  
																							   }
																							   catch (const std::exception &e)
																							   {
																								   jumpNode = true;
																							   }
																						   }
																						   else
																						   {
																							   try
																							   {
																								  
																							   }
																							   catch (const std::exception &e)
																							   {
																								   jumpNode = true;
																							   }
																						   }
																					   }
																					   else
																					   {
																						   try
																						   {
																							 
																						   }
																						   catch (const std::exception &e)
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
																								  
																							   }
																							   catch (const std::exception &e)
																							   {
																								   jumpNode = true;
																							   }
																						   }
																						   else
																						   {
																							   try
																							   {
																								   
																							   }
																							   catch (const std::exception &e)
																							   {
																								   jumpNode = true;
																							   }
																						   }
																					   }
																					   else
																					   {
																						   try
																						   {
																							  
																						   }
																						   catch (const std::exception &e)
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



																				   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

																				   iterBegin = iterStrEnd + 2;
																			   }
																		   }

																		   continue;
																	   }
																	   break;


																	   case static_cast<int>(REDISPARSETYPE::INTERGER) :
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
																						   
																					   }
																					   catch (const std::exception &e)
																					   {
																						   jumpNode = true;
																					   }
																				   }
																				   else
																				   {
																					   try
																					   {
																						   
																					   }
																					   catch (const std::exception &e)
																					   {
																						   jumpNode = true;
																					   }
																				   }
																			   }
																			   else
																			   {
																				   try
																				   {
																					   
																				   }
																				   catch (const std::exception &e)
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


																		   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
																		   break;


																		   case static_cast<int>(REDISPARSETYPE::ERROR) :
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
																					   if (len <= m_outRangeMaxSize)
																					   {
																						   std::copy(errorBegin, iterHalfEnd, m_outRangeBuffer.get());
																						   std::copy(iterHalfBegin, errorEnd, m_outRangeBuffer.get() + (iterHalfEnd - errorBegin));

																						   try
																						   {
																							   
																						   }
																						   catch (const std::exception &e)
																						   {
																							   jumpNode = true;
																						   }
																					   }
																					   else
																					   {
																						   try
																						   {
																							   
																						   }
																						   catch (const std::exception &e)
																						   {
																							   jumpNode = true;
																						   }
																					   }
																				   }
																				   else
																				   {
																					   len = errorEnd - errorBegin;

																					   try
																					   {
																						  
																					   }
																					   catch (const std::exception &e)
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

																			   case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
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
																							   std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
																							   std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

																							   try
																							   {
																								  
																							   }
																							   catch (const std::exception &e)
																							   {
																								   jumpNode = true;
																							   }
																						   }
																						   else
																						   {
																							   try
																							   {
																								   
																							   }
																							   catch (const std::exception &e)
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
																						   catch (const std::exception &e)
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



																				   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
																				   break;

																				   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
																				   case static_cast<int>(REDISPARSETYPE::ARRAY) :
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
																						   arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto &sum, auto const ch)
																						   {
																							   if (++index)
																								   num *= 10;
																							   return sum += (ch - '0')*num;
																						   });
																					   }
																					   else
																					   {
																						   if (arrayNumBegin < iterHalfEnd)
																						   {
																							   arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(iterHalfBegin), 0, [&index, &num](auto &sum, auto const ch)
																							   {
																								   if (++index)
																									   num *= 10;
																								   return sum += (ch - '0')*num;
																							   });
																							   arrayNum += std::accumulate(std::make_reverse_iterator(iterHalfEnd), std::make_reverse_iterator(arrayNumBegin), arrayNum, [&index, &num](auto &sum, auto const ch)
																							   {
																								   if (++index)
																									   num *= 10;
																								   return sum += (ch - '0')*num;
																							   });
																						   }
																						   else
																						   {
																							   arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto &sum, auto const ch)
																							   {
																								   if (++index)
																									   num *= 10;
																								   return sum += (ch - '0')*num;
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
																									  
																								   }
																								   catch (const std::exception &e)
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
																							   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																							   {
																								   if (++index)
																									   num *= 10;
																								   return sum += (ch - '0')*num;
																							   });
																						   }
																						   else
																						   {
																							   if (iterLenBegin < iterHalfEnd)
																							   {
																								   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterHalfBegin), 0, [&index, &num](auto &sum, auto const ch)
																								   {
																									   if (++index)
																										   num *= 10;
																									   return sum += (ch - '0')*num;
																								   });
																								   len += std::accumulate(std::make_reverse_iterator(iterHalfEnd), std::make_reverse_iterator(iterLenBegin), len, [&index, &num](auto &sum, auto const ch)
																								   {
																									   if (++index)
																										   num *= 10;
																									   return sum += (ch - '0')*num;
																								   });
																							   }
																							   else
																							   {
																								   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																								   {
																									   if (++index)
																										   num *= 10;
																									   return sum += (ch - '0')*num;
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
																										  
																									   }
																									   catch (const std::exception &e)
																									   {
																										   jumpNode = true;
																									   }
																								   }
																								   else
																								   {
																									   try
																									   {
																										   
																									   }
																									   catch (const std::exception &e)
																									   {
																										   jumpNode = true;
																									   }
																								   }
																							   }
																							   else
																							   {
																								   try
																								   {
																									   
																								   }
																								   catch (const std::exception &e)
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
																						 
																						   try
																						   {
																							  
																						   }
																						   catch (const std::exception &e)
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
			switch (*iterFind)
			{
				case static_cast<int>(REDISPARSETYPE::LOT_SIZE_STRING) :
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
																					   
																				   }
																				   catch (const std::exception &e)
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
																			   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																			   {
																				   if (++index)
																					   num *= 10;
																				   return sum += (ch - '0')*num;
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
																					   
																				   }
																				   catch (const std::exception &e)
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

																	   case static_cast<int>(REDISPARSETYPE::INTERGER) :
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
																															  
																														   }
																														   catch (const std::exception &e)
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

																													   case static_cast<int>(REDISPARSETYPE::ERROR) :
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
																															   catch (const std::exception &e)
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

																														   case static_cast<int>(REDISPARSETYPE::SIMPLE_STRING) :
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
																																   catch (const std::exception &e)
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


																															   case static_cast<int>(REDISPARSETYPE::ARRAY) :
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
																																   arrayNum = std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto &sum, auto const ch)
																																   {
																																	   if (++index)
																																		   num *= 10;
																																	   return sum += (ch - '0')*num;
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
																																				   
																																			   }
																																			   catch (const std::exception &e)
																																			   {
																																				   jumpNode = true;
																																			   }
																																		   }

																																		   //////////////////////////////////////////////////////////////////////////////////////
																																		   continue;
																																	   }

																																	   if (++iterFind == iterEnd)
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
																																	   len = std::accumulate(std::make_reverse_iterator(iterLenEnd), std::make_reverse_iterator(iterLenBegin), 0, [&index, &num](auto &sum, auto const ch)
																																	   {
																																		   if (++index)
																																			   num *= 10;
																																		   return sum += (ch - '0')*num;
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
																																			   
																																		   }
																																		   catch (const std::exception &e)
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
																																	  
																																	   try
																																	   {
																																		 
																																	   }
																																	   catch (const std::exception &e)
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





//设置一个while循环来判断
//加锁首先直接尝试取commandSize个元素，如果队列为空，则退出，设置处理状态为否
//如果获取到元素，则遍历计算需要的空间为多少
//尝试分配空间，分配失败则continue处理
//成功则copy组合，进入发送命令阶段循环
void MULTIREDISWRITE::readyMessage()
{

	//先将需要用到的关键变量复位，再尝试生成新的请求消息
	std::shared_ptr<redisWriteTypeSW> redisRequest;
	std::vector<std::string_view>::const_iterator souceBegin, souceEnd;
	std::vector<unsigned int>::const_iterator lenBegin, lenEnd;
	char *messageIter{};

	//计算本次所需要的空间大小
	int totalLen{}, everyLen{}, index{}, divisor{ 10 }, temp{}, thisStrLen{};

	while (true)
	{
		m_waitMessageListBegin = m_waitMessageList.get();
		m_waitMessageListEnd = m_waitMessageList.get() + m_commandMaxSize;


		//从m_messageList中获取元素到m_waitMessageList中
		m_messageList.lock();
		std::forward_list<std::shared_ptr<redisWriteTypeSW>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.unlock();
			m_mutex.lock();
			m_queryStatus = false;
			m_mutex.unlock();
			m_charMemoryPool.prepare();
			break;
		}



		do
		{
			*m_waitMessageListBegin++ = *flist.begin();
			flist.pop_front();
		} while (!flist.empty() && m_waitMessageListBegin != m_waitMessageListEnd);
		m_messageList.unlock();


		//尝试计算总命令个数  命令字符串所需要总长度,不用检查非空问题
		m_waitMessageListEnd = m_waitMessageListBegin;
		m_waitMessageListBegin = m_waitMessageList.get();
		totalLen = 0;

		do
		{
			redisRequest = *m_waitMessageListBegin;

			std::vector<std::string_view> &sourceVec = std::get<0>(*redisRequest).get();
			std::vector<unsigned int> &lenVec = std::get<2>(*redisRequest).get();
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
			catch (const std::exception &e)
			{
				m_messageBufferMaxSize = 0;

				m_waitMessageListBegin = m_waitMessageList.get();
				do
				{
					redisRequest = *m_waitMessageListBegin;

				} while (++m_waitMessageListBegin != m_waitMessageListEnd);
				continue;
			}
		}

		m_messageBufferNowSize = totalLen;



		/////////生成请求命令字符串

		m_waitMessageListBegin = m_waitMessageList.get();
		messageIter = m_messageBuffer.get();
		do
		{
			redisRequest = *m_waitMessageListBegin;

			std::vector<std::string_view> &sourceVec = std::get<0>(*redisRequest).get();
			std::vector<unsigned int> &lenVec = std::get<2>(*redisRequest).get();
			souceBegin = sourceVec.cbegin(), souceEnd = sourceVec.cend();
			lenBegin = lenVec.cbegin(), lenEnd = lenVec.cend();

			souceBegin = sourceVec.cbegin(), lenBegin = lenVec.cbegin();



			do
			{
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
			} while (++lenBegin != lenEnd);

			//拷贝完毕之后可以通知回收，减少计数
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);



		////   准备发送

		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		redisRequest = *m_waitMessageListBegin;

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(*redisRequest);

		m_commandCurrentSize = 0;
		//////////////////////////////////////////////////////
		//进入发送函数

		query();

		break;
	}
}
