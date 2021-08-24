#include "multiSQLREAD.h"

MULTISQLREAD::MULTISQLREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST,
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT, const unsigned int commandMaxSize)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB), m_unlockFun(unlockFun),m_commandMaxSize(commandMaxSize)
{
	try
	{
		if (SQLHOST.empty())
			throw std::runtime_error("SQLHOST is empty");
		if (SQLUSER.empty())
			throw std::runtime_error("SQLUSER is empty");
		if (SQLPASSWORD.empty())
			throw std::runtime_error("SQLPASSWORD is empty");
		if (SQLDB.empty())
			throw std::runtime_error("SQLDB is empty");
		if (!ioc)
			throw std::runtime_error("io_context is empty");
		if (SQLPORT.empty())
			throw std::runtime_error("SQLPORT is empty");
		if (!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (!std::all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'),
			std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		m_port = stoi(SQLPORT);
		if (m_port < 1 || m_port>65535)
			throw std::runtime_error("SQLPORT is invaild number");
		if(!commandMaxSize)
			throw std::runtime_error("commandMaxSize is invaild number");
		m_commandNowSize = 0;


		m_waitMessageList.reset(new std::shared_ptr<resultType>[m_commandMaxSize]);
		m_waitMessageListMaxSize = m_commandMaxSize;


		m_resultArray.reset(new MYSQL_RES *[m_commandMaxSize]);
		m_resultMaxSize = m_commandMaxSize;


		m_rowField.reset(new unsigned int[m_commandMaxSize * 2]);
		m_rowFieldMaxSize = m_commandMaxSize * 2;


		m_timer.reset(new steady_timer(*ioc));
		m_freeSqlResultFun.reset(new std::function<void(MYSQL_RES **, const unsigned int)>(std::bind(&MULTISQLREAD::FreeResult, this ,std::placeholders::_1, std::placeholders::_2)));
		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}



std::shared_ptr<std::function<void(MYSQL_RES**, const unsigned int)>> MULTISQLREAD::sqlRelease()
{
	return m_freeSqlResultFun;
}






//  ��麯�����ȼ�鵱ǰ���������Ѿ��ﵽ�޶�ֵ����ȥ����������û��Ԫ��

//  ����m_messageList���У���װ��Ϣǰ����waitMessage�̶����ȶ�����
//  ���Ȳ���string���棬Ȼ���ٵ�m_messageList������

bool MULTISQLREAD::insertSqlRequest(std::shared_ptr<resultType> sqlRequest)
{
	unsigned int requestTime{ std::get<2>(*sqlRequest) }, pointerTime{ std::get<1>(*sqlRequest) };
	const char **requestPointer{ std::get<0>(*sqlRequest) };
	if (!m_hasConnect.load() || !sqlRequest  || !pointerTime || !requestTime || requestTime > m_commandMaxSize || pointerTime % 2 ||
		std::any_of(requestPointer, requestPointer + pointerTime,std::bind(std::equal_to<>(),std::placeholders::_1,nullptr)))
		return false;


	//�����ж��Ƿ��Ѿ����ӣ�
	//δ����ֱ���ж�ʧ�ܣ���Ϊδ������Ŀǰ�������ղ���
	//
	//�ж�ִ��״̬��
	//���û��ִ�У����Խ��ô�����ƴ��������Ĭ�ϵ������м����;���Ѿ������
	//�����ִ�У���ֱ�Ӳ���ȴ��������֮��

	unsigned int strBeginSize{}, messageBeginSize{}, listBeginSize{}, index{}, charSize{};
	const char ** begin{ requestPointer }, **end{ requestPointer + pointerTime };

	char *messageIter{ m_messageBuffer.get() };

	m_messageBufferNowSize = 0;
	
	//���û����ִ��״̬
	//����ͳ������Ҫ���ַ����ܳ��ȣ�������������з��䣬���ɱ���������ַ���
	//Ȼ�󽫱��ε������Բ������ȡ�����������
	if (!m_queryStatus.load())
	{
		
		m_queryStatus.store(true);

		while (begin != end)
		{
			charSize += *(begin + 1) - *begin;
			begin += 2;
		}


		try
		{
			if (m_messageBufferMaxSize < charSize)
			{
				m_messageBuffer.reset(new char[charSize]);
				m_messageBufferMaxSize = charSize;
				messageIter = m_messageBuffer.get();
			}
		}
		catch (const std::exception &e)
		{
			m_queryStatus.store(false);
			return false;
		}


		begin = requestPointer;
		while (begin != end)
		{
			copy(*begin, *(begin + 1), messageIter);
			messageIter+= *(begin + 1) - *begin;
			begin += 2;
		}


		m_messageBufferNowSize = charSize;
		m_waitMessageListNowSize = 1;
		*(m_waitMessageList.get()) = sqlRequest;
		


		//���봦����
		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		m_commandNowSize = requestTime;

		firstQuery();

		return true;
	}
	else   
	{
		//�������ִ��״̬
		//���Բ�����������֮��

		m_messageList.getMutex().lock();

		if (!m_messageList.unsafeInsert(sqlRequest))
		{
			m_messageList.getMutex().unlock();
			return false;
		}
		m_messageList.getMutex().unlock();

		return true;
	}
	
	return false;
}




MULTISQLREAD::~MULTISQLREAD()
{
	FreeConnect();
}




void MULTISQLREAD::FreeResult(MYSQL_RES ** resultPtr, const unsigned int resultLen)
{
	if (!resultPtr || !resultLen)
		return;

	for (m_temp = 0; m_temp != resultLen; ++m_temp)
	{
		if(*resultPtr)
			mysql_free_result(*resultPtr);
		++resultPtr;
	}
}



void MULTISQLREAD::FreeConnect()
{
	if (m_hasConnect.load())
	{
		mysql_close(m_mysql);
		m_hasConnect.store(false);
	}
}



void MULTISQLREAD::ConnectDatabase()
{
	if (!m_hasConnect)
	{
		if (!(m_mysql = mysql_init(nullptr)))
		{
			m_timer->expires_after(std::chrono::seconds(1));
			m_timer->async_wait([this](const boost::system::error_code &err)
			{
				if (err)
				{
					//tryResetTimer();
				}
				else
				{
					ConnectDatabase();
				}
			});
			return;
		}

		ConnectDatabaseLoop();
	}
}



void MULTISQLREAD::ConnectDatabaseLoop()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket, 
		CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS | CLIENT_OPTIONAL_RESULTSET_METADATA);
	m_timer->expires_after(std::chrono::milliseconds(100));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)
		{
			cout << "sql error,please restart server\n";
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_ERROR:
				if (!resetConnect)
					cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_COMPLETE:
				if (!resetConnect)
					cout << "connect multi mysqlRead success\n";
				m_hasConnect.store(true);
				if (!resetConnect)
					(*m_unlockFun)();
				else
					resetConnect = false;
				break;

			default:
				if (!resetConnect)
					cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;
			}
		}
	});
}



