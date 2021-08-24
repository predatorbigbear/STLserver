#include "multiRedisRead.h"

MULTIREDISREAD::MULTIREDISREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, const unsigned int redisPort,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
	:m_redisIP(redisIP), m_redisPort(redisPort), m_ioc(ioc), m_unlockFun(unlockFun), m_receiveBufferMaxSize(memorySize),m_outRangeMaxSize(outRangeMaxSize),
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
		if(!outRangeMaxSize)
			throw std::runtime_error("outRangeMaxSize is invaild");
		if(!commandSize)
			throw std::runtime_error("commandSize is invaild");

		m_timer.reset(new boost::asio::steady_timer(*m_ioc));
		m_receiveBuffer.reset(new char[memorySize]);


		m_outRangeBuffer.reset(new char[m_outRangeMaxSize]);


		m_waitMessageList.reset(new std::shared_ptr<redisResultTypeSW>[m_commandMaxSize]);
		m_waitMessageListMaxSize = m_commandMaxSize;

		firstConnect();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
}


/*
	ִ������string_view��
	ִ���������
	ÿ������Ĵ���������������redis RESP����ƴ�ӣ�
	��ȡ�������  ����Ϊ����һЩ����������ܲ�һ���н�����أ�

	���ؽ��string_view
	ÿ������Ĵ������

	�ص�����
	*/

//�������������ж��Ƿ�����redis�������ɹ���
//���û�����ӣ�����ֱ�ӷ��ش���
//���ӳɹ�������£���������Ƿ����Ҫ��

//�Ƚ��в��뵽m_waitMessageList��


//   using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
//     std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
//     std::function<void(bool, enum ERRORMESSAGE) >> ;

bool MULTIREDISREAD::insertRedisRequest(std::shared_ptr<redisResultTypeSW> redisRequest)
{
	if (!redisRequest || std::get<0>(*redisRequest).get().empty() || !std::get<1>(*redisRequest) || std::get<1>(*redisRequest)> m_commandMaxSize
		|| !std::get<3>(*redisRequest) || std::get<1>(*redisRequest) != std::get<2>(*redisRequest).get().size() ||
		std::accumulate(std::get<2>(*redisRequest).get().cbegin(), std::get<2>(*redisRequest).get().cend(), 0) != std::get<0>(*redisRequest).get().size()
		)
		return false;
	m_mutex.lock();
	if (!m_connect)
	{
		m_mutex.unlock();
		return false;
	}
	if (!m_queryStatus)
	{
		m_queryStatus=true;
		m_mutex.unlock();

		//////////////////////////////////////////////////////////////////////

		std::vector<std::string_view> &sourceVec = std::get<0>(*redisRequest).get();
		std::vector<unsigned int> &lenVec = std::get<2>(*redisRequest).get();
		std::vector<std::string_view>::const_iterator souceBegin{ sourceVec.cbegin() }, souceEnd{ sourceVec.cend() };
		std::vector<unsigned int>::const_iterator lenBegin{ lenVec.cbegin() }, lenEnd{ lenVec.cend() };
		char *messageIter{};

		//���㱾������Ҫ�Ŀռ��С
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
				m_queryStatus=false;
				m_mutex.unlock();
				return false;
			}
		}

		m_messageBufferNowSize = totalLen;


		//copy���ɵ������ַ���buffer��

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


		m_sendMessage = m_messageBuffer.get(), m_sendLen =  m_messageBufferNowSize;

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
		//���뷢�ͺ���

		query();

		return true;
	}
	else
	{
	m_mutex.unlock();
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




bool MULTIREDISREAD::readyEndPoint()
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



bool MULTIREDISREAD::readySocket()
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



void MULTIREDISREAD::firstConnect()
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
			std::cout << "connect multi RedisRead success\n";
			setConnectSuccess();
			(*m_unlockFun)();
		}
	});
}




void MULTIREDISREAD::setConnectSuccess()
{
	m_mutex.lock();
	m_connect = true;
	m_mutex.unlock();
}




