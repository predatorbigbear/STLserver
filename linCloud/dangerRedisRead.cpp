#include "dangerRedisRead.h"


//ÿһ�������Ӧ����Ҫ���Ƕ���
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
		m_queryStatus.store(true);       //ֱ�ӽ��봦����

		m_redisRequest = redisRequest;

		m_data = std::get<2>(*m_redisRequest);

		//���Բ鿴�Ƿ�������
		if (!readyQuery(std::get<0>(*m_redisRequest), std::get<1>(*m_redisRequest)))
		{
			if (!m_messageList.empty())
				tryCheckList();
			else
				m_queryStatus.store(false);

			return false;
		}

		//�������׼���ɹ�����һ�β����ж��Ƿ������Ҫ����ķ�����Ϣ
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



//query֮��׼��������־��λ��receive�ɹ�ʱ���׼��������־Ϊtrue�������飬�������κδ���
//�ڽ���receive֮ǰ����receive��־λ��λ����׼��������ɺ���receive�Ƿ�ɹ��ˣ���δ�ɹ��������κδ�������ɹ��ˣ���ô������������

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
			//�����������ɹ�����׼����һ������֮ǰ����������Ž�����������У������������в���ʧ�ܣ���ôӦ������һ����־λ���ȴ�������Ϣ��������ټ�����һ����
			//�����������ɹ���������������ʧ�ܵĻ�����ôӦ������ѭ�����գ������η��ͳɹ�����Ϣ��������ټ�����һ���ж�
			//�����жϴ���������Ƿ�Ϊ�գ������Ϊ�գ�Ӧ�����Ƚ���������е���Ϣȫ��������ϣ���ȡ������������Ȼ��������ĿΪ0ʱ��Ϊ����Ӧ�ô���������ˣ��ȸ�����Ҳ���ܴ�����Ϻ�
			//�ټ���׼����һ��redis���չ����������µ�query
			//��֮���������������Ϊ0��ֱ�Ӽ����ȴ�������Ϣ�������֮��׼����һ��redis���չ����������µ�query
			//
			//����ɹ���׼����һ��redis���չ���
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
		//��������ɹ�����Ѿ��ɹ������������ϢӦ�ô������պ���ϢΪֹ�������Ƕ����������
		//��������ˣ����н����������ûص�������û�н���Ļ����Ǵ�������еķ��ʹ�����Ϣ�����ûص�����


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
			//������Ϣ

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
			//������Ϣ

			handleMessage();



			while (m_messageResponse.popTop(m_tempSINGLEMESSAGE))
			{
				(std::get<3>(*(m_tempSINGLEMESSAGE->m_tempRequest)))(m_tempSINGLEMESSAGE->m_tempVaild, m_tempSINGLEMESSAGE->m_tempEM);
			}
		}
		else
		{
			//������Ϣ

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



//��һ�ν������ԣ�ֻҪ�п��Խ��з��Ͳ�ѯ������Ŀ��ܣ����ӳٵ��첽�ȴ�ʱ����
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