void MULTISQLREAD::firstQuery()
{
	m_status = mysql_real_query_nonblocking(m_mysql, m_sendMessage, m_sendLen);
	m_timer->expires_after(std::chrono::microseconds(200));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{

		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				firstQuery();
				break;
			case NET_ASYNC_ERROR:              // �ص���������
				//������ȡ����Լ���������н��д������ش���Ȼ����գ�����   ���Ƚ���������λfalse 
				//����Ǵ���Ĳ�ѯ�ᱨ��
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				//handleLostConnection();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:
				break;
			case NET_ASYNC_COMPLETE:
				store();
				break;
			default:
				store();                       // store_result
				break;
			}
		}
	});
}



void MULTISQLREAD::handleAsyncError()
{
	m_hasConnect.store(false);

	clearMessageList(ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
	
	clearWaitMessageList();
	

	m_commandNowSize = m_resultNowSize = 0;

	mysql_close(m_mysql);

	resetConnect = true;

	ConnectDatabase();
}



void MULTISQLREAD::store()
{
	m_status = mysql_store_result_nonblocking(m_mysql, &m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{
			//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				store();
				break;
			case NET_ASYNC_ERROR:        // �ص���������
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:


				break;
			case NET_ASYNC_COMPLETE:
				if (!m_result)
				{
					//����
					//std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::SQL_MYSQL_RES_NULL);
				}
				else
				{
					*(m_resultArray.get() + m_resultNowSize++) = m_result;
					m_result = nullptr;
					if (m_resultNowSize == m_commandNowSize)
					{
						checkRowField();                       // store_result
					}
					else
					{
						next_result();
					}
				}
				break;
			default:
				
				break;
			}
		}
	});
}