void MULTIREDISREAD::setConnectFail()
{
	m_mutex.lock();
	m_connect = false;
	m_mutex.unlock();
}




void MULTIREDISREAD::query()
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



//���ճɹ��Ļ����봦����
void MULTIREDISREAD::receive()
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




void MULTIREDISREAD::resetReceiveBuffer()
{
	m_receiveBufferNowSize = m_lastPrasePos = 0;
}



//��󷵻ؽ�������ʱ���жϵ�ǰ�ڵ���������������ǵ�������Ļ�����ô����ϸ�����ַ��ؾ�����Ϣ
//��֮Ӧ�÷���һ�����ϲ�ѯ�ɹ���־

//�����ж�m_jumpNode״̬���������m_jumpNode״̬����ô��ȡ��Ҫ�����Ľ��������ʼ�ۻ�����
//���m_jumpNode״̬Ϊ�����Ļ�����ô��ȡ���ڵ�������������ռ���������������Ϊ�ڵ�ڶ�λ���ռ����������֮�󷵻�
//�������飬���ȴ洢����ʱbuffer�У���ȡ��Ϻ�һ���Դ洢���ڵ�vector������У�
//����֮���ж��Ƿ��ߵ�end�ڵ㣬�ǵĻ���ȡ��ϣ�����true

