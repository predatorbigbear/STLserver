#include "SQLREAD.h"

SQLREAD::SQLREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun , const std::string &SQLHOST, const std::string &SQLUSER, 
	const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB),m_unlockFun(unlockFun)
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
		if(!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (!std::all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'), 
			std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		m_port = stoi(SQLPORT);
		if (m_port < 1 || m_port>65535)
			throw std::runtime_error("SQLPORT is invaild number");
		m_timer.reset(new steady_timer(*ioc));
		m_freeSqlResultFun.reset(new std::function<void()>(std::bind(&SQLREAD::checkList, this)));
		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}





void SQLREAD::ConnectDatabase()
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



void SQLREAD::ConnectDatabaseLostConnection()
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
					ConnectDatabaseLostConnection();
				}
			});
			return;
		}

		ConnectDatabaseLoopLostConnection();
	}
}



void SQLREAD::ConnectDatabaseLoop()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket, CLIENT_MULTI_STATEMENTS| CLIENT_MULTI_RESULTS);
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
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_COMPLETE:
				cout << "connect mysqlRead success\n";
				m_hasConnect.store(true);
				(*m_unlockFun)();
				break;

			default:
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;
			}
		}
	});
}


void SQLREAD::ConnectDatabaseLoopLostConnection()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket, CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS);
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
				ConnectDatabaseLoopLostConnection();
				break;

			case NET_ASYNC_ERROR:
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLostConnection();
				break;

			case NET_ASYNC_COMPLETE:
				cout << "connect mysqlRead success\n";
				m_hasConnect.store(true);
				checkList();
				break;

			default:
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLoopLostConnection();
				break;
			}
		}
	});
}




void SQLREAD::checkList()
{
	if (m_result)
	{
		freeResult();
	}
	else
	{
		if (m_hasConnect.load())
		{
			if (m_nextSqlRequest)
			{
				m_sqlRequest = m_nextSqlRequest;

				m_nextSqlRequest.reset();

				std::get<3>(*m_sqlRequest) = std::get<4>(*m_sqlRequest) = 0;

				std::get<2>(*m_sqlRequest) = m_data = m_sqlResult.get();

				firstQuery();
			}
			else
			{

				if (m_messageList.popTop(m_sqlRequest))
				{
					std::get<3>(*m_sqlRequest) = std::get<4>(*m_sqlRequest) = 0;

					std::get<2>(*m_sqlRequest) = m_data = m_sqlResult.get();

					firstQuery();
				}
				else
					m_queryStatus.store(false);
			}
		}
	}
}



void SQLREAD::handleLostConnection()
{
	FreeConnect();
	ConnectDatabaseLostConnection();
}




bool SQLREAD::insertSqlRequest(std::shared_ptr<std::tuple<const char*, unsigned int,const char **, unsigned int, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>,unsigned int>> sqlRequest)
{
	if (m_hasConnect.load())
	{
		if (!sqlRequest || !std::get<0>(*sqlRequest) || !std::get<1>(*sqlRequest) || !std::get<5>(*sqlRequest))
			return false;
		if (!m_queryStatus.load())
		{
			m_queryStatus.store(true);       //直接进入处理函数

			m_sqlRequest = sqlRequest;

			std::get<2>(*m_sqlRequest) = m_data = m_sqlResult.get();

			std::get<3>(*m_sqlRequest) = std::get<4>(*m_sqlRequest) = 0;

			m_result = nullptr;

			firstQuery();

			return true;
		}
		return m_messageList.insert(sqlRequest);
	}
	return false;
}





std::shared_ptr<std::function<void()>> SQLREAD::getSqlUnlock()
{
	return m_freeSqlResultFun;
}