void MULTISQLREAD::next_result()
{
	m_status = mysql_next_result_nonblocking(m_mysql);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{
			//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				next_result();
				break;
			case NET_ASYNC_ERROR:        // �ص���������
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				
				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:
				
				break;
			case NET_ASYNC_COMPLETE:
				store();
				break;
			default:
				
				break;
			}
		}
	});
}



//uint64_t STDCALL mysql_num_rows(MYSQL_RES *res);
//unsigned int STDCALL mysql_num_fields(MYSQL_RES *res);
void MULTISQLREAD::checkRowField()
{
	m_rowFieldNowSize = 0;
	unsigned int rowNum{}, fieldNum{};
	unsigned int *rowFieldIter{ m_rowField.get() };
	MYSQL_RES **resultIter{ m_resultArray.get() };
	for (int i = 0; i != m_commandNowSize; ++i)
	{
		rowNum = mysql_num_rows(*resultIter);
		fieldNum = mysql_num_fields(*resultIter++);

		*rowFieldIter++ = rowNum;
		*rowFieldIter++ = fieldNum;
		m_rowFieldNowSize += rowNum * fieldNum;
	}

	/////////////////////////////////
	fetch_row();
}



void MULTISQLREAD::fetch_row()
{
	//mysql_fetch_row()
	int thisRow{}, thisField{}, rowCount{}, fieldCount{}, rowNum{};
	MYSQL_RES **resultIter{ m_resultArray.get() };
	unsigned int *rowFieldIter{ m_rowField.get() };

	m_dataVec.clear();
	try
	{
		for (int i = 0; i != m_commandNowSize; ++i)
		{
			thisRow = *rowFieldIter++;      //  ����
			thisField = *rowFieldIter++;    // ����

			rowCount = -1;
			while (++rowCount != thisRow)
			{
				m_row = mysql_fetch_row(*resultIter);

				//����mysql�ĺ��������ﲻ���п�
				m_lengths = mysql_fetch_lengths(*resultIter);

				for (fieldCount = 0; fieldCount != thisField; ++fieldCount)
				{
					if (m_row[fieldCount])
					{
						m_dataVec.emplace_back(m_row[fieldCount]);
						m_dataVec.emplace_back(m_row[fieldCount] + m_lengths[fieldCount]);
					}
					else
					{
						m_dataVec.emplace_back(nullptr);
						m_dataVec.emplace_back(nullptr);
					}
				}
			}

			++resultIter;
		}
	}
	catch (const std::exception &e)
	{
		//˼�� ���ﴦ��Ƿ��
		clearWaitMessageList();

		clearDataArray();
		//���Լ��messageList�Ƿ��д�ƴ���ַ���
		readyMakeMessage();

		makeMessage();

		return;
	}

	//��ʼ�������������
	multiGetResult();

	readyMakeMessage();

	makeMessage();
}



/*
	ִ�����ָ��

	�������const char*����string װ��ʵ���ַ�����Ϣ��������Ϊ�����������Կɶ���ȷʵ�ܺã����ǻ����һ�����ܣ�ԭ����������ƴ�ӹ����в��ɱ����������ݿ�������������ʹ�ö���ָ�룬��ȻҲ�����ݿ�����
	���ǿ��Խ�����������Ľ������ͣ����������ַ�������ֻ�ڷ�������׼�������н���һ��


	ָ����� ��ʵ�ʸ���=ָ�����/2  ��Ϊ������βλ�ã�����Ƕ���м���Ҫ����;��
	ִ���������
	�����ָ��
	������� ����int*    ÿ2���洢һ�����������������
	������� ����buffer��С
	���ָ�룬�洢ÿһ���Ľ��ָ��
	���ָ��buffer��С
	�ص�����
	*/

//  using resultType = std::tuple<const char**, unsigned int, unsigned int, MYSQL_RES**, std::shared_ptr<const unsigned int[]>, unsigned int, std::shared_ptr<const char*[]>, unsigned int,
//  std::function<void(bool, enum ERRORMESSAGE) >> ;


