#include "multiSQLREADSW.h"

MULTISQLREADSW::MULTISQLREADSW(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST, 
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT, const unsigned int commandMaxSize, 
	std::shared_ptr<LOG> log, const unsigned int bufferSize)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB), m_unlockFun(unlockFun), m_commandMaxSize(commandMaxSize),m_log(log),
	m_messageBufferMaxSize(bufferSize)
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
	if (!log)
		throw std::runtime_error("log is nullptr");
	if (!std::all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'),
		std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
		throw std::runtime_error("SQLPORT is not number");
	m_port = std::stoi(SQLPORT);
	if (m_port < 1 || m_port>65535)
		throw std::runtime_error("SQLPORT is invaild number");
	if (!commandMaxSize)
		throw std::runtime_error("commandMaxSize is invaild number");
	m_commandNowSize = 0;


	m_waitMessageList.reset(new std::shared_ptr<resultTypeSW>[m_commandMaxSize]);
	m_waitMessageListMaxSize = m_commandMaxSize;

	m_messageList.reset(m_commandMaxSize * 6);


	m_timer.reset(new boost::asio::steady_timer(*ioc));
	m_freeSqlResultFun.reset(new std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>(std::bind(&MULTISQLREADSW::FreeResult, this, std::placeholders::_1)));


	m_messageBuffer.reset(new char[m_messageBufferMaxSize]);
	m_messageBufferMaxSize = m_messageBufferMaxSize;



	if (!(m_mysql = mysql_init(m_mysql)))
	{
		std::cout << "mysql init error\n";
		return;
	}


	ConnectDatabase();
}



std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>> MULTISQLREADSW::sqlRelease()
{
	return m_freeSqlResultFun;
}




bool MULTISQLREADSW::insertSqlRequest(std::shared_ptr<resultTypeSW> &sqlRequest)
{
	if (!sqlRequest)
		return false;

	resultTypeSW& thisRequest{ *sqlRequest };


	if(std::get<0>(thisRequest).get().empty() ||
		std::any_of(std::get<0>(thisRequest).get().cbegin(), std::get<0>(thisRequest).get().cend(), [](auto const sw)
	{
		return sw.empty();
	}) || !std::get<1>(thisRequest) || std::get<1>(thisRequest) > m_commandMaxSize)
		return false;

	std::vector<MYSQL_RES*>& vec{ std::get<2>(thisRequest).get() };

	if (!vec.empty())
	{
		for (auto res : vec)
		{
			mysql_free_result(res);
		}
		vec.clear();
	}
	

	if (!m_hasConnect.load())
	{
		return false;
	}
	if (!m_queryStatus.load())
	{
		m_queryStatus.store(true);

		m_sendLen = std::accumulate(std::get<0>(*sqlRequest).get().cbegin(),std::get<0>(*sqlRequest).get().cend(),0,[](auto &sum,auto const sw)
		{
			return sum += sw.size();
		});

		try
		{
			if (m_messageBufferMaxSize < m_sendLen)
			{
				m_messageBuffer.reset(new char[m_sendLen]);
				m_messageBufferMaxSize = m_sendLen;
			}
		}
		catch (const std::bad_alloc &e)
		{
			m_queryStatus.store(false);
			return false;
		}
		

		char *buffer{ m_messageBuffer.get() };
		for (auto sw : std::get<0>(thisRequest).get())
		{
			std::copy(sw.cbegin(), sw.cend(), buffer);
			buffer += sw.size();
		}

		m_messageBufferNowSize = m_sendLen;
		m_waitMessageListNowSize = 1;
		*(m_waitMessageList.get()) = sqlRequest;
		m_commandNowSize = std::get<1>(thisRequest);
		m_sendMessage = m_messageBuffer.get();

		
		readyQuery();

		//进入处理函数
		firstQuery();

		return true;
	}
	else
	{
		//如果正在执行状态
		//则尝试插入待插入队列之中

		m_messageList.lock();
		
		if (!m_messageList.unsafeInsert(sqlRequest))
		{
			m_messageList.unlock();
			return false;
		}
		m_messageList.unlock();

		return true;
	}

	return false;


	////////////////////////////////////////////////////////////////////////////////////////////

	//首先判断是否已经连接，
	//未连接直接判断失败，因为未连接下目前会进行清空操作
	//
	//判断执行状态，
	//如果没有执行，则尝试将该次请求拼凑起来，默认调用者中间带有;。已经处理好
	//如果在执行，则直接插入等待插入队列之中

	//如果没有在执行状态
	//首先统计所需要的字符串总长度，如果不够，进行分配，生成本次请求的字符串
	//然后将本次的请求尝试插入待获取结果队列里面

	//如果正在执行状态
	//则尝试插入待插入队列之中
}