//��������
//PS������return�ĵط�����֮ǰ��Ҫ����ʱ������ֵ����ص����Ա����
// ��ʱ���������ٶȿ죬����Ҫ����ʹ������һ��ʼ������ʱ�����������Ա������ʼ����������˳�ʱ��ֵ�����ȥ
bool MULTIREDISREAD::prase(const int size)
{
	const char *iterBegin{ }, *iterEnd{ m_receiveBuffer.get() + m_receiveBufferNowSize }, *iterHalfBegin{ m_receiveBuffer.get() },
		*iterHalfEnd{ m_receiveBuffer.get() + m_receiveBufferMaxSize }, *iterFind{},        // ��ʼ ����  Ѱ�ҽڵ�
		*iterLenBegin{}, *iterLenEnd{},               //���Ƚڵ�
		*iterStrBegin{}, *iterStrEnd{},               //����SIMPLE_STRING��βλ��
		*errorBegin{}, *errorEnd{},
		*arrayNumBegin{}, *arrayNumEnd{};

	// m_lastPrasePos    m_jumpNode  m_waitMessageListBegin   m_commandTotalSize    m_commandCurrentSize����ʱ��Ҫͬ������

	bool jumpNode{ m_jumpNode }, jump{false};  //�Ƿ��Ѿ������ػ����
	std::shared_ptr<redisResultTypeSW> *waitMessageListBegin = m_waitMessageListBegin ;
	std::shared_ptr<redisResultTypeSW> *waitMessageListEnd= m_waitMessageListEnd;
	unsigned int commandTotalSize{ m_commandTotalSize }, commandCurrentSize{ m_commandCurrentSize };

	// debug  ////////////

	/////////////////////////////


	unsigned int len{};     //�����ַ�������ʹ��
	int index{}, num{}, arrayNum{}, arrayIndex{};


	if (m_lastPrasePos == m_receiveBufferMaxSize)
		iterBegin = m_receiveBuffer.get();
	else
		iterBegin = m_receiveBuffer.get() + m_lastPrasePos;


	//��������û�лػ����
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
							//���ν��������  -1�����������

							if (!jumpNode)
							{
								try
								{
									std::get<5>(**waitMessageListBegin).get().emplace_back(0);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}


							//ֻ���𷵻سɹ����
							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{



								}
								else
								{
									if(commandTotalSize==1)
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::NO_KEY);
									else
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_MULTI_OK);
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

							//���ν����ȡ�ɹ�����������д���

							//////////////////////////////////////////////////////////////////////////////////////////////////////////////
							if (!jumpNode)
							{
								try
								{
									std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, len));
									std::get<5>(**waitMessageListBegin).get().emplace_back(1);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{
					


								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_SINGLE_OK);
									else
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_MULTI_OK);
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

						//���iterLenBegin    iterLenEnd
						////////////////////////////////////////////////////////////////////////
						if (!jumpNode)
						{
							try
							{
								std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterLenBegin, iterLenEnd- iterLenBegin));
								std::get<5>(**waitMessageListBegin).get().emplace_back(1);
							}
							catch (const std::exception &e)
							{
								jumpNode = true;
							}
						}


						if (++commandCurrentSize == commandTotalSize)
						{
							//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
							if (jumpNode)
							{



							}
							else
							{
								std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
							//���ش�����Ϣ���
							////////////////////////////////////////////////////////////////////////////////////////////

							if (!jumpNode)
							{
								try
								{
									std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(errorBegin, errorEnd - errorBegin));
									std::get<5>(**waitMessageListBegin).get().emplace_back(1);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{



								}
								else
								{
									std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
								//���ش�����Ϣ���
								//////////////////////////////////////////////////////////////////////////////////////////////////////////

								if (!jumpNode)
								{
									try
									{
										std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, iterStrEnd - iterStrBegin));
										std::get<5>(**waitMessageListBegin).get().emplace_back(1);
									}
									catch (const std::exception &e)
									{
										jumpNode = true;
									}
								}


								if (++commandCurrentSize == commandTotalSize)
								{
									//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
									if (jumpNode)
									{



									}
									else
									{
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
									arrayNum= std::accumulate(std::make_reverse_iterator(arrayNumEnd), std::make_reverse_iterator(arrayNumBegin), 0, [&index, &num](auto &sum, auto const ch)
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
											///////////////////////   ����û�н����������ô���رȽϷ���  -1////////////////////

											if (!jumpNode)
											{
												try
												{
													m_arrayResult.emplace_back(std::string_view(nullptr, 0));
												}
												catch (const std::exception &e)
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

										//////////////////////////////////////////////  ÿ�ε���Ч���

										if (!jumpNode)
										{
											try
											{
												m_arrayResult.emplace_back(std::string_view(iterStrBegin, len));
											}
											catch (const std::exception &e)
											{
												jumpNode = true;
											}
										}



										//////////////////////////////////////////////////////////////
									}
									iterBegin = iterFind;

									///////////////////////////////////////////////////////////////////////// ������в�������


									if (!jumpNode)
									{
										std::vector<std::string_view>&vec= std::get<4>(**waitMessageListBegin).get() ;
										try
										{
											for(auto const sw: m_arrayResult)
												vec.emplace_back(sw);
											std::get<5>(**waitMessageListBegin).get().emplace_back(m_arrayResult.size());
										}
										catch (const std::exception &e)
										{
											jumpNode = true;
										}
									}


									if (++commandCurrentSize == commandTotalSize)
									{
										//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
										if (jumpNode)
										{



										}
										else
										{
											std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
	else      //�������ݻػ����
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
							//���ν��������  -1�����������

							///////////////////////////////////////////////////////


							if (!jumpNode)
							{
								try
								{
									std::get<5>(**waitMessageListBegin).get().emplace_back(0);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::NO_KEY);
									else
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_MULTI_OK);
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
							//���ν��������  -1�����������
							//////////////////////////////////////////

							if (!jumpNode)
							{
								try
								{
									std::get<5>(**waitMessageListBegin).get().emplace_back(0);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::NO_KEY);
									else
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_MULTI_OK);
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

						//���ν����ȡ�ɹ�����������д���ע���жϻػ����    iterStrBegin      iterStrEnd

						/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


						
						if (!jumpNode)
						{
							//��Ҫ�ػ�������
							if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
							{
								//��Ϊ��һ�׻��Ʊ��ʽ�����Σ�յ�ָ��֮�ϣ��������Ϊ�˰�ȫ�����������ݻػ������Ҫ���������ʱ���жϳ����Ƿ���m_outRangeMaxSize֮�ڣ������������Ը�ж�Ϊû�С�
							//���ʹ���߾��ÿ��Գ��ܺ������ô���������ﻻ�ɶ�̬�����ڴ���п���������Ҫ��סһ�㣺һ�����Ʋ��ã���ʹ����ζ�̬�����ڴ�ʱ�ڶ��ν�����Ϣ����ʱһ���������·��䣬
							//���ܻᴥ����ָ���쳣 ����ֱ�ӱ���
								if (len <= m_outRangeMaxSize)
								{
									std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
									std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

									try
									{
										std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
										std::get<5>(**waitMessageListBegin).get().emplace_back(1);
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
										std::get<5>(**waitMessageListBegin).get().emplace_back(0);
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
									std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, len));
									std::get<5>(**waitMessageListBegin).get().emplace_back(1);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}
						}


						if (++commandCurrentSize == commandTotalSize)
						{
							//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
							if (jumpNode)
							{



							}
							else
							{
								if (commandTotalSize == 1)
									std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_SINGLE_OK);
								else
									std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_MULTI_OK);
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

							//���ν����ȡ�ɹ�����������д���ע���жϻػ����

							/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


							if (!jumpNode)
							{
								//��Ҫ�ػ�������
								if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
								{
									//��Ϊ��һ�׻��Ʊ��ʽ�����Σ�յ�ָ��֮�ϣ��������Ϊ�˰�ȫ�����������ݻػ������Ҫ���������ʱ���жϳ����Ƿ���m_outRangeMaxSize֮�ڣ������������Ը�ж�Ϊû�С�
								//���ʹ���߾��ÿ��Գ��ܺ������ô���������ﻻ�ɶ�̬�����ڴ���п���������Ҫ��סһ�㣺һ�����Ʋ��ã���ʹ����ζ�̬�����ڴ�ʱ�ڶ��ν�����Ϣ����ʱһ���������·��䣬
								//���ܻᴥ����ָ���쳣 ����ֱ�ӱ���
									if (len <= m_outRangeMaxSize)
									{
										std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
										std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

										try
										{
											std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
											std::get<5>(**waitMessageListBegin).get().emplace_back(1);
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
											std::get<5>(**waitMessageListBegin).get().emplace_back(0);
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
										std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, len));
										std::get<5>(**waitMessageListBegin).get().emplace_back(1);
									}
									catch (const std::exception &e)
									{
										jumpNode = true;
									}
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{



								}
								else
								{
									if (commandTotalSize == 1)
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_SINGLE_OK);
									else
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::REDIS_MULTI_OK);
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


					//���iterLenBegin    iterLenEnd    ע��ػ��������

					/////////////////////////////////////////////////////////////////////////////////////////


					if (!jumpNode)
					{
						//��Ҫ�ػ�������
						if (iterLenBegin < iterHalfEnd && iterLenEnd>iterHalfBegin)
						{
							//��Ϊ��һ�׻��Ʊ��ʽ�����Σ�յ�ָ��֮�ϣ��������Ϊ�˰�ȫ�����������ݻػ������Ҫ���������ʱ���жϳ����Ƿ���m_outRangeMaxSize֮�ڣ������������Ը�ж�Ϊû�С�
						//���ʹ���߾��ÿ��Գ��ܺ������ô���������ﻻ�ɶ�̬�����ڴ���п���������Ҫ��סһ�㣺һ�����Ʋ��ã���ʹ����ζ�̬�����ڴ�ʱ�ڶ��ν�����Ϣ����ʱһ���������·��䣬
						//���ܻᴥ����ָ���쳣 ����ֱ�ӱ���
							if (len <= m_outRangeMaxSize)
							{
								std::copy(iterLenBegin, iterHalfEnd, m_outRangeBuffer.get());
								std::copy(iterHalfBegin, iterLenEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterLenBegin));

								try
								{
									std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
									std::get<5>(**waitMessageListBegin).get().emplace_back(1);
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
									std::get<5>(**waitMessageListBegin).get().emplace_back(0);
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
								std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterLenBegin, len));
								std::get<5>(**waitMessageListBegin).get().emplace_back(1);
							}
							catch (const std::exception &e)
							{
								jumpNode = true;
							}
						}
					}


					if (++commandCurrentSize == commandTotalSize)
					{
						//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
						if (jumpNode)
						{



						}
						else
						{
							std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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

						//���ش�����Ϣ���   errorBegin      errorEnd ע��ػ��������
						///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

						if (!jumpNode)
						{
							//��Ҫ�ػ�������
							if (errorBegin < iterHalfEnd && errorEnd>iterHalfBegin)
							{
								len = iterHalfEnd - errorBegin + errorEnd - iterHalfBegin;
								//��Ϊ��һ�׻��Ʊ��ʽ�����Σ�յ�ָ��֮�ϣ��������Ϊ�˰�ȫ�����������ݻػ������Ҫ���������ʱ���жϳ����Ƿ���m_outRangeMaxSize֮�ڣ������������Ը�ж�Ϊû�С�
							//���ʹ���߾��ÿ��Գ��ܺ������ô���������ﻻ�ɶ�̬�����ڴ���п���������Ҫ��סһ�㣺һ�����Ʋ��ã���ʹ����ζ�̬�����ڴ�ʱ�ڶ��ν�����Ϣ����ʱһ���������·��䣬
							//���ܻᴥ����ָ���쳣 ����ֱ�ӱ���
								if (len <= m_outRangeMaxSize)
								{
									std::copy(errorBegin, iterHalfEnd, m_outRangeBuffer.get());
									std::copy(iterHalfBegin, errorEnd, m_outRangeBuffer.get() + (iterHalfEnd - errorBegin));

									try
									{
										std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
										std::get<5>(**waitMessageListBegin).get().emplace_back(1);
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
										std::get<5>(**waitMessageListBegin).get().emplace_back(0);
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
									std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(errorBegin, len));
									std::get<5>(**waitMessageListBegin).get().emplace_back(1);
								}
								catch (const std::exception &e)
								{
									jumpNode = true;
								}
							}
						}


						if (++commandCurrentSize == commandTotalSize)
						{
							//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
							if (jumpNode)
							{



							}
							else
							{
								std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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

							//������Ϣ���   iterStrBegin      iterStrEnd ע��ػ��������   ��Ҫ����len
							////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

							if (!jumpNode)
							{
								//��Ҫ�ػ�������
								if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
								{
									len = iterHalfEnd - iterStrBegin + iterStrEnd - iterHalfBegin;
									//��Ϊ��һ�׻��Ʊ��ʽ�����Σ�յ�ָ��֮�ϣ��������Ϊ�˰�ȫ�����������ݻػ������Ҫ���������ʱ���жϳ����Ƿ���m_outRangeMaxSize֮�ڣ������������Ը�ж�Ϊû�С�
								//���ʹ���߾��ÿ��Գ��ܺ������ô���������ﻻ�ɶ�̬�����ڴ���п���������Ҫ��סһ�㣺һ�����Ʋ��ã���ʹ����ζ�̬�����ڴ�ʱ�ڶ��ν�����Ϣ����ʱһ���������·��䣬
								//���ܻᴥ����ָ���쳣 ����ֱ�ӱ���
									if (len <= m_outRangeMaxSize)
									{
										std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
										std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

										try
										{
											std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(m_outRangeBuffer.get(), len));
											std::get<5>(**waitMessageListBegin).get().emplace_back(1);
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
											std::get<5>(**waitMessageListBegin).get().emplace_back(0);
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
										std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, len));
										std::get<5>(**waitMessageListBegin).get().emplace_back(1);
									}
									catch (const std::exception &e)
									{
										jumpNode = true;
									}
								}
							}


							if (++commandCurrentSize == commandTotalSize)
							{
								//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
								if (jumpNode)
								{



								}
								else
								{
									std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
											if(iterHalfEnd - iterFind>=4)
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

										///////////////////////   ����û�н����������ô���رȽϷ���  -1  //////////////////////////////////////////////////////

										if (!jumpNode)
										{
											try
											{
												m_arrayResult.emplace_back(std::string_view(nullptr, 0));
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
										if(iterHalfEnd - iterLenEnd >=2)
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


									//////////////////////////////////////////////  ÿ�ε���Ч�����ע��ػ�����    iterStrBegin      iterStrEnd   len////////////////////


									if (!jumpNode)
									{
										//��Ҫ�ػ�������
										if (iterStrBegin < iterHalfEnd && iterStrEnd>iterHalfBegin)
										{
											//��Ϊ��һ�׻��Ʊ��ʽ�����Σ�յ�ָ��֮�ϣ��������Ϊ�˰�ȫ�����������ݻػ������Ҫ���������ʱ���жϳ����Ƿ���m_outRangeMaxSize֮�ڣ������������Ը�ж�Ϊû�С�
										//���ʹ���߾��ÿ��Գ��ܺ������ô���������ﻻ�ɶ�̬�����ڴ���п���������Ҫ��סһ�㣺һ�����Ʋ��ã���ʹ����ζ�̬�����ڴ�ʱ�ڶ��ν�����Ϣ����ʱһ���������·��䣬
										//���ܻᴥ����ָ���쳣 ����ֱ�ӱ���
											if (len <= m_outRangeMaxSize)
											{
												std::copy(iterStrBegin, iterHalfEnd, m_outRangeBuffer.get());
												std::copy(iterHalfBegin, iterStrEnd, m_outRangeBuffer.get() + (iterHalfEnd - iterStrBegin));

												try
												{
													m_arrayResult.emplace_back(std::string_view(m_outRangeBuffer.get(), len));
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
													m_arrayResult.emplace_back(std::string_view(nullptr,0));
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
												m_arrayResult.emplace_back(std::string_view(iterStrBegin, len));
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
									std::vector<std::string_view>&vec = std::get<4>(**waitMessageListBegin).get();
									try
									{
										for (auto const sw : m_arrayResult)
											vec.emplace_back(sw);
										std::get<5>(**waitMessageListBegin).get().emplace_back(m_arrayResult.size());
									}
									catch (const std::exception &e)
									{
										jumpNode = true;
									}
								}


								if (++commandCurrentSize == commandTotalSize)
								{
									//����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
									if (jumpNode)
									{



									}
									else
									{
										std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
																			   //���ν��������  -1�����������

																			   if (!jumpNode)
																			   {
																				   try
																				   {
																					   std::get<5>(**waitMessageListBegin).get().emplace_back(0);
																				   }
																				   catch (const std::exception &e)
																				   {
																					   jumpNode = true;
																				   }
																			   }


																			   if (++commandCurrentSize == commandTotalSize)
																			   {
																				   //����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
																				   if (jumpNode)
																				   {



																				   }
																				   else
																				   {
																					   std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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

																			   //���ν����ȡ�ɹ�����������д���

																			   //////////////////////////////////////////////////////////////////////////////////////////////////////////////
																			   if (!jumpNode)
																			   {
																				   try
																				   {
																					   std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, len));
																					   std::get<5>(**waitMessageListBegin).get().emplace_back(1);
																				   }
																				   catch (const std::exception &e)
																				   {
																					   jumpNode = true;
																				   }
																			   }


																			   if (++commandCurrentSize == commandTotalSize)
																			   {
																				   //����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
																				   if (jumpNode)
																				   {



																				   }
																				   else
																				   {
																					   std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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

																													   //���iterLenBegin    iterLenEnd
																													   ////////////////////////////////////////////////////////////////////////
																													   if (!jumpNode)
																													   {
																														   try
																														   {
																															   std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterLenBegin, iterLenEnd - iterLenBegin));
																															   std::get<5>(**waitMessageListBegin).get().emplace_back(1);
																														   }
																														   catch (const std::exception &e)
																														   {
																															   jumpNode = true;
																														   }
																													   }


																													   if (++commandCurrentSize == commandTotalSize)
																													   {
																														   //����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
																														   if (jumpNode)
																														   {



																														   }
																														   else
																														   {
																															   std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
																														   //���ش�����Ϣ���
																														   ////////////////////////////////////////////////////////////////////////////////////////////

																														   if (!jumpNode)
																														   {
																															   try
																															   {
																																   std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(errorBegin, errorEnd - errorBegin));
																																   std::get<5>(**waitMessageListBegin).get().emplace_back(1);
																															   }
																															   catch (const std::exception &e)
																															   {
																																   jumpNode = true;
																															   }
																														   }


																														   if (++commandCurrentSize == commandTotalSize)
																														   {
																															   //����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
																															   if (jumpNode)
																															   {



																															   }
																															   else
																															   {
																																   std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
																															   //���ش�����Ϣ���
																															   //////////////////////////////////////////////////////////////////////////////////////////////////////////

																															   if (!jumpNode)
																															   {
																																   try
																																   {
																																	   std::get<4>(**waitMessageListBegin).get().emplace_back(std::string_view(iterStrBegin, iterStrEnd - iterStrBegin));
																																	   std::get<5>(**waitMessageListBegin).get().emplace_back(1);
																																   }
																																   catch (const std::exception &e)
																																   {
																																	   jumpNode = true;
																																   }
																															   }


																															   if (++commandCurrentSize == commandTotalSize)
																															   {
																																   //����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
																																   if (jumpNode)
																																   {



																																   }
																																   else
																																   {
																																	   std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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
																																		   ///////////////////////   ����û�н����������ô���رȽϷ���  -1////////////////////

																																		   if (!jumpNode)
																																		   {
																																			   try
																																			   {
																																				   m_arrayResult.emplace_back(std::string_view(nullptr, 0));
																																			   }
																																			   catch (const std::exception &e)
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

																																	   //////////////////////////////////////////////  ÿ�ε���Ч���

																																	   if (!jumpNode)
																																	   {
																																		   try
																																		   {
																																			   m_arrayResult.emplace_back(std::string_view(iterStrBegin, len));
																																		   }
																																		   catch (const std::exception &e)
																																		   {
																																			   jumpNode = true;
																																		   }
																																	   }



																																	   //////////////////////////////////////////////////////////////
																																   }
																																   iterBegin = iterFind;

																																   ///////////////////////////////////////////////////////////////////////// ������в�������


																																   if (!jumpNode)
																																   {
																																	   std::vector<std::string_view>&vec = std::get<4>(**waitMessageListBegin).get();
																																	   try
																																	   {
																																		   for (auto const sw : m_arrayResult)
																																			   vec.emplace_back(sw);
																																		   std::get<5>(**waitMessageListBegin).get().emplace_back(m_arrayResult.size());
																																	   }
																																	   catch (const std::exception &e)
																																	   {
																																		   jumpNode = true;
																																	   }
																																   }


																																   if (++commandCurrentSize == commandTotalSize)
																																   {
																																	   //����jumpNode״̬ ���ûص��������ر��γɹ���ʧ�ܽ��
																																	   if (jumpNode)
																																	   {



																																	   }
																																	   else
																																	   {
																																		   std::get<6>(**waitMessageListBegin)(true, ERRORMESSAGE::OK);
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

	//  ����Ѿ��������������У��򷵻�true,	
	m_lastPrasePos = iterBegin - m_receiveBuffer.get();
	m_jumpNode = jumpNode;
	m_waitMessageListBegin = waitMessageListBegin;
	m_commandTotalSize = commandTotalSize;
	m_commandCurrentSize = commandCurrentSize;

	if (m_waitMessageListBegin == m_waitMessageListEnd)
		return true;
	return false;
}




void MULTIREDISREAD::handlelRead(const boost::system::error_code &err, const std::size_t size)
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

			//ÿ�ν��н���֮ǰ��Ҫ���浱ǰ�����ڵ�λ�ã�ʧ��ʱ���бȶԣ��������buffer������������Ȼ����û�б仯���򽫵�ǰ������еĽڵ�ȫ�����ش���Ȼ����������У������µ�������Ϣ��
			//��Ϊ����㷨���ٵĹؼ����ڼ�������ԭbuffer����

			m_waitMessageListBeforeParse = m_waitMessageListBegin;

			if (!prase(size))
			{
				// 
				m_waitMessageListAfterParse = m_waitMessageListBegin;

				if (m_waitMessageListBeforeParse == m_waitMessageListAfterParse)
				{
					if (m_receiveBufferNowSize == m_lastPrasePos)
					{
						// ��m_waitMessageListBegin �� m_waitMessageListEnd ͳͳ���ؽ�������״̬

						std::shared_ptr<redisResultTypeSW> request;
						do
						{
							request = *m_waitMessageListBegin;

							std::get<6>(*request)(false, ERRORMESSAGE::REDIS_READY_QUERY_ERROR);
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
				// ȫ��������ȫ�ˣ��ж�m_waitMessageListBegin == m_waitMessageListEnd
				//����һ��whileѭ�����ж�
				//��������ֱ�ӳ���ȡcommandSize��Ԫ�أ��������Ϊ�գ����˳������ô���״̬Ϊ��
				//�����ȡ��Ԫ�أ������������Ҫ�Ŀռ�Ϊ����
				//���Է���ռ䣬����ʧ����continue����
				//�ɹ���copy��ϣ����뷢������׶�ѭ��

				
				readyMessage();
			}
		}
		else
		{
			receive();
		}
	}
}



//����һ��whileѭ�����ж�
//��������ֱ�ӳ���ȡcommandSize��Ԫ�أ��������Ϊ�գ����˳������ô���״̬Ϊ��
//�����ȡ��Ԫ�أ������������Ҫ�Ŀռ�Ϊ����
//���Է���ռ䣬����ʧ����continue����
//�ɹ���copy��ϣ����뷢������׶�ѭ��
void MULTIREDISREAD::readyMessage()
{

	//�Ƚ���Ҫ�õ��Ĺؼ�������λ���ٳ��������µ�������Ϣ
	std::shared_ptr<redisResultTypeSW> redisRequest;
	std::vector<std::string_view>::const_iterator souceBegin, souceEnd;
	std::vector<unsigned int>::const_iterator lenBegin, lenEnd;
	char *messageIter{};

	//���㱾������Ҫ�Ŀռ��С
	int totalLen{}, everyLen{}, index{}, divisor{ 10 }, temp{}, thisStrLen{};

	while (true)
	{
		m_waitMessageListBegin = m_waitMessageList.get();
		m_waitMessageListEnd = m_waitMessageList.get() + m_commandMaxSize;


		//��m_messageList�л�ȡԪ�ص�m_waitMessageList��
		m_messageList.getMutex().lock();
		std::forward_list<std::shared_ptr<redisResultTypeSW>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.getMutex().unlock();
			m_mutex.lock();
			m_queryStatus=false;
			m_mutex.unlock();
			break;
		}



		do
		{
			*m_waitMessageListBegin++ = *flist.begin();
			flist.pop_front();
		} while (!flist.empty() && m_waitMessageListBegin != m_waitMessageListEnd);
		m_messageList.getMutex().unlock();


		//���Լ������������  �����ַ�������Ҫ�ܳ���,���ü��ǿ�����
		m_waitMessageListEnd = m_waitMessageListBegin;
		m_waitMessageListBegin= m_waitMessageList.get();
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



		//�����ܳ�����ϣ����Խ��з���ռ�
		//����ʧ��ʱ��ǵñ����������ش���
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
					std::get<6>(*redisRequest)(false, ERRORMESSAGE::STD_BADALLOC);

				} while (++m_waitMessageListBegin != m_waitMessageListEnd);
				continue;
			}
		}

		m_messageBufferNowSize = totalLen;

		

		/////////�������������ַ���

		m_waitMessageListBegin= m_waitMessageList.get();
		messageIter= m_messageBuffer.get();
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
		}while (++m_waitMessageListBegin != m_waitMessageListEnd);


		
		////   ׼������

		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		redisRequest = *m_waitMessageListBegin;

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(*redisRequest);

		m_commandCurrentSize = 0;
		//////////////////////////////////////////////////////
		//���뷢�ͺ���

		query();

		break;
	}
}