void MULTISQLREAD::multiGetResult()
{
	if (m_waitMessageListNowSize)
	{
		std::shared_ptr<resultType> result{};
		MYSQL_RES **resultIter{ m_resultArray.get() }, **everyResult{};
		unsigned int *rowFieldIter{ m_rowField.get() }, *rowFieldBegin{ m_rowField.get() }, *roeFieldEnd{ m_rowField.get() + m_commandNowSize * 2};
		unsigned int ResultNum{}, index{};
		auto dataIter = m_dataVec.cbegin();
		auto begin = m_waitMessageList.get(), end = m_waitMessageList.get() + m_waitMessageListNowSize;


		bool success{ true };
		// m_messageNowSize  * 2  Ϊ�洢������������Ŀռ��С

		//����洢���ָ����������
		unsigned int everyResultSize{ }, everyRorFielfSize{};
		unsigned int *everyRowField{};
		const char **everyRes{};
		unsigned int rowNum{}, fieldNum{}, everyResNum{};

		do
		{
			success = true;
			result = *begin;
			ResultNum = std::get<2>(*result);
			everyRorFielfSize = ResultNum * 2;

			if (std::get<4>(*result) < ResultNum)
			{
				try
				{
					std::get<3>(*result).reset(new MYSQL_RES*[ResultNum]);
					std::get<4>(*result) = ResultNum;
				}
				catch (const std::exception &e)
				{
					success = false;
				}
			}

			if (success)
			{
				everyResult = std::get<3>(*result).get();
			}
			

			// ������洢��������Ŀռ�
			if (success)
			{
				if (std::get<6>(*result) < everyRorFielfSize)
				{
					try
					{
						std::get<5>(*result).reset(new unsigned int[everyRorFielfSize]);
						std::get<6>(*result) = everyRorFielfSize;
					}
					catch (const std::exception &e)
					{
						success = false;
					}
				}
			}


			if (success)
			{
				// ������洢�������Ŀռ�
				everyResultSize = 0;

				index = -1;

				while (++index != ResultNum)
				{
					everyResultSize = *rowFieldBegin * (*(rowFieldBegin + 1));
					rowFieldBegin += 2;
				}
				everyResultSize *= 2;

				if (std::get<8>(*result) < everyResultSize)
				{
					try
					{
						std::get<7>(*result).reset(new const char*[everyResultSize]);
						std::get<8>(*result) = everyResultSize;
					}
					catch (const std::exception &e)
					{
						success = false;
					}
				}
			}
			////////////////


			if (success)
			{
				everyRowField = (std::get<5>(*result)).get();
				everyRes = (std::get<7>(*result)).get();
				while (ResultNum--)
				{
					*everyResult++ = *resultIter++;

					rowNum = *rowFieldIter++, fieldNum = *rowFieldIter++;

					*everyRowField++ = rowNum;

					*everyRowField++ = fieldNum;

					everyResNum = rowNum * fieldNum * 2;

					index = -1;

					while (++index != everyResNum)
					{
						*everyRes++ = *dataIter++;
					}
				}
				++begin;

				//����
				std::get<9>(*result)(true, ERRORMESSAGE::OK);
			}
			else   //��������һ������ʧ�����������ν��Ȼ�󷵻ش���
			{	
				while (ResultNum--)
				{
					++resultIter;

					rowNum = *rowFieldIter++, fieldNum = *rowFieldIter++;

					dataIter += rowNum * fieldNum * 2;
				}

				++begin;
				//����
				std::get<9>(*result)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
			}
		} while (begin != end);
		m_waitMessageListNowSize = m_commandNowSize = 0;
	}
}




void MULTISQLREAD::clearDataArray()
{
	if (m_commandNowSize)
	{
		FreeResult(m_resultArray.get(), m_commandNowSize);
		m_commandNowSize = 0;
	}
}




//   ������m_messageList�л�ȡԪ�ش���waitMessage�У���ֹ����Ϊ50�����50Ԫ�أ�
//   ���ڷ��������������ʵʱ��m_messageList��ɾ����ͳ����Ҫ���ַ������ȣ����������

void MULTISQLREAD::makeMessage()
{
	//��ʵ�֣��ȱ���һ�λ�ȡ��Ҫ�Ŀռ��Сһ�η��䣬�������������ȫ�����
	//����ʵ�֣��ڲ���ʱ�����
	//          ��ʹ������

	int status{};
	int index{ -1 };
	unsigned int resultNum{};
	std::forward_list<std::shared_ptr<resultType>>::const_iterator m_messageBegin, m_messageEnd,m_messageIter;
	std::shared_ptr<resultType> result;

	std::shared_ptr<resultType> *waitBegin, *waitEnd;
	unsigned int totalLen{}, totalResult{}, everyResult{}, pointerNum{}, waitMessageEndNum{ m_waitMessageListMaxSize };

	const char **PointerBegin{}, **pointEnd{}, **pointer{};
	char *messageIter{ };

	m_messageBufferNowSize = 0;
	bool openLock{ false };

	while (true)
	{
		totalLen = totalResult = m_commandNowSize = m_waitMessageListNowSize = 0;

		//һ������£�������Ƶ�������˴���whileѭ�������ֻ�����ڴ�ȱ��������²Ż����ڶ������ϵ�whileѭ��
		m_messageList.getMutex().lock();
		std::forward_list<std::shared_ptr<resultType>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.getMutex().unlock();
			m_queryStatus.store(false);
			break;
		}
		
		index = -1;
		waitBegin = m_waitMessageList.get();
		while (!flist.empty())
		{
			result = *flist.cbegin();

			everyResult = std::get<2>(*result);;
			if (totalResult + everyResult > m_resultMaxSize)
				break;

			pointer = std::get<0>(*result);

			pointerNum = std::get<1>(*result);

			PointerBegin = pointer, pointEnd = pointer + pointerNum;

			if (++index)
			{
				++totalLen;
			}
			while (PointerBegin != pointEnd)
			{
				totalLen += *(PointerBegin + 1) - *PointerBegin;
				PointerBegin += 2;
			}

			*waitBegin++ = result;

			++m_waitMessageListNowSize;
			m_commandNowSize += everyResult;
			flist.pop_front();

			if (m_waitMessageListNowSize == m_waitMessageListMaxSize)
				break;
		}
		m_messageList.getMutex().unlock();


		//���������һ�����Է���ռ䣬��֮��ʼ�µļ��
		if (m_waitMessageListNowSize)
		{
			if (m_messageBufferMaxSize < totalLen)
			{
				try
				{
					m_messageBuffer.reset(new char[totalLen]);
					m_messageBufferMaxSize = totalLen;
				}
				catch (const std::exception &e)
				{
					continue;
				}
			}

			waitBegin = m_waitMessageList.get(), waitEnd = m_waitMessageList.get() + m_waitMessageListNowSize;

			index = -1;
			messageIter = m_messageBuffer.get();
			do
			{
				result = *waitBegin;

				pointer = std::get<0>(*result);

				pointerNum = std::get<1>(*result);

				PointerBegin = pointer, pointEnd = pointer + pointerNum;

				if (++index)
				{
					*messageIter++ = ';';
				}
				while (PointerBegin != pointEnd)
				{
					std::copy(*PointerBegin, *(PointerBegin + 1), messageIter);
					messageIter+= *(PointerBegin + 1) - *PointerBegin;
					PointerBegin += 2;
				}
			} while (++waitBegin != waitEnd);

			m_messageBufferNowSize = totalLen;

			m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

			firstQuery();

			break;
		}
		else
		{
			continue;
		}
	}
}



void MULTISQLREAD::clearWaitMessageList()
{
	if (m_waitMessageListNowSize)
	{
		auto begin = m_waitMessageList.get(), end = m_waitMessageList.get() + m_waitMessageListNowSize;
		do
		{
			std::get<9>(*(*begin))(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		} while (++begin != end);

		m_waitMessageListNowSize = 0;
	}
}



void MULTISQLREAD::clearMessageList(const ERRORMESSAGE value)
{
	m_messageList.getMutex().lock();
	if (!m_messageList.listSelf().empty())
	{
		auto begin = m_messageList.listSelf().cbegin(), end = m_messageList.listSelf().cend();

		do
		{
			std::get<9>(*(*begin))(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		} while (++begin != end);

		m_messageList.unsafeClear();
	}
	m_messageList.getMutex().unlock();
}



void MULTISQLREAD::readyMakeMessage()
{

	m_messageBufferNowSize = 0;
	
	m_commandNowSize = 0;

	m_sendMessage = nullptr;

	m_sendLen = 0;

	m_waitMessageListNowSize = 0;

	m_resultNowSize = 0;

	m_rowFieldNowSize = 0;

	m_dataVec.clear();
}