void MULTISQLREADSW::FreeResult(std::reference_wrapper<std::vector<MYSQL_RES*>> vecwrapper)
{
	if (vecwrapper.get().empty())
		return;

	for (auto sqlPtr : vecwrapper.get())
	{
		if(sqlPtr)
			mysql_free_result(sqlPtr);
	}
	vecwrapper.get().clear();
}



MULTISQLREADSW::~MULTISQLREADSW()
{
	FreeConnect();
}



void MULTISQLREADSW::FreeConnect()
{
	if (m_hasConnect.load())
	{
		mysql_close(m_mysql);
		m_hasConnect.store(false);
	}
}



void MULTISQLREADSW::ConnectDatabase()
{
	if (!m_hasConnect.load())
	{
		ConnectDatabaseLoop();
	}
}



void MULTISQLREADSW::ConnectDatabaseLoop()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket,
		CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS | CLIENT_OPTIONAL_RESULTSET_METADATA);
	m_timer->expires_after(std::chrono::milliseconds(100));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)
		{
			
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
					std::cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_COMPLETE:
				if (!resetConnect)
					std::cout << "connect multi mysqlRead string_view success\n";
				m_hasConnect.store(true);
				if (!resetConnect)
					(*m_unlockFun)();
				else
					resetConnect = false;
				break;

			default:
				if (!resetConnect)
					std::cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;
			}
		}
	});
}




void MULTISQLREADSW::firstQuery()
{
	m_status = mysql_real_query_nonblocking(m_mysql, m_sendMessage, m_sendLen);
	m_timer->expires_after(std::chrono::microseconds(100));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
		{
		
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				firstQuery();
				break;
			case NET_ASYNC_ERROR:              // 回调函数处理
				//将待获取结果以及待插入队列进行处理，返回错误，然后清空，重连   首先将已连接置位false 
				//如果是错误的查询会报错
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleAsyncError();
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



void MULTISQLREADSW::handleAsyncError()
{
	m_hasConnect.store(false);
	

	m_messageList.lock();
	if (!m_messageList.isEmpty())
	{
		std::shared_ptr<resultTypeSW> *request{};
		
		do
		{
			m_messageList.unsafePop(*request);
			if (*request)
			{
				std::get<5>(**request)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
			}
		} while (*request);
	}
	m_messageList.unlock();


	if (m_waitMessageListNowSize)
	{
		std::shared_ptr<resultTypeSW> *begin{ m_waitMessageList.get() }, *end{ m_waitMessageList.get() + m_waitMessageListNowSize };
		do
		{
			std::get<5>(**begin)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
		} while (++begin != end);
		m_waitMessageListNowSize = 0;
	}

	resetConnect = true;
	m_queryStatus.store(false);

	ConnectDatabase();
}



void MULTISQLREADSW::store()
{
	m_status = mysql_store_result_nonblocking(m_mysql, &m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
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
			case NET_ASYNC_ERROR:        // 回调函数处理
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:


				break;
			case NET_ASYNC_COMPLETE:
				if (!m_result)
				{

	                //清理
					//std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::SQL_MYSQL_RES_NULL);
				}
				else
				{
					//首先检查行和列，存储起来
					//根据行列不断地获取每行数据存储，一旦失败，释放本次MYSQL_RES指针，判断本节点是否还有请求，有的话置位跳过
					if (m_jumpThisRes)
					{
						mysql_free_result(m_result);
						if (++m_currentCommandSize == m_thisCommandToalSize)
						{
							m_jumpThisRes = false;
							if (readyNextQuery())
							{
								next_result();
							}
							else
							{
								//检查待请求队列中是否有消息存在
								checkMakeMessage();
							}
						}
						else
						{
							next_result();
						}
					}
					else
					{
						//获取行数,列数
						m_rowNum = mysql_num_rows(m_result), m_fieldNum = mysql_num_fields(m_result);
						m_rowCount = m_fieldCount = -1;


						try
						{
							std::vector<unsigned int> &rowFieldVec = std::get<3>(**m_waitMessageListBegin);
							rowFieldVec.emplace_back(m_rowNum);
							rowFieldVec.emplace_back(m_fieldNum);
						}
						catch (const std::bad_alloc &e)
						{
							handleSqlMemoryError();
							return;
						}

						fetch_row();
					}
				}
				break;
			default:

				break;
			}
		}
	});
}



