#include "SQL.h"

SQL::SQL(shared_ptr<io_context> ioc, shared_ptr<function<void()>>unlockFun , const string &SQLHOST, const string &SQLUSER, const string &SQLPASSWORD, const string &SQLDB, const string &SQLPORT)
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
		if (!all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(greater_equal<char>(), std::placeholders::_1, '0'), std::bind(less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		m_port = stoi(SQLPORT);
		if (m_port < 1 || m_port>65535)
			throw std::runtime_error("SQLPORT is invaild number");
		m_timer.reset(new steady_timer(*ioc));
		m_freeSqlResultFun.reset(new std::function<void()>(std::bind(&SQL::checkList, this)));
		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}





void SQL::ConnectDatabase()
{
	if (!m_hasConnect)
	{
		if (!(m_mysql=mysql_init(nullptr)))
			throw std::runtime_error("mysql_init is fail");

		ConnectDatabaseLoop();
	}
}


void SQL::ConnectDatabaseLoop()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket, 0);
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
				break;

			case NET_ASYNC_COMPLETE:
				cout << "connect mysql success\n";
				m_hasConnect = true;
				(*m_unlockFun)();
				break;

			default:
				cout << "connect database error,please restart server\n";
				break;
			}
		}
	});
}




void SQL::checkList()
{
	if (m_result)
	{
		freeResult();
	}
	else
	{
		if (m_messageList.popTop(m_sqlRequest))
			query();
		else
			m_queryStatus.store(false);
	}
}



bool SQL::insertSqlRequest(std::shared_ptr<std::tuple<const char*, unsigned int,const char **, unsigned int, unsigned int, function<void(bool, enum ERRORMESSAGE)>,unsigned int>> sqlRequest)
{
	if (!sqlRequest || !std::get<0>(*sqlRequest) || !std::get<1>(*sqlRequest) || !std::get<2>(*sqlRequest) || !std::get<5>(*sqlRequest) || !std::get<6>(*sqlRequest))
		return false;
	if (!m_queryStatus.load())
	{
		m_queryStatus.store(true);       //直接进入处理函数

		m_sqlRequest = sqlRequest;

		std::get<3>(*m_sqlRequest) = 0;

		m_data = std::get<2>(*m_sqlRequest);

		query();

		return true;
	}
	return m_messageList.insert(sqlRequest);
}



std::shared_ptr<function<void()>> SQL::getSqlUnlock()
{
	return m_freeSqlResultFun;
}










void SQL::query()
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
				break;
			default:
				store();                       // store_result
				break;
			}
		}
	});
}




void SQL::store()
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



void SQL::fetch_row()
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




void SQL::freeResult()
{
	m_status = mysql_free_result_nonblocking(m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
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
				freeResult();
				break;
			case NET_ASYNC_ERROR:        // 回调函数处理
				
				break;
			default:
				m_result = nullptr;
				checkList();
				break;
			}
		}
	});
}



void SQL::freeResultStore()
{
	m_status = mysql_store_result_nonblocking(m_mysql, &m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
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
				freeResultStore();
				break;
			case NET_ASYNC_ERROR:        // 回调函数处理
	
				break;
			default:
				checkList();
				break;
			}
		}
	});
}



void SQL::checkRowField()
{
	std::get<3>(*m_sqlRequest) = m_rowNum = mysql_affected_rows(m_mysql);
	if (!m_rowNum)                 //错误处理回调
		std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ROW_ZERO);
	std::get<4>(*m_sqlRequest) = m_fieldNum = mysql_field_count(m_mysql);
	if (!m_fieldNum)                 //错误处理回调
		std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_FIELD_ZERO);
	if (m_rowNum*m_fieldNum * 2 > std::get<6>(*m_sqlRequest))
		std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_RESULT_TOO_LAGGE);

	m_rowCount = m_rowNum;
	fetch_row();
}




void SQL::FreeConnect()
{
	if (m_hasConnect)
	{
		mysql_close(m_mysql);
		m_hasConnect = false;
	}
}



SQL::~SQL()
{
	FreeConnect();
}



/*
bool SQL::Mymysql_real_query(const string &source)
{
	if (!source.empty())
	{
		do
		{
			res = mysql_store_result(&mysql);
			if (res)
				mysql_free_result(res);
		} while (!mysql_next_result(&mysql));

		if (!mysql_real_query(&mysql, &source[0], source.size()))
		{
			return true;
		}

		//MySQL server has gone away
		errorMessage.assign(mysql_error(&mysql));
		if (errorMessage == "MySQL server has gone away" || errorMessage == "Lost connection to MySQL server during query")
			return reconnect(source);
		else
		{
			printf("Query failed (%s)\n", mysql_error(&mysql));
			std::cout << source << '\n';
		}
	}
	return false;
}



bool SQL::MysqlResult(std::vector<std::vector<std::pair<std::string, std::string>>> &resultVec)
{
	static int num;
	static pair<std::string, std::string> onePair;
	static std::vector<pair<std::string, std::string>> result;

	num = 0;
	onePair.first.clear();
	onePair.second.clear();
	resultVec.clear();
	result.clear();

	if (!(res = mysql_store_result(&mysql))) //获得sql语句结束后返回的结果集
	{
		printf("Couldn't get result from %s\n", mysql_error(&mysql));
		return false;
	}

	//打印数据行数
	if (!mysql_affected_rows(&mysql))
		return false;

	if (!(num = mysql_field_count(&mysql)))
		return false;

	//获取字段的信息

	for (int i = 0; i < num; i++)
	{
		onePair.first.assign(mysql_fetch_field(res)->name);
		result.emplace_back(onePair);
	}

	while (column = mysql_fetch_row(res)) //在已知字段数量情况下，获取并打印下一行
	{
		for (int i = 0; i != num; ++i)
		{
			if (column[i])
				result[i].second.assign(column[i]);
			else
				result[i].second.clear();
		}
		resultVec.emplace_back(result);
	}

	do
	{
		res = mysql_store_result(&mysql);
		if (res)
			mysql_free_result(res);
	} while (!mysql_next_result(&mysql));

	return true;
}



const char *SQL::getSqlError()
{
	return mysql_error(&mysql);
}



bool SQL::reconnect(const string &source)
{
	FreeConnect();
	while (!ConnectDatabase())
	{
	}
	return Mymysql_real_query(source);
}



bool SQL::MYmysql_select_db(const char *DBname)
{
	if (DBname)
	{
		if (mysql_select_db(&mysql, DBname))
		{
			printf("Couldn't select db:%s\n", mysql_error(&mysql));
			return false;
		}
		return true;
	}
	return false;
}
*/



