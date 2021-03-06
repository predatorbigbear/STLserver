#include "multiRedisWrite.h"

MULTIREDISWRITE::MULTIREDISWRITE(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, 
	const unsigned int redisPort, const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
	:m_redisIP(redisIP), m_redisPort(redisPort), m_ioc(ioc), m_unlockFun(unlockFun), m_receiveBufferMaxSize(memorySize), m_outRangeMaxSize(outRangeMaxSize),
	m_commandMaxSize(commandSize)
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
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
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

		//????????????????????????
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


		//copy????????????????buffer??

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

				copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
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

					copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
					messageIter += HTTPRESPONSE::halfNewLineLen;

					copy(souceBegin->cbegin(), souceBegin->cend(), messageIter);
					messageIter += souceBegin->size();

					copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
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
		//????????????

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
			char *buffer{ m_charMemoryPool.getMemory(bufferLen) };

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

		m_messageList.getMutex().lock();

		if (!m_messageList.unsafeInsert(redisRequest))
		{
			m_messageList.getMutex().unlock();
			return false;
		}

		m_messageList.getMutex().unlock();

		return true;
	}
}






bool MULTIREDISWRITE::readyEndPoint()
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

			//??????????????????????????????????????????????????????????????????buffer????????????????????????????????????????????????????????????????????????????????????????????????????
			//????????????????????????????????????buffer????

			m_waitMessageListBeforeParse = m_waitMessageListBegin;


			if (!prase(size))
			{
				// 
				m_waitMessageListAfterParse = m_waitMessageListBegin;

				if (m_waitMessageListBeforeParse == m_waitMessageListAfterParse)
				{
					if (m_receiveBufferNowSize == m_lastPrasePos)
					{
						// ??m_waitMessageListBegin ?? m_waitMessageListEnd ????????????????????

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
				// ????????????????????m_waitMessageListBegin == m_waitMessageListEnd
				//????????while??????????
				//??????????????????commandSize??????????????????????????????????????????????
				//??????????????????????????????????????????
				//????????????????????????continue????
				//??????copy??????????????????????????


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
		*iterHalfEnd{ m_receiveBuffer.get() + m_receiveBufferMaxSize }, *iterFind{},        // ???? ????  ????????
		*iterLenBegin{}, *iterLenEnd{},               //????????
		*iterStrBegin{}, *iterStrEnd{},               //????SIMPLE_STRING????????
		*errorBegin{}, *errorEnd{},
		*arrayNumBegin{}, *arrayNumEnd{};

	// m_lastPrasePos    m_jumpNode  m_waitMessageListBegin   m_commandTotalSize    m_commandCurrentSize??????????????????

	bool jumpNode{ m_jumpNode }, jump{ false };  //????????????????????
	std::shared_ptr<redisWriteTypeSW> *waitMessageListBegin = m_waitMessageListBegin;
	std::shared_ptr<redisWriteTypeSW> *waitMessageListEnd = m_waitMessageListEnd;
	unsigned int commandTotalSize{ m_commandTotalSize }, commandCurrentSize{ m_commandCurrentSize };

	// debug  ////////////

	/////////////////////////////


	unsigned int len{};     //??????????????????
	int index{}, num{}, arrayNum{}, arrayIndex{};


	if (m_lastPrasePos == m_receiveBufferMaxSize)
		iterBegin = m_receiveBuffer.get();
	else
		iterBegin = m_receiveBuffer.get() + m_lastPrasePos;


	//????????????????????
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
																			   //??????????????  -1????????????

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


																			   //??????????????????
																			   if (++commandCurrentSize == commandTotalSize)
																			   {
																				   //????jumpNode???? ??????????????????????????????????
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

																			   //????????????????????????????????

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
																				   //????jumpNode???? ??????????????????????????????????
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

																													   //????iterLenBegin    iterLenEnd
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
																														   //????jumpNode???? ??????????????????????????????????
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
																														   //????????????????
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
																															   //????jumpNode???? ??????????????????????????????????
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
																															   //????????????????
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
																																   //????jumpNode???? ??????????????????????????????????
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
																																		   ///////////////////////   ??????????????????????????????????  -1////////////////////

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

																																	   //////////////////////////////////////////////  ??????????????

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

																																   ///////////////////////////////////////////////////////////////////////// ????????????????


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
																																	   //????jumpNode???? ??????????????????????????????????
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
	else      //????????????????
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
																				   //??????????????  -1????????????

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
																					   //????jumpNode???? ??????????????????????????????????
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
																				   //??????????????  -1????????????
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
																					   //????jumpNode???? ??????????????????????????????????
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

																				   //??????????????????????????????????????????????????    iterStrBegin      iterStrEnd

																				   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



																				   if (!jumpNode)
																				   {
																					   //????????????????
																					   if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
																					   {
																						   //??????????????????????????????????????????????????????????????????????????????????????????????????????????????m_outRangeMaxSize????????????????????????????????
																					   //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
																					   //???????????????????? ????????????
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
																					   //????jumpNode???? ??????????????????????????????????
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

																				   //??????????????????????????????????????????????????

																				   /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


																				   if (!jumpNode)
																				   {
																					   //????????????????
																					   if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
																					   {
																						   //??????????????????????????????????????????????????????????????????????????????????????????????????????????????m_outRangeMaxSize????????????????????????????????
																					   //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
																					   //???????????????????? ????????????
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
																					   //????jumpNode???? ??????????????????????????????????
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


																		   //????iterLenBegin    iterLenEnd    ????????????????

																		   /////////////////////////////////////////////////////////////////////////////////////////


																		   if (!jumpNode)
																		   {
																			   //????????????????
																			   if (iterLenBegin < iterHalfEnd && iterLenEnd>iterHalfBegin)
																			   {
																				   //??????????????????????????????????????????????????????????????????????????????????????????????????????????????m_outRangeMaxSize????????????????????????????????
																			   //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
																			   //???????????????????? ????????????
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
																			   //????jumpNode???? ??????????????????????????????????
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

																			   //????????????????   errorBegin      errorEnd ????????????????
																			   ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

																			   if (!jumpNode)
																			   {
																				   //????????????????
																				   if (errorBegin < iterHalfEnd && errorEnd>iterHalfBegin)
																				   {
																					   len = iterHalfEnd - errorBegin + errorEnd - iterHalfBegin;
																					   //??????????????????????????????????????????????????????????????????????????????????????????????????????????????m_outRangeMaxSize????????????????????????????????
																				   //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
																				   //???????????????????? ????????????
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
																				   //????jumpNode???? ??????????????????????????????????
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

																				   //????????????   iterStrBegin      iterStrEnd ????????????????   ????????len
																				   ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

																				   if (!jumpNode)
																				   {
																					   //????????????????
																					   if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
																					   {
																						   len = iterHalfEnd - iterStrBegin + iterStrEnd - iterHalfBegin;
																						   //??????????????????????????????????????????????????????????????????????????????????????????????????????????????m_outRangeMaxSize????????????????????????????????
																					   //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
																					   //???????????????????? ????????????
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
																					   //????jumpNode???? ??????????????????????????????????
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

																							   ///////////////////////   ??????????????????????????????????  -1  //////////////////////////////////////////////////////

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


																						   //////////////////////////////////////////////  ????????????????????????????    iterStrBegin      iterStrEnd   len////////////////////


																						   if (!jumpNode)
																						   {
																							   //????????????????
																							   if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
																							   {
																								   //??????????????????????????????????????????????????????????????????????????????????????????????????????????????m_outRangeMaxSize????????????????????????????????
																							   //????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
																							   //???????????????????? ????????????
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
																						   //????jumpNode???? ??????????????????????????????????
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
																			   //??????????????  -1????????????

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
																				   //????jumpNode???? ??????????????????????????????????
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

																			   //????????????????????????????????

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
																				   //????jumpNode???? ??????????????????????????????????
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

																													   //????iterLenBegin    iterLenEnd
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
																														   //????jumpNode???? ??????????????????????????????????
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
																														   //????????????????
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
																															   //????jumpNode???? ??????????????????????????????????
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
																															   //????????????????
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
																																   //????jumpNode???? ??????????????????????????????????
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
																																		   ///////////////////////   ??????????????????????????????????  -1////////////////////

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

																																	   //////////////////////////////////////////////  ??????????????

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

																																   ///////////////////////////////////////////////////////////////////////// ????????????????


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
																																	   //????jumpNode???? ??????????????????????????????????
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

	//  ??????????????????????????????true,	
	m_lastPrasePos = iterBegin - m_receiveBuffer.get();
	m_jumpNode = jumpNode;
	m_waitMessageListBegin = waitMessageListBegin;
	m_commandTotalSize = commandTotalSize;
	m_commandCurrentSize = commandCurrentSize;

	if (m_waitMessageListBegin == m_waitMessageListEnd)
		return true;
	return false;
}





//????????while??????????
//??????????????????commandSize??????????????????????????????????????????????
//??????????????????????????????????????????
//????????????????????????continue????
//??????copy??????????????????????????
void MULTIREDISWRITE::readyMessage()
{

	//??????????????????????????????????????????????????
	std::shared_ptr<redisWriteTypeSW> redisRequest;
	std::vector<std::string_view>::const_iterator souceBegin, souceEnd;
	std::vector<unsigned int>::const_iterator lenBegin, lenEnd;
	char *messageIter{};

	//????????????????????????
	int totalLen{}, everyLen{}, index{}, divisor{ 10 }, temp{}, thisStrLen{};

	while (true)
	{
		m_waitMessageListBegin = m_waitMessageList.get();
		m_waitMessageListEnd = m_waitMessageList.get() + m_commandMaxSize;


		//??m_messageList????????????m_waitMessageList??
		m_messageList.getMutex().lock();
		std::forward_list<std::shared_ptr<redisWriteTypeSW>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.getMutex().unlock();
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
		m_messageList.getMutex().unlock();


		//??????????????????  ??????????????????????,????????????????
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



		//????????????????????????????????
		//??????????????????????????????????
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



		/////////??????????????????

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

					copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
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

						copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
						messageIter += HTTPRESPONSE::halfNewLineLen;

						copy(souceBegin->cbegin(), souceBegin->cend(), messageIter);
						messageIter += souceBegin->size();

						copy(HTTPRESPONSE::halfNewLine, HTTPRESPONSE::halfNewLine + HTTPRESPONSE::halfNewLineLen, messageIter);
						messageIter += HTTPRESPONSE::halfNewLineLen;

						++souceBegin;
					}
				}
			} while (++lenBegin != lenEnd);

			//??????????????????????????????????
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);



		////   ????????

		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		redisRequest = *m_waitMessageListBegin;

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(*redisRequest);

		m_commandCurrentSize = 0;
		//////////////////////////////////////////////////////
		//????????????

		query();

		break;
	}
}