void SQLREAD::firstQuery()
{
	m_status = mysql_real_query_nonblocking(m_mysql, std::get<0>(*m_sqlRequest), std::get<1>(*m_sqlRequest));
	if (!m_nextSqlRequest)
	{
		m_messageList.popTop(m_nextSqlRequest);
	}

	switch (m_status)
	{
	case NET_ASYNC_NOT_READY:
		query();
		break;
	case NET_ASYNC_ERROR:              // 回调函数处理
		std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
		handleLostConnection();
		break;

	default:
		store();                       // store_result
		break;
	}
}



void SQLREAD::query()
{
	m_status = mysql_real_query_nonblocking(m_mysql, std::get<0>(*m_sqlRequest),std::get<1>(*m_sqlRequest));
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
		{
			std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				query();
				break;
			case NET_ASYNC_ERROR:        // 回调函数处理
				std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleLostConnection();
				break;
			default:
				store();                       // store_result
				break;
			}
		}
	});
}




void SQLREAD::store()
{
	m_status = mysql_store_result_nonblocking(m_mysql, &m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
		{
			std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				store();
				break;
			case NET_ASYNC_ERROR:        // 回调函数处理
				std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleLostConnection();
				break;
			default:
				if(!m_result)
					std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::SQL_MYSQL_RES_NULL);
				else
					checkRowField();                       // store_result
				break;
			}
		}
	});
}



void SQLREAD::fetch_row()
{
	if (m_rowCount)
	{
		m_status = mysql_fetch_row_nonblocking(m_result, &m_row);
		m_timer->expires_after(std::chrono::microseconds(5));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (err)   //回调函数处理
			{
				std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
			}
			else
			{
				switch (m_status)
				{
				case NET_ASYNC_NOT_READY:
					fetch_row();
					break;
				case NET_ASYNC_ERROR:        // 回调函数处理
					std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
					handleLostConnection();
					break;
				default:
					for (m_temp = 0; m_temp != m_fieldNum; ++m_temp)
					{
						if (m_row[m_temp])
						{
							*m_data++ = m_row[m_temp];
							*m_data++ = m_row[m_temp] + strlen(m_row[m_temp]);
						}
						else
						{
							*m_data++ = nullptr;
							*m_data++ = nullptr;
						}
					}
					--m_rowCount;
					fetch_row();
					break;
				}
			}
		});
	}
	else
	{
		std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::OK);
	}
}




void SQLREAD::freeResult()
{
	m_status = mysql_free_result_nonblocking(m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
		{
			std::cout << "freeResult error\n";
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				freeResult();
				break;
			case NET_ASYNC_ERROR:        // 回调函数处理
				std::cout << "freeResult async error\n";
				handleLostConnection();
				break;
			default:
				m_result = nullptr;
				checkList();
				break;
			}
		}
	});
}



void SQLREAD::checkRowField()
{
	std::get<3>(*m_sqlRequest) = m_rowNum = mysql_affected_rows(m_mysql);
	if (!m_rowNum)                 //错误处理回调
	{
		std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ROW_ZERO);
		return;
	}
	std::get<4>(*m_sqlRequest) = m_fieldNum = mysql_field_count(m_mysql);
	if (!m_fieldNum)                 //错误处理回调
	{
		std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_FIELD_ZERO);
		return;
	}
	int sqlnum{ m_rowNum*m_fieldNum * 2 };
	if (sqlnum > m_sqlResultSize)
	{
		try
		{
			m_sqlResult.reset(new const char*[sqlnum]);
			m_sqlResultSize = sqlnum;
			std::get<2>(*m_sqlRequest) = m_data = m_sqlResult.get();
		}
		catch (const std::exception &e)
		{
			m_sqlResult.reset();
			m_sqlResultSize = 0;
			std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_RESULT_TOO_LAGGE);
			return;
		}
	}
	m_rowCount = m_rowNum;
	fetch_row();
}




void SQLREAD::FreeConnect()
{
	if (m_hasConnect.load())
	{
		mysql_close(m_mysql);
		m_hasConnect.store(false);
	}
}



SQLREAD::~SQLREAD()
{
	FreeConnect();
}