void MULTISQLREADSW::readyQuery()
{

	//根据m_waitMessageListBegin获取每次的RES
	m_waitMessageListBegin = m_waitMessageList.get();
	m_waitMessageListEnd = m_waitMessageListBegin + m_waitMessageListNowSize;


	m_thisCommandToalSize = std::get<1>(**m_waitMessageListBegin);

	m_currentCommandSize = 0;

	m_jumpThisRes = false;
}



bool MULTISQLREADSW::readyNextQuery()
{
	if (++m_waitMessageListBegin != m_waitMessageListEnd)
	{
		
		m_thisCommandToalSize = std::get<1>(**m_waitMessageListBegin);

		m_currentCommandSize = 0;

		m_jumpThisRes = false;

		return true;
	}
	return false;
}



void MULTISQLREADSW::next_result()
{
	m_status = mysql_next_result_nonblocking(m_mysql);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
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
			case NET_ASYNC_ERROR:        // 回调函数处理
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




void MULTISQLREADSW::makeMessage()
{
	//简单实现：先遍历一次获取需要的空间大小一次分配，分配错误的情况下全部解除
	//后续实现：在插入时计算好
	//          或使用容器
	int index{ -1 };
	unsigned int resultNum{};
	// messageBuffer使用

	//waitMessage队列使用
	std::shared_ptr<resultTypeSW> *waitBegin{}, *waitEnd{}, *waitIter{};
	//统计字符串总长度，每个节点数据的命令个数
	unsigned int totalLen{}, everyResult{};

	char *bufferIter{};


	while (true)
	{
		waitBegin = nullptr;
		totalLen  = m_commandNowSize = m_waitMessageListNowSize = 0;

		//一般情况下，并不会频繁遭遇此处的while循环情况，只有在内存缺乏的情况下才会进入第二次以上的while循环
		m_messageList.lock();
		if (m_messageList.isEmpty())
		{
			m_messageList.unlock();

			m_queryStatus.store(false);
			break;
		}

		index = -1;
		waitBegin = m_waitMessageList.get();
		do
		{
			m_messageList.unsafePop(*waitBegin);
			if (!*waitBegin)
				break;
			

			//请求命令数总数不要超过最大限定值
			everyResult = std::get<1>(**waitBegin);

			m_commandNowSize += everyResult;

			//计算需要的字符空间大小
			for (auto const sw : std::get<0>(**waitBegin).get())
			{
				if (++index)
				{
					++totalLen;
				}
				totalLen += sw.size();
			}

			++waitBegin;


		} while ((std::distance(m_waitMessageList.get(), waitBegin) != m_waitMessageListMaxSize) && (m_commandNowSize < m_commandMaxSize));
		m_messageList.unlock();

		m_waitMessageListNowSize = std::distance(m_waitMessageList.get(), waitBegin);
	

		////////////////////////////////////////////////////////////////////////////////////////////////////////
		//如果至少有一个则尝试分配空间，反之开始新的检查
		
		
		//如果至少有一个消息节点有内容
		if (m_waitMessageListNowSize)
		{
			//分配失败时遍历已经存储的节点返回错误提醒
			if (m_messageBufferMaxSize < totalLen)
			{
				try
				{
					m_messageBuffer.reset(new char[totalLen]);
					m_messageBufferMaxSize = totalLen;
				}
				catch (const std::bad_alloc &e)
				{
					m_messageBufferMaxSize = 0;
					waitIter = m_waitMessageList.get();
					do
					{
						std::get<5>(**waitIter)(false, ERRORMESSAGE::SQL_QUERY_ERROR);

					} while (++waitIter != waitBegin);
					continue;
				}
			}

			
		    //组装消息到buffer里面
			waitBegin = m_waitMessageList.get(), waitEnd = m_waitMessageList.get() + m_waitMessageListNowSize;

			index = -1;
			bufferIter = m_messageBuffer.get();
			do
			{

				for (auto const sw : std::get<0>(**waitBegin).get())
				{
					if (++index)
					{
						*bufferIter++ = ';';
					}
					std::copy(sw.cbegin(), sw.cend(), bufferIter);
					bufferIter += sw.size();
				}
			} while (++waitBegin != waitEnd);


			m_messageBufferNowSize = totalLen;

			readyQuery();

			m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

			firstQuery();

			break;
		}
		else
		{
			m_queryStatus.store(false);
			break;
		}

	}

}




void MULTISQLREADSW::readyMakeMessage()
{
	m_messageBufferNowSize = 0;

	m_sendMessage = nullptr;

	m_sendLen = 0;

	m_jumpThisRes = false;

	m_commandNowSize = 0;

	m_thisCommandToalSize = 0;

	m_currentCommandSize = 0;

	
	if (m_waitMessageListNowSize)
	{
		m_waitMessageListBegin = m_waitMessageList.get();
		m_waitMessageListEnd = m_waitMessageList.get() + m_waitMessageListNowSize;
		do
		{
			m_waitMessageListBegin->reset();
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);
		m_waitMessageListNowSize = 0;

		m_waitMessageListBegin = nullptr;

		m_waitMessageListEnd = nullptr;
	}

}



void MULTISQLREADSW::checkMakeMessage()
{
	readyMakeMessage();

	makeMessage();
}



void MULTISQLREADSW::fetch_row()
{
	m_status = mysql_fetch_row_nonblocking(m_result,&m_row);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
		{
			//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			
			std::vector<std::string_view> &vec{ std::get<4>(**m_waitMessageListBegin).get() };
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				fetch_row();
				break;
			case NET_ASYNC_ERROR:        // 回调函数处理
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);

				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:

				break;
			case NET_ASYNC_COMPLETE:
				++m_rowCount;


				try
				{
					if (m_row)
					{
						m_lengths = mysql_fetch_lengths(m_result);

						for (m_fieldCount = 0; m_fieldCount != m_fieldNum; ++m_fieldCount)
						{
							if (m_row[m_fieldCount])
							{
								vec.emplace_back(std::string_view(m_row[m_fieldCount], m_lengths[m_fieldCount]));
							}
							else
							{
								vec.emplace_back(std::string_view(nullptr, 0));
							}
						}
					}
					else
					{

					}
				}
				catch (const std::bad_alloc &e)
				{
					handleSqlMemoryError();
					return;
				}



				if (m_rowCount != m_rowNum)
					fetch_row();
				else
				{

					
					try
					{
						
						std::get<2>(**m_waitMessageListBegin).get().emplace_back(m_result);
					}
					catch (const std::bad_alloc &e)
					{
						handleSqlMemoryError();
						return;
					}
					


					if (++m_currentCommandSize == m_thisCommandToalSize)
					{
						std::get<5>(**m_waitMessageListBegin)(true, ERRORMESSAGE::OK);
						if (readyNextQuery())
						{
							next_result();
						}
						else
						{
							//检查待请求队列中是否有消息存在
							checkMakeMessage();
						}
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



void MULTISQLREADSW::freeResult(std::shared_ptr<resultTypeSW>* m_waitMessageListBegin, std::shared_ptr<resultTypeSW>* m_waitMessageListEnd)
{
	
	

}




//获取每个result的结果发生错误时，首先返回错误信息进行处理，然后检查本条指令是否还有命令结果需要取回，
//如果有，则获取了然后释放，没有的话尝试获取下一个请求节点的result
void MULTISQLREADSW::handleSqlMemoryError()
{

	mysql_free_result(m_result);
	std::get<5>(**m_waitMessageListBegin)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
	if (++m_currentCommandSize == m_thisCommandToalSize)
	{
		if (readyNextQuery())
		{
			next_result();
		}
		else
		{
			//检查待请求队列中是否有消息存在
			checkMakeMessage();
		}
	}
	else
	{
		m_jumpThisRes = true;
		next_result();
	}
}



