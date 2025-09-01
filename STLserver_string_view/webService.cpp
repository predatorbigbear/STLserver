#include "webService.h"





WEBSERVICE::WEBSERVICE(const std::shared_ptr<io_context>& ioc,
	const std::shared_ptr<ASYNCLOG>& log,
	const std::string& doc_root,
	const std::shared_ptr<MULTISQLREADSW>& multiSqlReadSWMaster,
	const std::shared_ptr<MULTIREDISREAD>& multiRedisReadMaster,
	const std::shared_ptr<MULTIREDISREADCOPY>& multiRedisReadCopyMaster,
	const std::shared_ptr<MULTIREDISWRITE>& multiRedisWriteMaster,
	const std::shared_ptr<MULTISQLWRITESW>& multiSqlWriteSWMaster,
	const std::shared_ptr<STLTimeWheel>& timeWheel,
	const std::shared_ptr<std::vector<std::string>>& fileVec,
	const std::shared_ptr<std::vector<std::string>>& BGfileVec,
	const unsigned int timeOut, bool& success, const unsigned int serviceNum,
	const std::shared_ptr<std::function<void(std::shared_ptr<WEBSERVICE>&)>>& cleanFun,
	const std::shared_ptr<CHECKIP>& checkIP,
	const std::shared_ptr<RandomCodeGenerator>& randomCode,
	const std::shared_ptr<VERIFYCODE>& verifyCode,
	const unsigned int bufNum
)
	:m_ioc(ioc), m_log(log), m_doc_root(doc_root), m_BGfileVec(BGfileVec), m_checkIP(checkIP),m_randomCode(randomCode),
	m_multiSqlReadSWMaster(multiSqlReadSWMaster), m_fileVec(fileVec), m_clearFunction(cleanFun),
	m_multiRedisReadMaster(multiRedisReadMaster), m_multiRedisWriteMaster(multiRedisWriteMaster), m_multiSqlWriteSWMaster(multiSqlWriteSWMaster),
	m_timeOut(timeOut), m_timeWheel(timeWheel), m_maxReadLen(bufNum), m_defaultReadLen(bufNum), m_serviceNum(serviceNum),
	m_multiRedisReadCopyMaster(multiRedisReadCopyMaster), m_verifyCode(verifyCode)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is nullptr");
		if (!log)
			throw std::runtime_error("log is nullptr");
		if (!timeOut)
			throw std::runtime_error("timeOut is invaild");
		if (!multiSqlReadSWMaster)
			throw std::runtime_error("multiSqlReadSWMaster is nullptr");
		if (!multiRedisReadMaster)
			throw std::runtime_error("multiRedisReadMaster is nullptr");
		if (!multiRedisWriteMaster)
			throw std::runtime_error("multiRedisWriteMaster is nullptr");
		if (!multiSqlWriteSWMaster)
			throw std::runtime_error("multiSqlWriteSWMaster is nullptr");
		if (m_doc_root.empty())
			throw std::runtime_error("doc_root is empty");
		if (!m_timeWheel)
			throw std::runtime_error("timeWheel is nullptr");
		if (!bufNum)
			throw std::runtime_error("bufNum is 0");
		if (!serviceNum)
			throw std::runtime_error("serviceNum is 0");
		if(!checkIP)
			throw std::runtime_error("checkIP is nullptr");
		if(!randomCode)
			throw std::runtime_error("randomCode is nullptr");
		if(!multiRedisReadCopyMaster)
			throw std::runtime_error("multiRedisReadCopyMaster is nullptr");
		if(!verifyCode)
			throw std::runtime_error("verifyCode is nullptr");

		m_buffer.reset(new ReadBuffer(m_ioc, bufNum));

		m_verifyData.reset(new const char* [VerifyDataPos::maxBufferSize]);

		m_business = std::bind(&WEBSERVICE::sendOK, this);

		prepare();

		success = true;
	}
	catch (const std::exception& e)
	{
		success = false;
		cout << e.what() << "  ,please restart server\n";
	}
}




void WEBSERVICE::setReady(std::shared_ptr<WEBSERVICE>& other)
{

	m_mySelf = other;
	m_hasClean.store(false);
	hasLoginBack = false;
	hasUserLogin = false;
	hasVerifyRegister = false;
	m_requestTime = 0;
	m_loginBackTime = 0;           //输入后台登录密码错误计数
	m_userLoginTime = 0;           //输入用户登录密码错误计数
	m_phone.clear();
	m_userInfo.clear();

	boost::system::error_code ec;
	boost::asio::ip::tcp::endpoint remote_ep = m_buffer->getSSLSock()->lowest_layer().remote_endpoint(ec);



	if (!ec)
	{
		m_IP = remote_ep.address().to_string();
		m_port = remote_ep.port();
		m_log->writeLog(__FUNCTION__, __LINE__, m_serviceNum, m_IP, m_port);
		//判断是否是中国境内公网ip地址，不是则启动回收socket操作
		if (m_checkIP->is_china_ipv4(m_IP))
		{
			run();
		}
		else
		{
			clean();
		}	
	}
	else
	{
		m_IP.clear();
		m_port = 0;
		m_log->writeLog(__FUNCTION__, __LINE__, m_serviceNum, m_IP, m_port);
		clean();
	}
}




std::shared_ptr<WEBSERVICE>* WEBSERVICE::getListIter()
{
	// TODO: 在此处插入 return 语句
	return mySelfIter.load();
}


void WEBSERVICE::setListIter(std::shared_ptr<WEBSERVICE>* iter)
{
	if (iter)
		mySelfIter.store(iter);
}




void WEBSERVICE::checkTimeOut()
{
	if (!m_hasRecv.load())
	{
		clean();
	}
	else
	{
		m_hasRecv.store(false);
	}
}




void WEBSERVICE::run()
{
	m_readBuffer = m_buffer->getBuffer();
	m_startPos = 0;
	m_maxReadLen = m_defaultReadLen;
	handShake();
}




void WEBSERVICE::checkMethod()
{
	ReadBuffer& refBuffer{ *m_buffer };
	switch (refBuffer.getView().method())
	{
	case METHOD::POST:
		if (!refBuffer.getView().target().empty() &&
			std::all_of(refBuffer.getView().target().cbegin() + 1, refBuffer.getView().target().cend(),
				std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'), std::bind(std::less_equal<>(), std::placeholders::_1, '9'))))
		{
			switchPOSTInterface();
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		}
		break;

	case METHOD::GET:
		testGet();
		break;


	default:

		startWrite(WEBSERVICEANSWER::result3.data(), WEBSERVICEANSWER::result3.size());
		break;
	}
}




void WEBSERVICE::switchPOSTInterface()
{
	ReadBuffer& refBuffer{ *m_buffer };
	int index{ -1 }, num{ 1 };

	switch (std::accumulate(std::make_reverse_iterator(refBuffer.getView().target().cend()), std::make_reverse_iterator(refBuffer.getView().target().cbegin() + 1), 0, [&index, &num](auto& sum, auto const ch)
	{
		if (++index)num *= 10;
		return sum += (ch - '0') * num;
	}))
	{
		
	case WEBSERVICEINTERFACE::web_testMultiPartFormData:
		testMultiPartFormData();
		break;


	case WEBSERVICEINTERFACE::web_successUpload:
		testSuccessUpload();
		break;

		//登陆后台
	case WEBSERVICEINTERFACE::web_loginBack:
		loginBackGround();
		break;


		//退出管理后台
	case WEBSERVICEINTERFACE::web_exitBack:
		exitBack();
		break;


		//用户注册接口1，填写手机号发送验证码
	case WEBSERVICEINTERFACE::web_registration1:
		registration1();
		break;


		//检查手机号与验证码接口
	case WEBSERVICEINTERFACE::web_checkVerifyCode:
		checkVerifyCode();
		break;


		//提交用户注册信息
	case WEBSERVICEINTERFACE::web_registration2:
		registration2();
		break;


		//用户登录账号
	case WEBSERVICEINTERFACE::web_userLogin:
		userLogin();
		break;


		//退出用户登录账号
	case WEBSERVICEINTERFACE::web_userLoginOut:
		userLoginOut();
		break;


		//用户提交信息登记
	case WEBSERVICEINTERFACE::web_userInfo:
		userInfo();
		break;


		//查询用户五项信息
	case WEBSERVICEINTERFACE::web_getUserInfo:
		getUserInfo();
		break;


		//查询用户待审核信息到后台
	case WEBSERVICEINTERFACE::web_getUserInfoExamine:
		getUserInfoExamine();
		break;


		//默认，不匹配任何接口情况
	default:
		startWrite(WEBSERVICEANSWER::result3.data(), WEBSERVICEANSWER::result3.size());
		break;
	}
}




//读取文件接口
//读取存在文件  wrk -t1 -c100 -d60  http://127.0.0.1:8085/webfile
//读取不存在文件  wrk -t1 -c100 -d60  http://127.0.0.1:8085/webfile1
//比查map更快的获取文件数据的方式，将网页文件路径定义为数字
void WEBSERVICE::testGet()
{
	if (m_buffer->getView().target().empty())
		return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

	//没有登录后台
	if (!hasLoginBack)
	{
		if (m_buffer->getView().target().size() == 1)
			return startWrite((*m_fileVec)[0].data(), (*m_fileVec)[0].size());

		int index{ -1 }, num{ 1 }, sum{};
		//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
		if (!std::all_of(std::make_reverse_iterator(m_buffer->getView().target().cend()), std::make_reverse_iterator(m_buffer->getView().target().cbegin() + 1), [&index, &num, &sum](const char ch)
		{
			if (!std::isdigit(ch))
			{
				return false;
			}
			if (++index)
				num *= 10;
			sum += (ch - '0') * num;
			return true;
		}))
			return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

		if (sum >= m_fileVec->size())
			return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

		return startWrite((*m_fileVec)[sum].data(), (*m_fileVec)[sum].size());
	}
	else
	{
		//已经登录后台

		if (m_buffer->getView().target().size() == 1)
			return startWrite((*m_BGfileVec)[0].data(), (*m_BGfileVec)[0].size());

		int index{ -1 }, num{ 1 }, sum{};
		//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
		if (!std::all_of(std::make_reverse_iterator(m_buffer->getView().target().cend()), std::make_reverse_iterator(m_buffer->getView().target().cbegin() + 1), [&index, &num, &sum](const char ch)
		{
			if (!std::isdigit(ch))
			{
				return false;
			}
			if (++index)
				num *= 10;
			sum += (ch - '0') * num;
			return true;
		}))
			return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

		if (sum >= m_BGfileVec->size())
			return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

		return startWrite((*m_BGfileVec)[sum].data(), (*m_BGfileVec)[sum].size());


	}
}







void WEBSERVICE::testBusiness()
{
	startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
}



void WEBSERVICE::readyParseMultiPartFormData()
{
	m_boundaryLen = boundaryEnd - boundaryBegin + 4;
	char* buffer{ m_MemoryPool.getMemory<char*>(m_boundaryLen) }, * iter{};

	if (!buffer)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	iter = buffer;
	*iter++ = '\r';
	*iter++ = '\n';
	*iter++ = '-';
	*iter++ = '-';

	std::copy(boundaryBegin, boundaryEnd, iter);

	boundaryBegin = buffer + 2, boundaryEnd = buffer + m_boundaryLen;

	m_boundaryLen -= 2;

	m_parseStatus = PARSERESULT::begin_checkBoundaryBegin;

	parseReadData(*(m_verifyData.get() + VerifyDataPos::customPos1Begin), *(m_verifyData.get() + VerifyDataPos::customPos1End) - *(m_verifyData.get() + VerifyDataPos::customPos1Begin));
}




void WEBSERVICE::testMultiPartFormData()
{
	STLtreeFast& st1{ m_STLtreeFastVec[0] };

	st1.reset();

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success_upload, STATICSTRING::success_upload + STATICSTRING::success_uploadLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1, nullptr, 0);
}



void WEBSERVICE::testSuccessUpload()
{
	STLtreeFast& st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success_upload, STATICSTRING::success_upload + STATICSTRING::success_uploadLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1, nullptr, 0);
}



void WEBSERVICE::readyParseChunkData()
{
	m_parseStatus = PARSERESULT::begin_checkChunkData;

	parseReadData(m_readBuffer, bodyEnd - bodyBegin);
}




void WEBSERVICE::resetVerifyData()
{
	std::fill(m_verifyData.get(), m_verifyData.get() + VerifyDataPos::maxBufferSize, nullptr);
}










//这个函数仅供参考
/*

假设有一组数据库数据


# id, name, age, province, city, country, phone
'1', 'dengdanjun', '9', 'GuangDong', 'GuangZhou', 'TianHe', '13528223629'  char*


为了避免大量字符拷贝，以小代价指代每一个字符串，从数据库读取buffer回来时，仅记录首尾位置，存进二级指针数组中


idBegin,idEnd, nameBegin,nameEnd, ageBegin,ageEnd, provinceBegin,provinceEnd, cityBegin,cityEnd, countryBegin,countryEnd,
, phoneBegin,phoneEnd


char**

假设使用者明确这个接口查询数据，那么仅仅需要知道行数有多少，假设有10行，也就是10组以上数据


为了进一步缩减操作，这10组我们仅仅记录每组在char**数组的开始位置，存进三级指针数组


char***      为了避免排序中频繁跳位置，假设本次排序以age为排序准则，从小到大，那么只需要存


char**+4，（char**+4+7*2）   依次类推



接下来在三级指针数组中排序

						std::sort(sortBuf.get(), sortBuffer, [&indexLeft,&numLeft,&indexRight,&numRight](const char **left, const char **right)
			{
				if (!*left)
				{
					if (!*right)
						return false;
					else
						return true;
				}
				else
				{
					if (!*right)
						return false;
					indexLeft = indexRight = -1;
					numLeft = numRight = 1;
					return std::accumulate(std::make_reverse_iterator(*left + (*(left + 1) - *left)), std::make_reverse_iterator(*left), 0, [&indexLeft, &numLeft](auto &sum, auto const ch)
					{
						if (++indexLeft)numLeft *= 10;
						return sum += (ch - '0')*numLeft;
					})<
						std::accumulate(std::make_reverse_iterator(*right + (*(right + 1) - *right)), std::make_reverse_iterator(*right), 0, [&indexRight, &numRight](auto &sum, auto const ch)
					{
						if (++indexRight)numRight *= 10;
						return sum += (ch - '0')*numRight;
					});     //将里面的二级数组中的char*转换为数字比较

				}
			});

这样接下来遍历三级指针数组，每组减去4到接下来的14位置的指针位置就是每组的数据了



*/


void WEBSERVICE::handleERRORMESSAGE(ERRORMESSAGE em)
{
	switch (em)
	{
	case ERRORMESSAGE::SQL_QUERY_ERROR:
		startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
		break;

	case ERRORMESSAGE::SQL_NET_ASYNC_ERROR:
		startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
		break;

	case ERRORMESSAGE::SQL_MYSQL_RES_NULL:
		startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
		break;

	case ERRORMESSAGE::SQL_QUERY_ROW_ZERO:
		startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
		break;

	case ERRORMESSAGE::SQL_QUERY_FIELD_ZERO:
		startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
		break;

	case ERRORMESSAGE::SQL_RESULT_TOO_LAGGE:
		startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
		break;

	case ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR:
		startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		break;

	case ERRORMESSAGE::REDIS_ASYNC_READ_ERROR:
		startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		break;

	case ERRORMESSAGE::CHECK_REDIS_MESSAGE_ERROR:
		startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		break;


	case ERRORMESSAGE::REDIS_READY_QUERY_ERROR:
		startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		break;


	case ERRORMESSAGE::STD_BADALLOC:
		startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
		break;


	case ERRORMESSAGE::REDIS_ERROR:
		startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		break;


	default:
		startWrite(WEBSERVICEANSWER::result2unknown.data(), WEBSERVICEANSWER::result2unknown.size());
		break;

	}
}





void WEBSERVICE::clean()
{
	//cout << "start clean\n";
	if (!m_hasClean.load())
	{
		m_hasClean.store(true);
		m_log->writeLog(__FUNCTION__, __LINE__, m_serviceNum, m_IP, m_port, m_requestTime);


		m_startPos = 0;
		m_firstTime = 0;


		m_maxReadLen = m_defaultReadLen;
		recoverMemory();

		//将所有存储MYSQL_RES*的vector进行一次清理
		for (auto ptr : m_multiSqlRequestSWVec)
		{
			m_multiSqlReadSWMaster->FreeResult(std::get<2>(*ptr));
		}


		sslShutdownLoop();
	}
}



void WEBSERVICE::setRecvTrue()
{
	m_hasRecv.store(true);
}





void WEBSERVICE::sslShutdownLoop()
{
	m_buffer->getSSLSock()->async_shutdown([this](const boost::system::error_code& err)
	{
		//async_shutdown调用之后，无论成功与否，都可以进行下面的处理
		if (err)
		{
			m_log->writeLog(__FUNCTION__, __LINE__, err.value(), err.message());
		}
	});

	m_timeWheel->insert([this]() 
	{
		ec = {};
		m_buffer->getSSLSock()->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
		m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
	}, 10);
}



void WEBSERVICE::shutdownLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_buffer->getSSLSock()->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
		m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
	}
	else
	{
		m_buffer->getSSLSock()->lowest_layer().cancel(ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
}


void WEBSERVICE::cancelLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_buffer->getSSLSock()->lowest_layer().cancel(ec);
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
	else
	{
		m_buffer->getSSLSock()->lowest_layer().close(ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {closeLoop(); }, 5);
	}

}

void WEBSERVICE::closeLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_buffer->getSSLSock()->lowest_layer().close(ec);
		m_timeWheel->insert([this]() {closeLoop(); }, 5);
	}
	else
	{
		resetSocket();
	}
}

void WEBSERVICE::resetSocket()
{
	m_buffer->getSock().reset(new boost::asio::ip::tcp::socket(*m_ioc));
	m_buffer->getSock()->set_option(boost::asio::socket_base::keep_alive(true), ec);

	(*m_clearFunction)(m_mySelf);
}


void WEBSERVICE::recoverMemory()
{
	m_httpresult.resetheader();
	m_MemoryPool.prepare();
}


void WEBSERVICE::sendOK()
{
	startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
}



void WEBSERVICE::cleanData()
{
	if (m_availableLen)
	{
		m_buffer->getSSLSock()->async_read_some(boost::asio::buffer(m_readBuffer, m_availableLen >= m_maxReadLen ? m_maxReadLen : m_availableLen), [this](const boost::system::error_code& err, std::size_t size)
		{
			if (err)
			{
				//超时时clean函数会调用cancel,触发operation_aborted错误  修复发生错误时不会触发回收的情况
				if (err != boost::asio::error::operation_aborted)
				{
					clean();
				}
			}
			else
			{
				m_availableLen = m_buffer->getSSLSock()->lowest_layer().available();
				cleanData();
			}
		});
	}
	else
	{
		startWrite(m_sendBuffer, m_sendLen);
	}
}







void WEBSERVICE::prepare()
{
	m_httpHeaderMap.reset(new const char* [HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN]);
	std::fill(m_httpHeaderMap.get(), m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN, nullptr);

	/*
	m_MethodBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Method, m_MethodEnd = m_MethodBegin + 1;

	m_TargetBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Target, m_TargetEnd = m_TargetBegin + 1;

	m_VersionBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Version, m_VersionEnd = m_VersionBegin + 1;

	m_AcceptBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Accept, m_AcceptEnd = m_AcceptBegin + 1;

	m_Accept_CharsetBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Accept_Charset, m_Accept_CharsetEnd = m_Accept_CharsetBegin + 1;

	m_Accept_EncodingBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Accept_Encoding, m_Accept_EncodingEnd = m_Accept_EncodingBegin + 1;

	m_Accept_LanguageBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Accept_Language, m_Accept_LanguageEnd= m_Accept_LanguageBegin + 1;

	m_Accept_RangesBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Accept_Ranges, m_Accept_RangesEnd= m_Accept_RangesBegin + 1;

	m_AuthorizationBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Authorization, m_AuthorizationEnd= m_AuthorizationBegin + 1;

	m_Cache_ControlBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Cache_Control, m_Cache_ControlEnd= m_Cache_ControlBegin + 1;

	m_ConnectionBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Connection, m_ConnectionEnd= m_ConnectionBegin + 1;

	m_CookieBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Cookie, m_CookieEnd= m_CookieBegin + 1;

	m_Content_LengthBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Content_Length, m_Content_LengthEnd= m_Content_LengthBegin + 1;

	m_Content_TypeBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Content_Type, m_Content_TypeEnd= m_Content_TypeBegin + 1;

	m_DateBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Date, m_DateEnd= m_DateBegin + 1;

	m_ExpectBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Expect, m_ExpectEnd= m_ExpectBegin + 1;

	m_FromBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::From, m_FromEnd= m_FromBegin + 1;

	m_HostBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Host, m_HostEnd= m_HostBegin + 1;

	m_If_MatchBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::If_Match, m_If_MatchEnd= m_If_MatchBegin + 1;

	m_If_Modified_SinceBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::If_Modified_Since, m_If_Modified_SinceEnd= m_If_Modified_SinceBegin + 1;

	m_If_None_MatchBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::If_None_Match, m_If_None_MatchEnd= m_If_None_MatchBegin + 1;

	m_If_RangeBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::If_Range, m_If_RangeEnd= m_If_RangeBegin + 1;

	m_If_Unmodified_SinceBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::If_Unmodified_Since, m_If_Unmodified_SinceEnd= m_If_Unmodified_SinceBegin + 1;

	m_Max_ForwardsBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Max_Forwards, m_Max_ForwardsEnd= m_Max_ForwardsBegin + 1;

	m_PragmaBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Pragma, m_PragmaEnd= m_PragmaBegin + 1;

	m_Proxy_AuthorizationBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Proxy_Authorization, m_Proxy_AuthorizationEnd= m_Proxy_AuthorizationBegin + 1;

	m_RangeBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Range, m_RangeEnd= m_RangeBegin + 1;

	m_RefererBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Referer, m_RefererEnd= m_RefererBegin + 1;

	m_TEBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::TE, m_TEEnd= m_TEBegin + 1;

	m_UpgradeBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Upgrade, m_UpgradeEnd= m_UpgradeBegin + 1;

	m_User_AgentBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::User_Agent, m_User_AgentEnd= m_User_AgentBegin + 1;

	m_ViaBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Via, m_ViaEnd= m_ViaBegin + 1;

	m_WarningBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Warning, m_WarningEnd= m_WarningBegin + 1;

	m_BodyBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Body, m_BodyEnd = m_BodyBegin + 1;

	m_Transfer_EncodingBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Transfer_Encoding, m_Transfer_EncodingEnd = m_Transfer_EncodingBegin + 1;
	*/


	m_Boundary_ContentDispositionBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_ContentDisposition, m_Boundary_ContentDispositionEnd = m_Boundary_ContentDispositionBegin + 1;

	m_Boundary_NameBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_Name, m_Boundary_NameEnd = m_Boundary_NameBegin + 1;

	m_Boundary_FilenameBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_Filename, m_Boundary_FilenameEnd = m_Boundary_FilenameBegin + 1;

	m_Boundary_ContentTypeBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_ContentType, m_Boundary_ContentTypeEnd = m_Boundary_ContentTypeBegin + 1;

	m_HostNameBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::hostWebSite, m_HostNameEnd = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::hostWebSite + 1;

	m_HostPortBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::hostPort, m_HostPortEnd = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::hostPort + 1;





	m_MemoryPool.reset();


	int i = -1, j{}, z{};

	m_stringViewVec.resize(15, std::vector<std::string_view>{});
	m_mysqlResVec.resize(3, std::vector<MYSQL_RES*>{});
	m_unsignedIntVec.resize(15, std::vector<unsigned int>{});

	while (++i != 4)
	{
		m_STLtreeFastVec.emplace_back(STLtreeFast(&m_MemoryPool));
	}




	i = -1, j = -1, z = -1;
	while (++i != 3)
	{
		m_multiSqlRequestSWVec.emplace_back(std::make_shared<resultTypeSW>(m_stringViewVec[++j], 0, m_mysqlResVec[i], m_unsignedIntVec[++z], m_stringViewVec[++j], nullptr, m_unsignedIntVec[++z]));
		m_multiRedisRequestSWVec.emplace_back(std::make_shared<redisResultTypeSW>(m_stringViewVec[++j], 0, m_unsignedIntVec[++z], 0, m_stringViewVec[++j], m_unsignedIntVec[++z], nullptr, m_MemoryPool));
		m_multiRedisWriteSWVec.emplace_back(std::make_shared<redisWriteTypeSW>(m_stringViewVec[++j], 0, m_unsignedIntVec[++z]));
	}

	keyVec.resize(10, {});
}


void WEBSERVICE::handShake()
{
	m_buffer->getSSLSock()->async_handshake(boost::asio::ssl::stream_base::server,
		[this](const boost::system::error_code& err)
	{
		if (err)
		{
			if (err != boost::asio::error::operation_aborted)
			{
				m_log->writeLog(__FUNCTION__, __LINE__, err.value(), err.message(), m_IP, m_port);
				clean();
			}
		}
		else
		{
			startRead();
		}
	});
}




void WEBSERVICE::startRead()
{

	//如果Connection则发送http 响应后继续接收新消息，否则停止接收，等待回收到对象池
	if (keep_alive)
	{
		m_buffer->getSSLSock()->async_read_some(boost::asio::buffer(m_readBuffer + m_startPos, m_maxReadLen - m_startPos), [this](const boost::system::error_code& err, std::size_t size)
		{
			if (err)
			{
				if (err != boost::asio::error::operation_aborted)
				{
					//超时时clean函数会调用cancel,触发operation_aborted错误  修复发生错误时不会触发回收的情况
					clean();
				}
			}
			else
			{
				m_hasRecv.store(true);
				if (size > 0)
				{
					std::string_view message{ m_readBuffer + m_startPos, size };
					m_message.swap(message);

					parseReadData(m_readBuffer + m_startPos, size);
				}
				else
				{
					startRead();
				}
			}
		});
	}
	else
	{
		clean();
	}
}



//发生异常时还原置位，开启新的监听
//m_startPos 集中在外层设置，表示下次的起始读取位置
void WEBSERVICE::parseReadData(const char* source, const int size)
{
	ReadBuffer& refBuffer{ *m_buffer };

	m_parseStatus = parseHttp(source, size);


	try
	{
		switch (m_parseStatus)
		{
		case PARSERESULT::complete:
			m_startPos = 0;
			m_readBuffer = refBuffer.getBuffer();
			m_maxReadLen = m_defaultReadLen;
			//https://www.cnblogs.com/wmShareBlog/p/5924144.html
			//解决Expect:100-continue 问题			
			if (expect_continue)
			{
				return startWrite(HTTPRESPONSEREADY::http100Continue, HTTPRESPONSEREADY::http100ContinueLen);
			}
			checkMethod();
			break;


		case PARSERESULT::check_messageComplete:
			m_startPos = 0;
			checkMethod();
			break;


		case PARSERESULT::invaild:
			m_startPos = 0;
			m_readBuffer = refBuffer.getBuffer();
			m_maxReadLen = m_defaultReadLen;
			m_availableLen = refBuffer.getSSLSock()->lowest_layer().available();
			m_sendBuffer = HTTPRESPONSEREADY::http11invaild, m_sendLen = HTTPRESPONSEREADY::http11invaildLen;
			cleanData();
			break;

		case PARSERESULT::parseMultiPartFormData:
			readyParseMultiPartFormData();
			break;


		case PARSERESULT::parseChunk:
			readyParseChunkData();
			break;


		default:
			startRead();
			break;
		}
	}
	catch (const std::exception& e)
	{
		m_parseStatus = PARSERESULT::complete;
		m_startPos = 0;
		m_readBuffer = refBuffer.getBuffer();
		m_maxReadLen = m_defaultReadLen;
		m_availableLen = refBuffer.getSSLSock()->lowest_layer().available();
		m_sendBuffer = WEBSERVICEANSWER::result2stl.data(), m_sendLen = WEBSERVICEANSWER::result2stl.size();
		cleanData();
	}
}




//常规http解析对于分包处理是将数据append然后处理
//这里为了更快，每次解析不全时记录下当前状态位，下一次收到数据包时可以根据状态位恢复到最近的地方继续解析,参考状态机思想实现
//有需要才进行拷贝，没有的话只是记录下位置以备需要，同时兼顾性能与扩展，仅当关键数据段跨越两块内存区时才进行拷贝合成，否则直接用指针指向
//支持POST  GET
// 支持分包解析,可以一次发送多个http请求进来，每次进行接口处理后会接着上次解析完的位置继续解析
// 除了支持常规body  chunk解析和multipartFromdata两种上传格式解析
//支持pipeline，在每次发送完毕后判断有没有后续请求包需要解析处理  可查看startWriteLoop的相关处理
//在解析处理过程中对target、Query para以及body进行url转码处理，后续无需在转码处理，搜素UrlDecodeWithTransChinese函数即可定位处理代码
//UrlDecodeWithTransChinese可以在一次循环内同时转换url转义字符和中文字符转换

int WEBSERVICE::parseHttp(const char* source, const int size)
{
#define MAXMETHODLEN 7
#define MAXTARGETBEGINLEN 2
#define MAXTARGETLEN 256
#define MAXFIRSTBODYLEN 65535
#define MAXHTTPLEN 4
#define MAXVERSIONBEGINLEN 1
#define MAXVERSIONLEN 3
#define MAXLINELEN 5
#define MAXHEADBEGINLEN 1
#define MAXHEADLEN 50
#define MAXWORDBEGINLEN 20
#define MAXWORDLEN 1024
#define MAXCHUNKNUMBERLEN 6
#define MAXCHUNKNUMENDLEN 10
#define MAXCHUNKFinalEndLEN 4
#define MAXTHIRDLENLEN 2
#define MAXBOUNDARYHEADERNLEN 30
#define KEEPALIVELEN 10

	static const std::string HTTPStr{ "HTTP" };
	static const std::string http10{ "1.0" }, http11{ "1.1" };
	static const std::string lineStr{ "\r\n\r\n" };
	static const std::string PUTStr{ "PUT" }, GETStr{ "GET" };                                         // 3                
	static const std::string POSTStr{ "POST" }, HEADStr{ "HEAD" };                    // 4                     
	static const std::string TRACEStr{ "TRACE" };                                     // 5
	static const std::string DELETEStr{ "DELETE " };                                  // 6
	static const std::string CONNECTStr{ "CONNECT" }, OPTIONSStr{ "OPTIONS" };        // 7
	//针对每一个关键位置的长度作限制，避免遇到超长无效内容被浪费性能

	/////////////////////////////////////////////////////////////////////////////////////
	static const std::string Accept{ "accept" };
	static const std::string Accept_Charset{ "accept-charset" };
	static const std::string Accept_Encoding{ "accept-encoding" };
	static const std::string Accept_Language{ "accept-language" };
	static const std::string Accept_Ranges{ "accept-ranges" };
	static const std::string Authorization{ "authorization" };
	static const std::string Cache_Control{ "cache-control" };
	static const std::string Connection{ "connection" };
	static const std::string Cookie{ "cookie" };
	static const std::string Content_Type{ "content-type" };
	static const std::string Date{ "date" };
	static const std::string Expect{ "expect" };
	static const std::string From{ "from" };
	static const std::string Host{ "host" };
	static const std::string If_Match{ "if-match" };
	static const std::string If_Modified_Since{ "if-modified-since" };
	static const std::string If_None_Match{ "if-none-match" };
	static const std::string If_Range{ "if-range" };
	static const std::string If_Unmodified_Since{ "if-unmodified-since" };
	static const std::string Max_Forwards{ "max-forwards" };
	static const std::string Pragma{ "Pragma" };
	static const std::string Proxy_Authorization{ "proxy-authorization" };
	static const std::string Range{ "range" };
	static const std::string Referer{ "referer" };
	static const std::string Upgrade{ "upgrade" };
	static const std::string User_Agent{ "user-agent" };
	static const std::string Set_Cookie{ "set-cookie" };
	static const std::string Via{ "via" };
	static const std::string Warning{ "warning" };
	static const std::string TE{ "te" };
	static const std::string ContentLength{ "content-length" };
	static const std::string Transfer_Encoding{ "transfer-encoding" };
	static const std::string chunked{ "chunked" };
	static const std::string multipartform_data{ "multipart/form-data" };
	static const std::string boundary{ "boundary=" };


	static const std::string Content_Disposition{ "content-disposition" };
	static const std::string Name{ "name" };
	static const std::string Filename{ "filename" };
	static const std::string boundaryFinal{ "--\r\n" };



	///////////////////////////////////////////////////////////////////////////////////////
	int len{}, num{}, index{}, sum{};                   //累计body长度
	const char** headerPos{ m_httpHeaderMap.get() };
	char* newBuffer{}, * newBufferBegin{}, * newBufferEnd{}, * newBufferIter{};
	bool isDoubleQuotation{};   //判断boundaryWord首字符是否是\"

	const char* iterFindBegin{ source }, * iterFindEnd{ source + size }, * iterFinalEnd{ m_readBuffer + m_maxReadLen }, * iterFindThisEnd{};


	std::vector<std::pair<const char*, const char*>>& dataBufferVec{ m_dataBufferVec };
	std::vector<std::pair<const char*, const char*>>::const_iterator dataBufferVecBegin{}, dataBufferVecEnd{};

	//使用引用指向，减少反复解引用的消耗
	ASYNCLOG& reflog{ *m_log };
	ReadBuffer& refBuffer{ *m_buffer };
	MYREQVIEW& refMyReView{ refBuffer.getView() };


	switch (m_parseStatus)
	{
	case PARSERESULT::check_messageComplete:
	case PARSERESULT::invaild:
	case PARSERESULT::complete:
		if (isHttp)
			hostPort = 80;         //默认http  host端口
		else
			hostPort = 443;        //默认http  host端口
		refMyReView.clear();
		hasX_www_form_urlencoded = hasXml = hasJson = hasChunk = false;
		hasMultipart_form_data = expect_continue = keep_alive = hasCompress = hasDeflate = hasGzip = false;
		hasIdentity = keep_alive = true;
		m_bodyLen = 0;
		m_boundaryHeaderPos = 0;
		refMyReView.method() = 0;
		m_dataBufferVec.clear();
		break;


	case PARSERESULT::find_funBegin:
		goto find_funBegin;
		break;

	case find_funEnd:
		goto find_funEnd;
		break;

	case find_targetBegin:
		goto find_targetBegin;
		break;

	case PARSERESULT::find_targetEnd:
		goto find_targetEnd;
		break;

	case PARSERESULT::find_paraBegin:
		goto find_paraBegin;
		break;


	case PARSERESULT::find_paraEnd:
		goto find_paraEnd;
		break;

	case PARSERESULT::find_httpBegin:
		goto find_httpBegin;
		break;

	case PARSERESULT::find_httpEnd:
		goto find_httpEnd;
		break;

	case PARSERESULT::find_versionBegin:
		goto find_versionBegin;
		break;

	case PARSERESULT::find_versionEnd:
		goto find_versionEnd;
		break;

	case PARSERESULT::find_lineEnd:
		goto find_lineEnd;
		break;

	case PARSERESULT::find_headBegin:
		goto find_headBegin;
		break;

	case PARSERESULT::find_headEnd:
		goto find_headEnd;
		break;

	case PARSERESULT::find_wordBegin:
		goto find_wordBegin;
		break;

	case PARSERESULT::find_wordEnd:
		goto find_wordEnd;
		break;

	case PARSERESULT::find_finalBodyEnd:
		goto find_finalBodyEnd;
		break;


	case PARSERESULT::begin_checkBoundaryBegin:
		goto begin_checkBoundaryBegin;
		break;


	case PARSERESULT::begin_checkChunkData:
		goto begin_checkChunkData;
		break;


	case PARSERESULT::find_fistCharacter:
		goto find_fistCharacter;
		break;


	case PARSERESULT::find_chunkNumEnd:
		goto find_chunkNumEnd;
		break;


	case PARSERESULT::find_thirdLineEnd:
		goto find_thirdLineEnd;
		break;


	case PARSERESULT::find_chunkDataBegin:
		goto find_chunkDataBegin;
		break;


	case PARSERESULT::find_chunkDataEnd:
		goto find_chunkDataEnd;
		break;


	case PARSERESULT::find_fourthLineBegin:
		goto find_fourthLineBegin;
		break;


	case PARSERESULT::find_fourthLineEnd:
		goto find_fourthLineEnd;
		break;


	case PARSERESULT::find_boundaryBegin:
		goto find_boundaryBegin;
		break;


	case PARSERESULT::find_halfBoundaryBegin:
		goto find_halfBoundaryBegin;
		break;


	case PARSERESULT::find_boundaryHeaderBegin:
		goto find_boundaryHeaderBegin;
		break;


	case PARSERESULT::find_boundaryHeaderBegin2:
		goto find_boundaryHeaderBegin2;
		break;

	case PARSERESULT::find_nextboundaryHeaderBegin2:
		goto find_nextboundaryHeaderBegin2;
		break;


	case PARSERESULT::find_nextboundaryHeaderBegin3:
		goto find_nextboundaryHeaderBegin3;
		break;


	case PARSERESULT::find_boundaryHeaderEnd:
		goto find_boundaryHeaderEnd;
		break;


	case PARSERESULT::find_boundaryWordBegin:
		goto find_boundaryWordBegin;
		break;


	case PARSERESULT::find_boundaryWordBegin2:
		goto find_boundaryWordBegin2;
		break;


	case PARSERESULT::find_boundaryWordEnd:
		goto find_boundaryWordEnd;
		break;


	case PARSERESULT::find_boundaryWordEnd2:
		goto find_boundaryWordEnd2;
		break;


	case PARSERESULT::find_fileBegin:
		goto find_fileBegin;
		break;


	case PARSERESULT::find_fileBoundaryBegin:
		goto find_fileBoundaryBegin;
		break;


	case PARSERESULT::find_fileBoundaryBegin2:
		goto find_fileBoundaryBegin2;
		break;

	case PARSERESULT::check_fileBoundaryEnd:
		goto check_fileBoundaryEnd;
		break;


	default:
		break;
	}


	accumulateLen = 0;
	while (iterFindBegin != iterFindEnd)
	{
	find_funBegin:

		if (!isupper(*iterFindBegin))
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funBegin !isupper");
			return PARSERESULT::invaild;
		}

		funBegin = iterFindBegin;

		iterFindBegin = funBegin + 1, accumulateLen = 0;

	find_funEnd:
		// httpService.cpp    
		//  accumulateLen用于统计method累计搜索过的长度，
		// 如果当前搜索位置iterFindBegin到本次接收数据尾位置iterFindEnd的距离大于method的设置最大大小MAXMETHODLEN
		// 则执行如下操作：
		// 用当前位置 加上 剩余搜索长度 得到本次搜索结束位置iterFindThisEnd
		//  如果在本次搜索结束位置未搜索到结束标志，则判断非法请求

		// 反之，将本次接收数据尾位置iterFindEnd设置为iterFindThisEnd
		// 进行搜索，如果依然搜索不到，首先将本次搜索长度累加到accumulateLen中，
		// 如果本次接收数据尾位置不等于本次读取buffer最终位置，则
		// 将下次接收消息的位置设置为iterFindEnd的位置

		// 反之，表示本读取buffer已经塞满数据，
		// 如果用于保存位置对的vector  dataBufferVec是空的
		// 那么将funBegin, iterFindEnd存到dataBufferVec中
		// 反之将本次读取buffer整段存入
		// 然后从内存池获取一块新buffer作为新读取buffer继续接收消息


		// 当找到funEnd时，判断dataBufferVec是否为空，
		// 如果不为，则遍历获取所需装载的内存大小
		// 从内存池中获取对应大小buffer  copy  然后用指针指向首尾位置
		// 反之，直接用指针指向首尾位置
		// 这样在能够在一块读取buffer内读取完的特定数据段可以直接用指针指向，只有对于跨越不同读取内存块位置的数据段才需要进行一次copy，
		// 同时兼顾性能与扩展性

		len = std::distance(iterFindBegin, iterFindEnd);
		//检测该数据字段是否达到预设的最大值，防止恶意发送超长数据消耗服务器解析资源
		if (accumulateLen + len >= MAXMETHODLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXMETHODLEN - accumulateLen);

			funEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_not<>(), std::bind(isupper, std::placeholders::_1)));

			if (funEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd funEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{
			funEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_not<>(), std::bind(isupper, std::placeholders::_1)));

			if (funEnd == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					//如果本次接收数据末尾未到达本次使用的接收缓冲区的末尾，则将下次接收起点设为本次数据的终点位置
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_funEnd;
				}
				else
				{
					//往vector里插入不完整的数据断首尾位置
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(funBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));

					//从内存池申请一块新缓存用于接收新的数据
					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					//将下次使用的读取缓冲区切换成从内存池申请的新缓冲区
					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_funEnd;
				}
			}
		}

		if (*funEnd != ' ')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd *funEnd != ' '");
			return PARSERESULT::invaild;
		}

		if (dataBufferVec.empty())
		{
			//本次数据在一块内存中，不需要进行任何拷贝工作，直接用指针指向首尾位置
			finalFunBegin = funBegin, finalFunEnd = funEnd;
		}
		else
		{
			//因为dataBufferVec非空时，在存入的时候已经累加了长度，因此只需要加上std::distance(m_readBuffer, const_cast<char*>(funEnd)) 本次解析长度即可
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(funEnd));

			//从内存池获取一块内存块，用于存放本次解析数据段的完整数据
			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			//将vector里的数据和本次解析到的数据内容拷贝到从内存池获取到的内存块中，再用指针指向数据首尾位置
			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(funEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(funEnd));

			finalFunBegin = newBuffer, finalFunEnd = newBufferIter;

			dataBufferVec.clear();
		}



		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		//使用switch 判断http方法长度，再根据首字符，高效分离需要比对的http方法
		switch (std::distance(finalFunBegin, finalFunEnd))
		{
		case HTTPHEADER::THREE:
			switch (*finalFunBegin)
			{
			case 'P':
				if (!std::equal(finalFunBegin, finalFunEnd, PUTStr.cbegin(), PUTStr.cend()))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal PUTStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::PUT;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::THREE);
				break;
			case 'G':
				if (!std::equal(finalFunBegin, finalFunEnd, GETStr.cbegin(), GETStr.cend()))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal GETStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::GET;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::THREE);
				break;
			default:
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd THREE default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		case HTTPHEADER::FOUR:
			switch (*finalFunBegin)
			{
			case 'P':
				if (!std::equal(finalFunBegin, finalFunEnd, POSTStr.cbegin(), POSTStr.cend()))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal POSTStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::POST;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::FOUR);
				break;
			case 'H':
				if (!std::equal(finalFunBegin, finalFunEnd, HEADStr.cbegin(), HEADStr.cend()))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal HEADStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::HEAD;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::FOUR);
				break;
			default:
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd FOUR default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		case HTTPHEADER::FIVE:
			if (!std::equal(finalFunBegin, finalFunEnd, TRACEStr.cbegin(), TRACEStr.cend()))
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal TRACEStr");
				return PARSERESULT::invaild;
			}
			refMyReView.method() = METHOD::TRACE;
			refMyReView.setMethod(finalFunBegin, HTTPHEADER::FIVE);
			break;
		case HTTPHEADER::SIX:
			if (!std::equal(finalFunBegin, finalFunEnd, DELETEStr.cbegin(), DELETEStr.cend()))
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal DELETEStr");
				return PARSERESULT::invaild;
			}
			refMyReView.method() = METHOD::DELETE;
			refMyReView.setMethod(finalFunBegin, HTTPHEADER::SIX);
			break;
		case HTTPHEADER::SEVEN:
			switch (*finalFunBegin)
			{
			case 'C':
				if (!std::equal(finalFunBegin, finalFunEnd, CONNECTStr.cbegin(), CONNECTStr.cend()))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal CONNECTStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::CONNECT;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::SEVEN);
				break;
			case 'O':
				if (!std::equal(finalFunBegin, finalFunEnd, OPTIONSStr.cbegin(), OPTIONSStr.cend()))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal OPTIONSStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::OPTIONS;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::SEVEN);
				break;
			default:
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd SEVEN default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		default:
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_funEnd default");
			return PARSERESULT::invaild;
			break;
		}


		///////////////////////////////////  示例  参考以上来考虑
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		iterFindBegin = funEnd + 1;
		accumulateLen = 0;

	find_targetBegin:
		if (iterFindBegin == iterFindEnd)
		{
			if (iterFindEnd != iterFinalEnd)
			{
				m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
				return PARSERESULT::find_targetBegin;
			}
			else
			{
				newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

				if (!newBuffer)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_targetBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_targetBegin;
			}
		}

		if (*iterFindBegin == '\r')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_targetBegin *iterFindBegin == '\r'");
			return PARSERESULT::invaild;
		}


		targetBegin = iterFindBegin;
		iterFindBegin = targetBegin + 1;
		accumulateLen = 0;


		///////////////////////////////////////////////////////////////  begin时无需存储，begin到end才需要存储 ////////////////////////////////////////////////////

	find_targetEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXTARGETLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXTARGETLEN - accumulateLen);

			targetEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
				std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, '?'), std::bind(std::equal_to<>(), std::placeholders::_1, '\r'))));

			if (targetEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_targetEnd targetEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			targetEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
				std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, '?'), std::bind(std::equal_to<>(), std::placeholders::_1, '\r'))));

			if (targetEnd == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_targetEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(targetBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));

					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_targetEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_targetEnd;
				}
			}
		}


		if (*targetEnd == '\r')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_targetEnd *targetEnd == '\r'");
			return PARSERESULT::invaild;
		}



		if (dataBufferVec.empty())
		{
			//对target进行url转码和中文转换，此种中文转换只在charset=UTF-8下有用
			if (!UrlDecodeWithTransChinese(targetBegin, targetEnd - targetBegin, len))
				return PARSERESULT::invaild;

			finalTargetBegin = targetBegin, finalTargetEnd = targetBegin + len;
		}
		//如果很长，横跨了不同分段，那么进行整合再处理
		else
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(targetEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_targetEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(targetEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(targetEnd));

			//对target进行url转码和中文转换，此种中文转换只在charset=UTF-8下有用
			if (!UrlDecodeWithTransChinese(newBuffer, std::distance(newBuffer, newBufferIter), len))
				return PARSERESULT::invaild;

			finalTargetBegin = newBuffer, finalTargetEnd = newBuffer + len;

			dataBufferVec.clear();
		}



		refMyReView.setTarget(finalTargetBegin, finalTargetEnd - finalTargetBegin);
		accumulateLen = 0;


		//////////////////////////////////////////////////////////////////   target部分解析完毕  ////////////////////////////////////////////

		if (*targetEnd == '?')
		{
			iterFindBegin = targetEnd + 1;

		find_paraBegin:
			if (iterFindBegin == iterFindEnd)
			{
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_targetBegin;
				}
				else
				{
					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_paraBegin !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_targetBegin;
				}
			}

			if (*iterFindBegin == '\r')
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_paraBegin *iterFindBegin == '\r'");
				return PARSERESULT::invaild;
			}


			////////////////////////////////////////////////////////////   bodyBegin已经完成  /////////////////////////////////////////
			paraBegin = iterFindBegin;
			iterFindBegin = paraBegin + 1;
			accumulateLen = 0;

		find_paraEnd:
			len = std::distance(iterFindBegin, iterFindEnd);
			if (accumulateLen + len >= MAXFIRSTBODYLEN)
			{
				iterFindThisEnd = iterFindBegin + (MAXFIRSTBODYLEN - accumulateLen);

				paraEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
					std::bind(std::equal_to<>(), std::placeholders::_1, '\r')));

				if (paraEnd == iterFindThisEnd)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_paraEnd paraEnd == iterFindThisEnd");
					return PARSERESULT::invaild;
				}

			}
			else
			{

				paraEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
					std::bind(std::equal_to<>(), std::placeholders::_1, '\r')));

				if (paraEnd == iterFindEnd)
				{
					accumulateLen += len;
					if (iterFindEnd != iterFinalEnd)
					{
						m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
						return PARSERESULT::find_paraEnd;
					}
					else
					{
						if (dataBufferVec.empty())
							dataBufferVec.emplace_back(std::make_pair(paraBegin, iterFindEnd));
						else
							dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


						newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

						if (!newBuffer)
						{
							reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_paraEnd !newBuffer");
							return PARSERESULT::invaild;
						}

						m_readBuffer = newBuffer;
						m_startPos = 0;
						return PARSERESULT::find_paraEnd;
					}
				}
			}


			if (*paraEnd == '\r')
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_paraEnd *paraEnd == '\r'");
				return PARSERESULT::invaild;
			}


			if (dataBufferVec.empty())
			{

				finalParaBegin = paraBegin, finalParaEnd = paraEnd;
			}
			//如果很长，横跨了不同分段，那么进行整合再处理
			else
			{
				accumulateLen += std::distance(m_readBuffer, const_cast<char*>(paraEnd));

				newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

				if (!newBuffer)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_paraEnd !newBuffer");
					return PARSERESULT::invaild;
				}

				newBufferIter = newBuffer;

				for (auto const& singlePair : dataBufferVec)
				{
					std::copy(singlePair.first, singlePair.second, newBufferIter);
					newBufferIter += std::distance(singlePair.first, singlePair.second);
				}
				std::copy(m_readBuffer, const_cast<char*>(paraEnd), newBufferIter);
				newBufferIter += std::distance(m_readBuffer, const_cast<char*>(paraEnd));


				finalParaBegin = newBuffer, finalParaEnd = newBufferIter;

				dataBufferVec.clear();
			}


			m_httpresult.setpara(finalParaBegin, finalParaEnd);
		}

		///////////////////////////////////////////////////////////////////////////////   firstBody已完成  /////////////////////////////////

		iterFindBegin = (*targetEnd == ' ' ? targetEnd : paraEnd);
		accumulateLen = 0;
		if (*iterFindBegin == ' ')
			++iterFindBegin;


	find_httpBegin:
		if (iterFindBegin == iterFindEnd)
		{
			if (iterFindEnd != iterFinalEnd)
			{
				m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
				return PARSERESULT::find_httpBegin;
			}
			else
			{
				newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

				if (!newBuffer)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_httpBegin;
			}
		}

		if (*iterFindBegin != 'H')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpBegin *iterFindBegin != 'H'");
			return PARSERESULT::invaild;
		}


		httpBegin = iterFindBegin;
		iterFindBegin = httpBegin + 1;
		accumulateLen = 0;

		//////////////////////////////////////////////////////////////////////////////////////////////////////

	find_httpEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXHTTPLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXHTTPLEN - accumulateLen);

			httpEnd = std::find_if_not(iterFindBegin, iterFindThisEnd, std::bind(std::logical_and<>(), std::bind(greater_equal<>(), std::placeholders::_1, 'A'),
				std::bind(less_equal<>(), std::placeholders::_1, 'Z')));

			if (httpEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpEnd httpEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			httpEnd = std::find_if_not(iterFindBegin, iterFindEnd, std::bind(std::logical_and<>(), std::bind(greater_equal<>(), std::placeholders::_1, 'A'),
				std::bind(less_equal<>(), std::placeholders::_1, 'Z')));

			if (httpEnd == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_httpEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(httpBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_paraEnd;
				}
			}
		}


		if (*httpEnd != '/')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpEnd *httpEnd != '/'");
			return PARSERESULT::invaild;
		}


		if (dataBufferVec.empty())
		{
			finalHttpBegin = httpBegin, finalHttpEnd = httpEnd;
		}
		//如果很长，横跨了不同分段，那么进行整合再处理
		else
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(httpEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(httpEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(httpEnd));

			finalHttpBegin = newBuffer, finalHttpEnd = newBufferIter;

			dataBufferVec.clear();
		}


		if (!std::equal(finalHttpBegin, finalHttpEnd, HTTPStr.cbegin(), HTTPStr.cend()))
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_httpEnd !equal HTTPStr");
			return PARSERESULT::invaild;
		}

		iterFindBegin = httpEnd + 1;
		accumulateLen = 0;

		///////////////////////////////////////////////////////////////////////////////////////////////////////已完成

	find_versionBegin:
		if (iterFindBegin == iterFindEnd)
		{
			if (iterFindEnd != iterFinalEnd)
			{
				m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
				return PARSERESULT::find_versionBegin;
			}
			else
			{
				newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

				if (!newBuffer)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_versionBegin;
			}
		}


		if (*iterFindBegin != '1' && *iterFindBegin != '2')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionBegin *iterFindBegin != '1' && *iterFindBegin != '2'");
			return PARSERESULT::invaild;
		}

		versionBegin = iterFindBegin;
		iterFindBegin = versionBegin + 1;
		accumulateLen = 0;

		///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	find_versionEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXVERSIONLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXVERSIONLEN - accumulateLen);

			versionEnd = std::find(iterFindBegin, iterFindThisEnd, '\r');

			if (versionEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionEnd versionEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			versionEnd = std::find(iterFindBegin, iterFindEnd, '\r');

			if (versionEnd == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_versionEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(versionBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_versionEnd;
				}
			}
		}


		if (*versionEnd != '\r')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionEnd *versionEnd != '\r'");
			return PARSERESULT::invaild;
		}


		if (dataBufferVec.empty())
		{
			finalVersionBegin = versionBegin, finalVersionEnd = versionEnd;
		}
		//如果很长，横跨了不同分段，那么进行整合再处理
		else
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(versionEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(versionEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(versionEnd));

			finalVersionBegin = newBuffer, finalVersionEnd = newBufferIter;

			dataBufferVec.clear();
		}



		if (*finalVersionBegin != '1' || *(finalVersionBegin + 1) != '.')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionEnd !equal http11 HTTP11");
			return PARSERESULT::invaild;
		}

		switch (*(finalVersionBegin + 2))
		{
		case '1':
			isHttp11 = true;
			isHttp10 = false;
			break;
		case '0':
			isHttp10 = true;
			isHttp11 = false;
			break;
		default:
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_versionEnd !equal http11 HTTP11");
			return PARSERESULT::invaild;
			break;
		}


		refMyReView.setVersion(finalVersionBegin, finalVersionEnd - finalVersionBegin);

		iterFindBegin = lineBegin = versionEnd;
		accumulateLen = 0;


	find_lineEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXLINELEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXLINELEN - accumulateLen);

			lineEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'),
				std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')));

			if (lineEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_lineEnd lineEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			lineEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'),
				std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')));

			if (lineEnd == iterFindEnd)
			{
				if (!m_bodyLen)
				{
					if (dataBufferVec.empty())
					{
						if (std::distance(lineBegin, lineEnd) == 4)
						{
							messageBegin = lineEnd;
							goto check_messageComplete;
						}
					}
					else
					{
						if (std::distance(dataBufferVec.front().first, dataBufferVec.front().second) + std::distance(m_readBuffer, const_cast<char*>(lineEnd)) == 4)
						{
							messageBegin = lineEnd;
							goto check_messageComplete;
						}
					}
				}
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_lineEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(lineBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_lineEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_lineEnd;
				}
			}
		}


		if (dataBufferVec.empty())
		{
			finalLineBegin = lineBegin, finalLineEnd = lineEnd;
		}
		else
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(lineEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_lineEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(lineEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(lineEnd));

			finalLineBegin = newBuffer, finalLineEnd = newBufferIter;

			dataBufferVec.clear();
		}



		len = distance(finalLineBegin, finalLineEnd);

		accumulateLen = 0;

		break;
	}

	while (len == 2)
	{
		if (!std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cbegin() + 2))
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_lineEnd len ==2 !equal lineStr");
			return PARSERESULT::invaild;
		}

		iterFindBegin = lineEnd;

	find_headBegin:

		headBegin = iterFindBegin;
		iterFindBegin = headBegin + 1;
		accumulateLen = 0;


	find_headEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXHEADLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXHEADLEN - accumulateLen);

			headEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ':'),
				std::bind(std::equal_to<>(), std::placeholders::_1, '\r')));

			if (headEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_headEnd headEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			headEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ':'),
				std::bind(std::equal_to<>(), std::placeholders::_1, '\r')));

			if (headEnd == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_headEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(headBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_headEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_headEnd;
				}
			}
		}


		if (*headEnd == '\r')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_headEnd *headEnd == '\r'");
			return PARSERESULT::invaild;
		}



		if (dataBufferVec.empty())
		{
			finalHeadBegin = headBegin, finalHeadEnd = headEnd;
		}
		else
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(headEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_headEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(headEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(headEnd));

			finalHeadBegin = newBuffer, finalHeadEnd = newBufferIter;

			dataBufferVec.clear();
		}



		iterFindBegin = headEnd + 1;
		accumulateLen = 0;


		///////////////////////////////////////////        先进行一部分http 头部解析，后期有性能问题再看看需不需要改状态机实现



		////////////////////////////////////////////////////////

	find_wordBegin:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXWORDBEGINLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXWORDBEGINLEN - accumulateLen);

			wordBegin = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

			if (wordBegin == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_wordBegin wordBegin == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			wordBegin = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

			if (wordBegin == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_wordBegin;
				}
				else
				{
					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_wordBegin !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_wordBegin;
				}
			}
		}


		//有些http 字段允许为空，比如Host
		if (*wordBegin == '\r')
		{
			finalWordBegin = wordBegin;
			wordEnd = finalWordEnd = finalWordBegin;
			goto set_httpHeader;
		}

		////////////////////////////////////////////////////////////////
		iterFindBegin = wordBegin + 1;
		accumulateLen = 0;

	find_wordEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXWORDLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXWORDLEN - accumulateLen);

			wordEnd = std::find(iterFindBegin, iterFindThisEnd, '\r');

			if (wordEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_wordEnd wordEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			wordEnd = find(iterFindBegin, iterFindEnd, '\r');

			if (wordEnd == iterFindEnd)
			{
				accumulateLen += len;
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_wordEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(wordBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_wordEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_wordEnd;
				}
			}
		}



		if (dataBufferVec.empty())
		{
			finalWordBegin = wordBegin, finalWordEnd = wordEnd;
		}
		//如果很长，横跨了不同分段，那么进行整合再处理
		else
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(wordEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_wordEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(wordEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(wordEnd));

			finalWordBegin = newBuffer, finalWordEnd = newBufferIter;

			dataBufferVec.clear();
		}


	set_httpHeader:

		switch (std::distance(finalHeadBegin, finalHeadEnd))
		{
		case HTTPHEADER::TWO:
			//"TE"
			if (std::equal(finalHeadBegin, finalHeadEnd, TE.cbegin(), TE.cend(),
				std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
			{
				if (!m_httpresult.isTEEmpty())
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_TEBegin is not nullptr");
					return PARSERESULT::invaild;
				}

				m_httpresult.setTE(finalWordBegin, finalWordEnd);

			}

			break;

		case HTTPHEADER::THREE:
			//"Via"
			if (std::equal(finalHeadBegin, finalHeadEnd, Via.cbegin(), Via.cend(),
				std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
			{
				if (!m_httpresult.isViaEmpty())
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_ViaBegin is not nullptr");
					return PARSERESULT::invaild;
				}

				m_httpresult.setVia(finalWordBegin, finalWordEnd);


			}

			break;

		case HTTPHEADER::FOUR:
			switch (*finalHeadBegin)
			{
			case 'd':
			case 'D':
				//"Date"
				if (std::equal(finalHeadBegin, finalHeadEnd, Date.cbegin(), Date.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isDateEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_DateBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setDate(finalWordBegin, finalWordEnd);

				}

				break;

			case 'f':
			case 'F':
				//"From"
				if (std::equal(finalHeadBegin, finalHeadEnd, From.cbegin(), From.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isFromEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_FromBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setFrom(finalWordBegin, finalWordEnd);


				}

				break;

			case 'h':
			case 'H':
				//"Host"
				if (std::equal(finalHeadBegin, finalHeadEnd, Host.cbegin(), Host.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isHostEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_HostBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setHost(finalWordBegin, finalWordEnd);


				}

				break;


			default:

				break;
			}
			break;

		case HTTPHEADER::FIVE:
			//"Range"
			if (std::equal(finalHeadBegin, finalHeadEnd, Range.cbegin(), Range.cend(),
				std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
			{
				if (!m_httpresult.isRangeEmpty())
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_RangeBegin is not nullptr");
					return PARSERESULT::invaild;
				}

				m_httpresult.setRange(finalWordBegin, finalWordEnd);

			}

			break;

		case HTTPHEADER::SIX:
			switch (*finalHeadBegin)
			{
			case 'a':
			case 'A':
				//"Accept"
				if (std::equal(finalHeadBegin, finalHeadEnd, Accept.cbegin(), Accept.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isAcceptEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_AcceptBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setAccept(finalWordBegin, finalWordEnd);


				}

				break;

			case 'c':
			case 'C':
				//"Cookie"
				if (std::equal(finalHeadBegin, finalHeadEnd, Cookie.cbegin(), Cookie.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isCookieEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_CookieBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setCookie(finalWordBegin, finalWordEnd);


				}

				break;

			case 'e':
			case 'E':
				//"Expect"
				if (std::equal(finalHeadBegin, finalHeadEnd, Expect.cbegin(), Expect.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isExpectEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_ExpectBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setExpect(finalWordBegin, finalWordEnd);


				}

				break;

			case 'p':
			case 'P':
				//"Pragma"
				if (std::equal(finalHeadBegin, finalHeadEnd, Pragma.cbegin(), Pragma.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isPragmaEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_PragmaBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setPragma(finalWordBegin, finalWordEnd);


				}

				break;

			default:


				break;
			}
			break;

		case HTTPHEADER::SEVEN:
			switch (*finalHeadBegin)
			{
			case 'r':
			case 'R':
				//"Referer"
				if (std::equal(finalHeadBegin, finalHeadEnd, Referer.cbegin(), Referer.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isRefererEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_RefererBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setReferer(finalWordBegin, finalWordEnd);


				}

				break;

			case 'u':
			case 'U':
				//"Upgrade"
				if (std::equal(finalHeadBegin, finalHeadEnd, Upgrade.cbegin(), Upgrade.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isUpgradeEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_UpgradeBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setUpgrade(finalWordBegin, finalWordEnd);


				}

				break;

			case 'w':
			case 'W':
				//"Warning"
				if (std::equal(finalHeadBegin, finalHeadEnd, Warning.cbegin(), Warning.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isWarningEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_WarningBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setWarning(finalWordBegin, finalWordEnd);


				}

				break;

			default:

				break;
			}
			break;


		case HTTPHEADER::EIGHT:
			switch (*(finalHeadBegin + 3))
			{
			case 'm':
			case 'M':
				//"If-Match"
				if (std::equal(finalHeadBegin, finalHeadEnd, If_Match.cbegin(), If_Match.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isIf_MatchEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_If_MatchBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setIf_Match(finalWordBegin, finalWordEnd);


				}

				break;

			case 'r':
			case 'R':
				//"If-Range"
				if (std::equal(finalHeadBegin, finalHeadEnd, If_Range.cbegin(), If_Range.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isIf_RangeEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_If_RangeBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setIf_Range(finalWordBegin, finalWordEnd);

				}

				break;

			default:

				break;
			}
			break;

		case HTTPHEADER::TEN:
			switch (*finalHeadBegin)
			{
			case 'c':
			case 'C':
				// "Connection"
				if (std::equal(finalHeadBegin, finalHeadEnd, Connection.cbegin(), Connection.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isConnectionEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_ConnectionBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setConnection(finalWordBegin, finalWordEnd);


				}

				break;

			case 'U':
			case 'u':
				//"User-Agent"
				if (std::equal(finalHeadBegin, finalHeadEnd, User_Agent.cbegin(), User_Agent.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isUser_AgentEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_ConnectionBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setUser_Agent(finalWordBegin, finalWordEnd);

				}

				break;
			case 'S':
			case 's':
				//Set-Cookie
				//可能有多个的情况出现
				if (std::equal(finalHeadBegin, finalHeadEnd, Set_Cookie.cbegin(), Set_Cookie.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{

					m_httpresult.setSetCookie(finalWordBegin, finalWordEnd);

				}

				break;

			default:

				break;
			}
			break;

		case HTTPHEADER::TWELVE:
			switch (*finalHeadBegin)
			{
			case 'c':
			case 'C':
				//"Content-Type"
				if (std::equal(finalHeadBegin, finalHeadEnd, Content_Type.cbegin(), Content_Type.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isContent_TypeEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Content_TypeBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setContent_Type(finalWordBegin, finalWordEnd);


				}

				break;

			case 'm':
			case 'M':
				//"Max-Forwards"
				if (std::equal(finalHeadBegin, finalHeadEnd, Max_Forwards.cbegin(), Max_Forwards.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isMax_ForwardsEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Max_ForwardsBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setMax_Forwards(finalWordBegin, finalWordEnd);


				}

				break;

			default:

				break;
			}
			break;

		case HTTPHEADER::THIRTEEN:
			switch (*(finalHeadBegin + 1))
			{
			case 'c':
			case 'C':
				//"Accept-Ranges"
				if (std::equal(finalHeadBegin, finalHeadEnd, Accept_Ranges.cbegin(), Accept_Ranges.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isAccept_RangesEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Accept_RangesBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setAccept_Ranges(finalWordBegin, finalWordEnd);


				}

				break;

			case 'u':
			case 'U':
				//"Authorization"
				if (std::equal(finalHeadBegin, finalHeadEnd, Authorization.cbegin(), Authorization.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isAuthorizationEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_AuthorizationBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setAuthorization(finalWordBegin, finalWordEnd);


				}

				break;

			case 'a':
			case 'A':
				// "Cache-Control"
				if (std::equal(finalHeadBegin, finalHeadEnd, Cache_Control.cbegin(), Cache_Control.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isCache_ControlEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Cache_ControlBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setCache_Control(finalWordBegin, finalWordEnd);


				}

				break;

			case 'f':
			case 'F':
				// "If-None-Match"
				if (std::equal(finalHeadBegin, finalHeadEnd, If_None_Match.cbegin(), If_None_Match.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isIf_None_MatchEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_If_None_MatchBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setIf_None_Match(finalWordBegin, finalWordEnd);


				}

				break;

			default:

				break;
			}
			break;

		case HTTPHEADER::FOURTEEN:
			switch (*finalHeadBegin)
			{
			case 'a':
			case 'A':
				//"Accept-Charset"
				if (std::equal(finalHeadBegin, finalHeadEnd, Accept_Charset.cbegin(), Accept_Charset.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isAccept_CharsetEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_If_None_MatchBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setAccept_Charset(finalWordBegin, finalWordEnd);


				}

				break;

			case 'c':
			case 'C':
				//"Content-Length"

				if (std::equal(finalHeadBegin, finalHeadEnd, ContentLength.cbegin(), ContentLength.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isContent_LengthEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Content_LengthBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setContent_Length(finalWordBegin, finalWordEnd);


				}

				break;

			default:

				break;
			}

			break;

		case HTTPHEADER::FIFTEEN:
			switch (*(finalHeadBegin + 7))
			{
			case 'e':
			case 'E':
				//"Accept-Encoding"
				if (std::equal(finalHeadBegin, finalHeadEnd, Accept_Encoding.cbegin(), Accept_Encoding.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isAccept_EncodingEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Accept_EncodingBegin  is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setAccept_Encoding(finalWordBegin, finalWordEnd);

				}

				break;

			case 'l':
			case 'L':
				//"Accept-Language"
				if (std::equal(finalHeadBegin, finalHeadEnd, Accept_Language.cbegin(), Accept_Language.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isAccept_LanguageEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Accept_LanguageBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setAccept_Language(finalWordBegin, finalWordEnd);


				}

				break;

			default:

				break;
			}


			break;

		case HTTPHEADER::SEVENTEEN:
			switch (*finalHeadBegin)
			{
			case 'i':
			case 'I':
				// "If-Modified-Since"
				if (std::equal(finalHeadBegin, finalHeadEnd, If_Modified_Since.cbegin(), If_Modified_Since.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isIf_Modified_SinceEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_If_Modified_SinceBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setIf_Modified_Since(finalWordBegin, finalWordEnd);

				}

				break;

			case 't':
			case 'T':
				//Transfer-Encoding
				if (std::equal(finalHeadBegin, finalHeadEnd, Transfer_Encoding.cbegin(), Transfer_Encoding.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isTransfer_EncodingEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Transfer_EncodingBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setTransfer_Encoding(finalWordBegin, finalWordEnd);

				}

				break;

			default:

				break;
			}

		case HTTPHEADER::NINETEEN:
			switch (*finalHeadBegin)
			{
			case 'i':
			case 'I':
				// "If-Unmodified-Since"
				if (std::equal(finalHeadBegin, finalHeadEnd, If_Unmodified_Since.cbegin(), If_Unmodified_Since.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isIf_Unmodified_SinceEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_If_Unmodified_SinceBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setIf_Unmodified_Since(finalWordBegin, finalWordEnd);


				}

				break;

			case 'p':
			case 'P':
				// "Proxy-Authorization"
				if (std::equal(finalHeadBegin, finalHeadEnd, Proxy_Authorization.cbegin(), Proxy_Authorization.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (!m_httpresult.isProxy_AuthorizationEmpty())
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Proxy_AuthorizationBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					m_httpresult.setProxy_Authorization(finalWordBegin, finalWordEnd);


				}

				break;

			default:

				break;
			}
			break;


		default:

			break;
		}


		//////////////////////////////////////////////////////////////////
	before_findSecondLineEnd:
		lineBegin = iterFindBegin = wordEnd;
		accumulateLen = 0;

	find_secondLineEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXLINELEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXLINELEN - accumulateLen);

			lineEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'),
				std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')));

			if (lineEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd lineEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

		}
		else
		{

			lineEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'),
				std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')));

			if (lineEnd == iterFindEnd)
			{

				accumulateLen += len;

				if (len == 4)
				{
					finalLineBegin = lineBegin, finalLineEnd = lineEnd;
					goto check_len;
				}
				if (iterFindEnd != iterFinalEnd)
				{
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_secondLineEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(lineBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

					if (!newBuffer)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_secondLineEnd;
				}
			}
		}


		if (dataBufferVec.empty())
		{
			finalLineBegin = lineBegin, finalLineEnd = lineEnd;
		}
		//如果很长，横跨了不同分段，那么进行整合再处理
		else
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(lineEnd));

			newBuffer = m_MemoryPool.getMemory<char*>(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const& singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(lineEnd), newBufferIter);
			newBufferIter += std::distance(m_readBuffer, const_cast<char*>(lineEnd));

			finalLineBegin = newBuffer, finalLineEnd = newBufferIter;

			dataBufferVec.clear();
		}


		len = std::distance(finalLineBegin, finalLineEnd);
	}


	//http1.1必需带host头
	if (len != 4)
	{
		reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd len != 2 && len != 4");
		return PARSERESULT::invaild;
	}


check_len:
	//在这里进入解析http  header字段函数
	if (!parseHttpHeader())
		return PARSERESULT::invaild;


	//在content-lengeh有长度时首先考虑是否是MultiPartFormData文件上传格式，之后才考虑常规body格式
	//无长度时考虑是否是chunk
	{
		if (!std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cend()))
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "len == 4 !std::equal lineStr");
			return PARSERESULT::invaild;
		}


		if (m_bodyLen)
		{
			if (!m_httpresult.isContent_TypeEmpty())
			{
				std::string_view Content_TypeView{ m_httpresult.getContent_Type() };
				if (*Content_TypeView.cbegin() == 'm' && (boundaryBegin = std::search(Content_TypeView.cbegin(), Content_TypeView.cend(), boundary.cbegin(), boundary.cend())) != Content_TypeView.cend())
				{
					*(m_verifyData.get() + VerifyDataPos::customPos1Begin) = lineEnd;
					*(m_verifyData.get() + VerifyDataPos::customPos1End) = iterFindEnd;
					boundaryBegin += boundary.size();
					boundaryEnd = Content_TypeView.cend();
					return PARSERESULT::parseMultiPartFormData;
				}
			}


			iterFindBegin = lineEnd;


		find_finalBodyEnd:
			if (std::distance(iterFindBegin, iterFindEnd) >= m_bodyLen)
			{

				finalBodyBegin = iterFindBegin, finalBodyEnd = iterFindBegin + m_bodyLen;
				m_httpresult.setBody(finalBodyBegin, finalBodyEnd);
				messageBegin = iterFindBegin + m_bodyLen;
				goto check_messageComplete;
			}
			else
			{

				char* newReadBuffer{ m_MemoryPool.getMemory<char*>(m_bodyLen) };
				if (!newReadBuffer)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_finalBodyEnd !newReadBuffer");
					return PARSERESULT::invaild;
				}

				std::copy(iterFindBegin, iterFindEnd, newReadBuffer);

				m_startPos = std::distance(iterFindBegin, iterFindEnd);
				finalBodyBegin = m_readBuffer = newReadBuffer;
				m_maxReadLen = m_bodyLen;
				return PARSERESULT::find_finalBodyEnd;
			}
		}
		else if (hasChunk)
		{
			bodyBegin = lineEnd;
			bodyEnd = iterFindEnd;

			return PARSERESULT::parseChunk;


		begin_checkChunkData:
			m_readBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

			if (!m_readBuffer)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "begin_checkChunkData !m_readBuffer");
				return PARSERESULT::invaild;
			}

			///////////////////////////////////////////////////////

			//  https://blog.csdn.net/u014558668/article/details/70141956
			//chunk编码格式如下：
			//[chunk size][\r\n][chunk data][\r\n][chunk size][\r\n][chunk data][\r\n][chunk size = 0][\r\n][\r\n]
			while (true)
			{
				m_chunkLen = 0;

			find_fistCharacter:
				if (iterFindBegin == iterFindEnd)
					return PARSERESULT::find_fistCharacter;


				if (!std::isalnum(*iterFindBegin))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_fistCharacter !std::isalnum");
					return PARSERESULT::invaild;
				}

				chunkNumBegin = iterFindBegin;
				chunkNumEnd = chunkNumBegin + 1;

			find_chunkNumEnd:
				if (chunkNumEnd == iterFindEnd)
				{
					m_startPos = std::distance(chunkNumBegin, chunkNumEnd);
					if (chunkNumBegin != m_readBuffer)
					{
						std::copy(chunkNumBegin, chunkNumEnd, m_readBuffer);
						chunkNumBegin = m_readBuffer, chunkNumEnd = chunkNumBegin + m_startPos;
					}
					return PARSERESULT::find_chunkNumEnd;
				}


				len = std::distance(chunkNumBegin, iterFindEnd);
				if (len >= MAXCHUNKNUMBERLEN)
				{
					iterFindThisEnd = chunkNumBegin + MAXCHUNKNUMBERLEN;

					if ((chunkNumEnd = std::find_if_not(chunkNumEnd, iterFindThisEnd, std::bind(::isalnum, std::placeholders::_1))) == iterFindThisEnd)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_chunkNumEnd std::find_if_not");
						return PARSERESULT::invaild;
					}
				}
				else
				{
					if ((chunkNumEnd = std::find_if_not(chunkNumEnd, iterFindEnd, std::bind(::isalnum, std::placeholders::_1))) == iterFindEnd)
					{
						m_startPos = len;
						if (chunkNumBegin != m_readBuffer)
						{
							std::copy(chunkNumBegin, chunkNumEnd, m_readBuffer);
							chunkNumBegin = m_readBuffer, chunkNumEnd = chunkNumBegin + m_startPos;
						}
						return PARSERESULT::find_chunkNumEnd;
					}
				}


				if (*chunkNumEnd != '\r')
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_chunkNumEnd *chunkNumEnd != '\r'");
					return PARSERESULT::invaild;
				}


				index = -1, num = 1;
				m_chunkLen = std::accumulate(std::make_reverse_iterator(chunkNumEnd), std::make_reverse_iterator(chunkNumBegin), 0, [&index, &num](auto& sum, auto const ch)
				{
					if (++index)num *= 16;
					return sum += (std::isdigit(ch) ? ch - '0' : std::isupper(ch) ? (ch - 'A') : (ch - 'a')) * num;
				});



			find_thirdLineEnd:
				// \r\n   \r\n\r\n
				len = (m_chunkLen ? 2 : 4);
				index = std::distance(chunkNumEnd, iterFindEnd);
				if (index < len)
				{
					if (chunkNumEnd != m_readBuffer)
					{
						std::copy(chunkNumEnd, iterFindEnd, m_readBuffer);
						chunkNumEnd = m_readBuffer;
					}
					m_startPos = index;
					return PARSERESULT::find_thirdLineEnd;
				}


				if (!std::equal(lineStr.cbegin(), lineStr.cbegin() + len, chunkNumEnd, chunkNumEnd + len))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_thirdLineEnd !std::equal lineStr");
					return PARSERESULT::invaild;
				}

				if (!m_chunkLen)
				{
					messageBegin = chunkNumEnd + len;
					goto check_messageComplete;
				}

				chunkDataBegin = chunkNumEnd + len;

			find_chunkDataBegin:
				if (chunkDataBegin == iterFindEnd)
				{
					m_startPos = 0;
					chunkDataBegin = m_readBuffer;
					return PARSERESULT::find_chunkDataBegin;
				}


			find_chunkDataEnd:
				len = std::distance(chunkDataBegin, iterFindEnd);
				if (len < m_chunkLen)
				{
					//  chunkDataBegin, iterFindEnd  为接收的数据，需要保存
					m_chunkLen -= len;
					m_startPos = 0;
					chunkDataBegin = m_readBuffer;
					return PARSERESULT::find_chunkDataEnd;
				}

				//  chunkDataBegin, chunkDataEnd  为接收的数据，需要保存
				chunkDataEnd = chunkDataBegin + m_chunkLen;



			find_fourthLineBegin:
				if (chunkDataEnd == iterFindEnd)
				{
					m_startPos = 0;
					chunkDataEnd = m_readBuffer;
					return PARSERESULT::find_fourthLineBegin;
				}


			find_fourthLineEnd:
				len = std::distance(chunkDataEnd, iterFindEnd);
				if (len < 2)
				{
					if (chunkDataEnd != m_readBuffer)
					{
						std::copy(chunkDataEnd, iterFindEnd, m_readBuffer);
						chunkDataEnd = m_readBuffer;
					}
					m_startPos = len;
					return PARSERESULT::find_fourthLineEnd;
				}


				if (!std::equal(chunkDataEnd, chunkDataEnd + 2, lineStr.cbegin(), lineStr.cbegin() + 2))
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_fourthLineEnd !std::equal lineStr");
					return PARSERESULT::invaild;
				}


				iterFindBegin = chunkDataEnd + 2;

			}



		}
		messageBegin = finalLineEnd;
		goto check_messageComplete;
	}



	// 表单上传解析  multipart/form-data
	// 首先定位boundary开始位置   --加上boundary内容
	// 之后寻找 \r\n--加上boundary内容
begin_checkBoundaryBegin:
	m_readBuffer = m_MemoryPool.getMemory<char*>(m_maxReadLen);

	if (!m_readBuffer)
	{
		reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "begin_checkBoundaryBegin !m_readBuffer");
		return PARSERESULT::invaild;
	}

	m_boundaryHeaderPos = 0;
	*m_Boundary_NameBegin = *m_Boundary_FilenameBegin = *m_Boundary_ContentTypeBegin = *m_Boundary_ContentDispositionBegin = nullptr;
	findBoundaryBegin = iterFindBegin;

find_boundaryBegin:
	if (findBoundaryBegin == iterFindEnd)
	{
		m_startPos = 0;
		m_bodyLen -= iterFindEnd - iterFindBegin;
		if (m_bodyLen <= 0)
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryBegin m_bodyLen <= 0");
			return PARSERESULT::invaild;
		}
		findBoundaryBegin = m_readBuffer;
		return PARSERESULT::find_boundaryBegin;
	}

	if (*findBoundaryBegin != *boundaryBegin)
	{
		reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryBegin *findBoundaryBegin != *m_boundaryBegin");
		return PARSERESULT::invaild;
	}


find_halfBoundaryBegin:
	if (m_bodyLen <= m_boundaryLen)
	{
		reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_halfBoundaryBegin m_bodyLen <= m_boundaryLen");
		return PARSERESULT::invaild;
	}

	len = std::distance(findBoundaryBegin, iterFindEnd);
	if (len < m_boundaryLen)
	{
		m_bodyLen -= iterFindEnd - iterFindBegin;
		if (m_bodyLen <= 0)
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_halfBoundaryBegin m_bodyLen <= 0");
			return PARSERESULT::invaild;
		}
		if (findBoundaryBegin != m_readBuffer)
		{
			std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
			findBoundaryBegin = m_readBuffer;
		}
		m_startPos = len;
		return PARSERESULT::find_halfBoundaryBegin;
	}


	if (!std::equal(findBoundaryBegin, findBoundaryBegin + m_boundaryLen, boundaryBegin, boundaryEnd))
	{
		reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_halfBoundaryBegin !std::equal m_boundaryBegin");
		return PARSERESULT::invaild;
	}

	//如果相等

	findBoundaryBegin += m_boundaryLen;

	m_boundaryLen += 2;
	boundaryBegin -= 2;


find_boundaryHeaderBegin:
	len = std::distance(findBoundaryBegin, iterFindEnd);
	if (len < 2)
	{
		m_bodyLen -= iterFindEnd - iterFindBegin;
		if (m_bodyLen <= 0)
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderBegin m_bodyLen <= 0");
			return PARSERESULT::invaild;
		}
		if (findBoundaryBegin != m_readBuffer)
		{
			std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
			findBoundaryBegin = m_readBuffer;
		}
		m_startPos = len;
		return PARSERESULT::find_boundaryHeaderBegin;
	}


	findBoundaryBegin += 2;

	while (m_bodyLen - std::distance(findBoundaryBegin, iterFindEnd) > 0)
	{
		boundaryHeaderBegin = findBoundaryBegin;


		do
		{
		find_boundaryHeaderBegin2:
			if ((boundaryHeaderBegin = std::find_if(boundaryHeaderBegin, iterFindEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '))) == iterFindEnd)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderBegin2 m_bodyLen <= 0");
					return PARSERESULT::invaild;
				}
				m_startPos = 0;
				boundaryHeaderBegin = m_readBuffer;
				return PARSERESULT::find_boundaryHeaderBegin2;
			}


			boundaryHeaderEnd = boundaryHeaderBegin;
		find_boundaryHeaderEnd:
			len = std::distance(boundaryHeaderEnd, iterFindEnd);
			iterFindThisEnd = len >= MAXBOUNDARYHEADERNLEN ? (boundaryHeaderEnd + MAXBOUNDARYHEADERNLEN) : iterFindEnd;

			if ((boundaryHeaderEnd = std::find_if(boundaryHeaderEnd, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ':'), std::bind(std::equal_to<>(), std::placeholders::_1, '=')))) == iterFindEnd)
			{
				if (std::distance(boundaryHeaderBegin, boundaryHeaderEnd) >= MAXBOUNDARYHEADERNLEN)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderEnd >= MAXBOUNDARYHEADERNLEN");
					return PARSERESULT::invaild;
				}
				else
				{
					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderEnd m_bodyLen <= 0");
						return PARSERESULT::invaild;
					}
					if (boundaryHeaderBegin != m_readBuffer)
					{
						std::copy(boundaryHeaderBegin, boundaryHeaderEnd, m_readBuffer);
						boundaryHeaderEnd = m_readBuffer + std::distance(boundaryHeaderBegin, boundaryHeaderEnd);
						boundaryHeaderBegin = m_readBuffer;
					}
					m_startPos = len;
					return PARSERESULT::find_boundaryHeaderEnd;
				}
			}

			// 4  8  12  19  
			switch (std::distance(boundaryHeaderBegin, boundaryHeaderEnd))
			{
			case HTTPBOUNDARYHEADERLEN::NameLen:
				if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Name.cbegin(), Name.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (*m_Boundary_NameBegin)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Boundary_NameBegin is not nullptr");
						return PARSERESULT::invaild;
					}
					m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::NameLen;
				}
				break;
			case HTTPBOUNDARYHEADERLEN::FilenameLen:
				if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Filename.cbegin(), Filename.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (*m_Boundary_FilenameBegin)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Boundary_FilenameBegin is not nullptr");
						return PARSERESULT::invaild;
					}
					m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::FilenameLen;
				}
				break;
			case HTTPBOUNDARYHEADERLEN::Content_TypeLen:
				if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Content_Type.cbegin(), Content_Type.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (*m_Boundary_ContentTypeBegin)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Boundary_ContentTypeBegin is not nullptr");
						return PARSERESULT::invaild;
					}
					m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::Content_TypeLen;
				}
				break;
			case HTTPBOUNDARYHEADERLEN::Content_DispositionLen:
				if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Content_Disposition.cbegin(), Content_Disposition.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (*m_Boundary_ContentDispositionBegin)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "m_Boundary_ContentDispositionBegin is not nullptr");
						return PARSERESULT::invaild;
					}
					m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::Content_DispositionLen;
				}
				break;
			default:
				break;
			}


			boundaryWordBegin = boundaryHeaderEnd + 1;


		find_boundaryWordBegin:
			if ((boundaryWordBegin = std::find_if(boundaryWordBegin, iterFindEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '))) == iterFindEnd)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordBegin m_bodyLen <= 0");
					return PARSERESULT::invaild;
				}
				m_startPos = 0;
				boundaryWordBegin = m_readBuffer;
				return PARSERESULT::find_boundaryWordBegin;
			}



			if (*boundaryWordBegin == '\"')
			{
				isDoubleQuotation = true;
				++boundaryWordBegin;
			}
			else
			{
				isDoubleQuotation = false;
			}


		find_boundaryWordBegin2:
			if (boundaryWordBegin == iterFindEnd)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordBegin2 m_bodyLen <= 0");
					return PARSERESULT::invaild;
				}
				m_startPos = 0;
				boundaryWordBegin = m_readBuffer;
				return PARSERESULT::find_boundaryWordBegin2;
			}

			thisBoundaryWordBegin = boundaryWordEnd = boundaryWordBegin;


			if (isDoubleQuotation)
			{
			find_boundaryWordEnd:
				if ((boundaryWordEnd = std::find(boundaryWordEnd, iterFindEnd, '\"')) == iterFindEnd)
				{
					len = std::distance(thisBoundaryWordBegin, boundaryWordEnd);
					if (len == m_maxReadLen)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd len == m_maxReadLen");
						return PARSERESULT::invaild;
					}
					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd m_bodyLen <= 0");
						return PARSERESULT::invaild;
					}
					if (thisBoundaryWordBegin != m_readBuffer)
					{
						std::copy(thisBoundaryWordBegin, boundaryWordEnd, m_readBuffer);
						boundaryWordEnd = m_readBuffer + std::distance(thisBoundaryWordBegin, boundaryWordEnd);
						thisBoundaryWordBegin = m_readBuffer;
					}
					m_startPos = len;
					return PARSERESULT::find_boundaryWordEnd;
				}

				thisBoundaryWordEnd = boundaryWordEnd;
			}


		find_boundaryWordEnd2:
			if ((boundaryWordEnd = std::find_if(boundaryWordEnd, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ';'), std::bind(std::equal_to<>(), std::placeholders::_1, '\r')))) == iterFindEnd)
			{
				len = std::distance(thisBoundaryWordBegin, boundaryWordEnd);
				if (isDoubleQuotation)
				{
					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 m_bodyLen <= 0");
						return PARSERESULT::invaild;
					}
					// 修复此处再次读取时thisBoundaryWordBegin失效问题
					// 来到这里的时候thisBoundaryWordBegin肯定已经有值，thisBoundaryWordEnd也肯定已经有值，于是拷贝后在这里再次设置确保thisBoundaryWordBegin和thisBoundaryWordEnd不失效
					if (thisBoundaryWordBegin != m_readBuffer)
					{
						std::copy(thisBoundaryWordBegin, boundaryWordEnd, m_readBuffer);
						thisBoundaryWordEnd = m_readBuffer + std::distance(thisBoundaryWordBegin, thisBoundaryWordEnd);
						boundaryWordEnd = m_readBuffer + std::distance(thisBoundaryWordBegin, boundaryWordEnd);
						thisBoundaryWordBegin = m_readBuffer;
					}
					m_startPos = len;
					return PARSERESULT::find_boundaryWordEnd2;
				}
				else
				{
					if (len == m_maxReadLen)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 len == m_maxReadLen");
						return PARSERESULT::invaild;
					}
					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 m_bodyLen <= 0 2");
						return PARSERESULT::invaild;
					}
					if (thisBoundaryWordBegin != m_readBuffer)
					{
						std::copy(thisBoundaryWordBegin, iterFindEnd, m_readBuffer);
						boundaryWordEnd = m_readBuffer + std::distance(thisBoundaryWordBegin, boundaryWordEnd);
						thisBoundaryWordBegin = m_readBuffer;
					}
					// 修复当本次内容不足时thisBoundaryWordBegin指向失效问题
					m_startPos = len;
					return PARSERESULT::find_boundaryWordEnd2;
				}
			}


			if (!isDoubleQuotation)
				thisBoundaryWordEnd = boundaryWordEnd;


			len = std::distance(thisBoundaryWordBegin, thisBoundaryWordEnd);

			if (len)
			{
				newBuffer = m_MemoryPool.getMemory<char*>(len);

				if (!newBuffer)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 !newBuffer");
					return PARSERESULT::invaild;
				}

				std::copy(thisBoundaryWordBegin, thisBoundaryWordEnd, newBuffer);

				thisBoundaryWordBegin = newBuffer;
				thisBoundaryWordEnd = newBuffer + len;
			}
			else
			{
				thisBoundaryWordBegin = nullptr;
				thisBoundaryWordEnd = nullptr;
			}


			switch (m_boundaryHeaderPos)
			{
			case HTTPBOUNDARYHEADERLEN::NameLen:
				*m_Boundary_NameBegin = thisBoundaryWordBegin;
				*m_Boundary_NameEnd = thisBoundaryWordEnd;
				break;
			case HTTPBOUNDARYHEADERLEN::FilenameLen:
				*m_Boundary_FilenameBegin = thisBoundaryWordBegin;
				*m_Boundary_FilenameEnd = thisBoundaryWordEnd;
				break;
			case HTTPBOUNDARYHEADERLEN::Content_TypeLen:
				*m_Boundary_ContentTypeBegin = thisBoundaryWordBegin;
				*m_Boundary_ContentTypeEnd = thisBoundaryWordEnd;
				break;
			case HTTPBOUNDARYHEADERLEN::Content_DispositionLen:
				*m_Boundary_ContentDispositionBegin = thisBoundaryWordBegin;
				*m_Boundary_ContentDispositionEnd = thisBoundaryWordEnd;
				break;
			default:
				break;
			}


		find_nextboundaryHeaderBegin:
			if (*boundaryWordEnd == ';')
			{
				boundaryHeaderBegin = boundaryWordEnd + 1;
			}
			else if (*boundaryWordEnd == '\r')
			{
			find_nextboundaryHeaderBegin2:
				len = std::distance(boundaryWordEnd, iterFindEnd);
				if (len < 2)
				{
					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_nextboundaryHeaderBegin m_bodyLen <= 0");
						return PARSERESULT::invaild;
					}
					if (boundaryWordEnd != m_readBuffer)
					{
						std::copy(boundaryWordEnd, iterFindEnd, m_readBuffer);
						boundaryWordEnd = m_readBuffer;
					}
					m_startPos = len;
					return PARSERESULT::find_nextboundaryHeaderBegin2;
				}

				boundaryHeaderBegin = boundaryWordEnd + 2;
			}


		find_nextboundaryHeaderBegin3:
			if (boundaryHeaderBegin == iterFindEnd)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_nextboundaryHeaderBegin3 m_bodyLen <= 0");
					return PARSERESULT::invaild;
				}
				m_startPos = 0;
				boundaryHeaderBegin = m_readBuffer;
				return PARSERESULT::find_nextboundaryHeaderBegin3;
			}

		} while (*boundaryHeaderBegin != '\r');

	find_fileBegin:
		len = std::distance(boundaryHeaderBegin, iterFindEnd);
		if (len < 2)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin;
			if (m_bodyLen <= 0)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_fileBegin m_bodyLen <= 0");
				return PARSERESULT::invaild;
			}

			if (boundaryHeaderBegin != m_readBuffer)
			{
				std::copy(boundaryHeaderBegin, iterFindEnd, m_readBuffer);
				boundaryHeaderBegin = m_readBuffer;
			}
			m_startPos = len;
			return PARSERESULT::find_fileBegin;
		}

		boundaryHeaderBegin += 2;

		findBoundaryBegin = boundaryHeaderBegin;




		do
		{
		find_fileBoundaryBegin:
			if ((findBoundaryBegin = std::find(findBoundaryBegin, iterFindEnd, *boundaryBegin)) == iterFindEnd)
			{
				//boundaryHeaderBegin 到 findBoundaryBegin 为文件内容


				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_fileBoundaryBegin m_bodyLen <= 0 1");
					return PARSERESULT::invaild;
				}
				m_startPos = 0;
				findBoundaryBegin = m_readBuffer;
				return PARSERESULT::find_fileBoundaryBegin;
			}

		find_fileBoundaryBegin2:
			if (std::distance(findBoundaryBegin, iterFindEnd) < m_boundaryLen)
			{
				//boundaryHeaderBegin 到 findBoundaryBegin  为文件内容

				len = std::distance(findBoundaryBegin, iterFindEnd);
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "find_fileBoundaryBegin m_bodyLen <= 0 2");
					return PARSERESULT::invaild;
				}
				if (findBoundaryBegin != m_readBuffer)
				{
					std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
					findBoundaryBegin = m_readBuffer;
				}
				m_startPos = len;
				return PARSERESULT::find_fileBoundaryBegin2;
			}

			if (std::equal(findBoundaryBegin, findBoundaryBegin + m_boundaryLen, boundaryBegin, boundaryEnd))
			{
				//boundaryHeaderBegin 到 findBoundaryBegin 为文件内容

				findBoundaryBegin += m_boundaryLen;
				break;
			}
			else
			{
				++findBoundaryBegin;
			}

		} while (findBoundaryBegin != iterFindEnd);



		m_boundaryHeaderPos = 0;
		*m_Boundary_NameBegin = *m_Boundary_FilenameBegin = *m_Boundary_ContentTypeBegin = *m_Boundary_ContentDispositionBegin = nullptr;

		//校验

	check_fileBoundaryEnd:
		if (findBoundaryBegin == iterFindEnd)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin;
			if (m_bodyLen <= 0)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "check_fileBoundaryEnd m_bodyLen <= 0");
				return PARSERESULT::invaild;
			}
			m_startPos = 0;
			findBoundaryBegin = m_readBuffer;
			return PARSERESULT::check_fileBoundaryEnd;
		}

		if (*findBoundaryBegin != '-' && *findBoundaryBegin != '\r')
		{
			reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "check_fileBoundaryEnd *findBoundaryBegin != '-' && *findBoundaryBegin != '\r'");
			return PARSERESULT::invaild;
		}


	check_fileBoundaryEnd2:
		if ((len = std::distance(findBoundaryBegin, iterFindEnd)) < 4)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin;
			if (m_bodyLen <= 0)
			{
				reflog.writeLog(__FUNCTION__, __LINE__, m_message, m_parseStatus, "check_fileBoundaryEnd2 m_bodyLen <= 0");
				return PARSERESULT::invaild;
			}
			if (findBoundaryBegin != m_readBuffer)
			{
				std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
				findBoundaryBegin = m_readBuffer;
			}
			m_startPos = len;
			return PARSERESULT::check_fileBoundaryEnd2;
		}


		if (m_bodyLen - std::distance(iterFindBegin, findBoundaryBegin) == 4)
		{
			if (!equal(findBoundaryBegin, findBoundaryBegin + 4, boundaryFinal.cbegin(), boundaryFinal.cend()))
				return PARSERESULT::invaild;
			messageBegin = findBoundaryBegin + 4;
			goto check_messageComplete;
		}

		findBoundaryBegin += 2;
	}


	// 检查是否有额外信息
check_messageComplete:

	if (messageBegin == iterFindEnd)
		return PARSERESULT::complete;
	messageEnd = iterFindEnd;
	return PARSERESULT::check_messageComplete;
}


//解析http header字段内容
bool WEBSERVICE::parseHttpHeader()
{
	//HTTP/1.1：‌必须存在Host字段‌，缺失则返回400 Bad Request错误‌1；
	//HTTP / 1.0：允许缺失（旧协议无强制要求）‌
	const char* iter1Begin{}, * iter1End{}, * iter2Begin{}, * iter2End{};
	const char* strBegin{}, * strEnd{};
	int index{}, num{}, sum{};

	static const std::string gzip{ "gzip" }, chunked{ "chunked" }, compress{ "compress" }, deflate{ "deflate" }, identity{ "identity" };
	static const std::string keepalive{ "keep-alive" }, closeStr{ "close" };
	static const std::string Expect100_continue{ "100-continue" };
	static const std::string json{ "json" }, xml{ "xml" }, x_www_form_urlencoded{ "x-www-form-urlencoded" }, multipart_form_data{ "form-data" };

	//字段解析基本完成，比workflow还多解析了字段
	//可以去看workflow的http_parser.c 中的__check_message_header函数比对，后面再加上Content-Type解析吧，



	//解析Host字段内容
	if (m_httpresult.isHostEmpty())
	{
		*m_HostNameBegin = *m_HostNameEnd = *m_HostPortBegin = *m_HostPortEnd = nullptr;
		if (isHttp11)
			return false;
	}
	else
	{
		std::string_view HostView{ m_httpresult.getHost() };
		strBegin = HostView.cbegin();
		strEnd = HostView.cend();
		//Host字段允许为空
		if (std::distance(strBegin, strEnd))
		{
			iter1Begin = strBegin;
			//斜杠（/）属于 URI 路径部分，‌禁止出现在 Host 值中
			iter1End = std::find_if(iter1Begin, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ':'), std::bind(std::equal_to<>(), std::placeholders::_1, '/')));
			//判断请求的目标服务标识是否为空  比如:80  没有前面的网址信息  HTTP / 1.0：允许缺失
			if (iter1Begin == iter1End)
			{
				*m_HostNameBegin = *m_HostNameEnd = *m_HostPortBegin = *m_HostPortEnd = nullptr;
				if (isHttp11)
					return false;
			}
			if (iter1End != strEnd)
			{
				//斜杠（/）属于 URI 路径部分，‌禁止出现在 Host 值中
				if (*iter1End == '/')
				{
					*m_HostNameBegin = *m_HostNameEnd = *m_HostPortBegin = *m_HostPortEnd = nullptr;
					return false;
				}
				iter2Begin = iter1End + 1;
				iter2End = strEnd;
				index = -1, num = 1, sum = 0;
				//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
				if (!std::all_of(std::make_reverse_iterator(iter2End), std::make_reverse_iterator(iter2Begin), [&index, &num, &sum](const char ch)
				{
					if (!std::isdigit(ch))
					{
						return false;
					}
					if (++index)
						num *= 10;
					sum += (ch - '0') * num;
					return true;
				}))
				{
					*m_HostNameBegin = *m_HostNameEnd = *m_HostPortBegin = *m_HostPortEnd = nullptr;
					return false;
				}
				//检测端口是否异常
				if (sum < 0 || sum>65535)
				{
					*m_HostNameBegin = *m_HostNameEnd = *m_HostPortBegin = *m_HostPortEnd = nullptr;
					return false;
				}
				hostPort = sum;

				//记录下来，方便有问题时查看字符串内容
				*m_HostPortBegin = iter2Begin;
				*m_HostPortEnd = iter2End;
			}

			*m_HostNameBegin = iter1Begin;
			*m_HostNameEnd = iter1End;
		}
		else
		{
			//HTTP/1.1强制要求请求必须包含Host字段，缺失或值为空均为非法
			*m_HostNameBegin = *m_HostNameEnd = *m_HostPortBegin = *m_HostPortEnd = nullptr;
			if (isHttp11)
				return false;
		}
	}


	//‌HTTP协议无强制要求‌
	//RFC 7231等核心规范仅定义User-Agent为‌可选请求头字段‌，未强制服务器必须解析或处理69。服务器可忽略该字段而不违反协议标准


	//解析Transfer_Encoding
	//‌编码值必须小写
	//逗号后需加空格
	if (!m_httpresult.isTransfer_EncodingEmpty())
	{
		std::string_view Transfer_EncodingView{ m_httpresult.getTransfer_Encoding() };
		strBegin = Transfer_EncodingView.cbegin();
		strEnd = Transfer_EncodingView.cend();

		while (strBegin != strEnd)
		{
			iter1Begin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));
			if (iter1Begin == strEnd)
				break;

			iter1End = std::find_if(iter1Begin + 1, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
					std::bind(std::equal_to<>(), std::placeholders::_1, ',')));

			switch (std::distance(iter1Begin, iter1End))
			{
			case 4:
				if (!std::equal(iter1Begin, iter1End, gzip.cbegin(), gzip.cend()))
					return false;
				hasGzip = true;
				break;
			case 7:
				switch (*iter1Begin)
				{
				case 'c':
					if (!std::equal(iter1Begin + 1, iter1End, chunked.cbegin() + 1, chunked.cend()))
						return false;
					hasChunk = true;
					break;
				case 'd':
					if (!std::equal(iter1Begin + 1, iter1End, deflate.cbegin() + 1, deflate.cend()))
						return false;
					hasDeflate = true;
					break;
				default:
					return false;
				}
				break;
			case 8:
				switch (*iter1Begin)
				{
				case 'c':
					if (!std::equal(iter1Begin + 1, iter1End, compress.cbegin() + 1, compress.cend()))
						return false;
					hasCompress = true;
					break;
				case 'i':
					if (!std::equal(iter1Begin + 1, iter1End, identity.cbegin() + 1, identity.cend()))
						return false;
					hasIdentity = true;
					break;
				default:
					return false;
				}
				break;
			default:
				return false;
				break;
			}

			if (iter1End == strEnd)
				break;
			if (*iter1End == ' ')
			{
				iter1End = std::find(iter1End + 1, strEnd, ',');
				if (iter1End == strEnd)
					break;
				//逗号后需加空格
				++iter1End;
				if (iter1End == strEnd)
					break;
				if (*iter1End != ' ')
					return false;
				strBegin = iter1End;
				continue;
			}

			++iter1End;
			if (iter1End == strEnd)
				break;
			//逗号后需加空格
			if (*iter1End != ' ')
				return false;
			strBegin = iter1End + 1;

		}
	}


	//解析Connection,目前仅处理close 和 keep-alive
	//因为代理控制指令和非标准扩展指令都是自定义的
	//会遍历内容查找是否存在close 或者 keep-alive，找到则跳出解析
	if (!m_httpresult.isConnectionEmpty())
	{
		std::string_view ConnectionView{ m_httpresult.getConnection() };
		strBegin = ConnectionView.cbegin();
		strEnd = ConnectionView.cend();

		//字段值标准指令必须全小写
		while (strBegin != strEnd)
		{
			iter1Begin = std::find_if(strBegin, strEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));
			if (iter1Begin == strEnd)
				break;

			iter1End = std::find_if(iter1Begin + 1, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
					std::bind(std::equal_to<>(), std::placeholders::_1, ',')));

			switch (std::distance(iter1Begin, iter1End))
			{
			case 5:
				if (std::equal(iter1Begin, iter1End, closeStr.cbegin(), closeStr.cend()))
					keep_alive = false;
				goto after_parse_Connection;
			case 10:
				if (std::equal(iter1Begin, iter1End, keepalive.cbegin(), keepalive.cend()))
					keep_alive = true;
				goto after_parse_Connection;
				break;
			default:
				break;
			}

			if (iter1End == strEnd)
				break;

			iter1End = std::find_if(iter1End + 1, strEnd, std::bind(::islower, std::placeholders::_1));
			if (iter1End == strEnd)
				break;

			strBegin = iter1End;
		}
	}
	else
	{
		//http 1.1默认开启持久连接
		//http 1.0需在请求头中显式声明
		if (isHttp10)
			keep_alive = false;
		else if (isHttp11)
			keep_alive = true;
	}

after_parse_Connection:
	;

	//解析Expect头部
	if (!m_httpresult.isExpectEmpty())
	{
		std::string_view ExpectView{ m_httpresult.getExpect() };
		if (std::equal(ExpectView.cbegin(), ExpectView.cend(), Expect100_continue.cbegin(), Expect100_continue.cend()))
			expect_continue = true;
	}


	//解析Content-Length 长度
	if (!m_httpresult.isContent_LengthEmpty())
	{
		std::string_view Content_LengthView{ m_httpresult.getContent_Length() };
		strBegin = Content_LengthView.cbegin();
		strEnd = Content_LengthView.cend();

		index = -1, num = 1, sum = 0;
		//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
		if (!std::all_of(std::make_reverse_iterator(strEnd), std::make_reverse_iterator(strBegin), [&index, &num, &sum](const char ch)
		{
			if (!std::isdigit(ch))
				return false;
			if (++index)
				num *= 10;
			sum += (ch - '0') * num;
			return true;
		}))
			return false;
		m_bodyLen = sum;
	}


	//解析Content-Type类型   
	//应用程序请求服务器只需要考虑
	// application/json
	//‌application/xml‌
	//application/x-www-form-urlencoded
	//multipart/form-data     文件上传功能还需要测试
	if (!m_httpresult.isContent_TypeEmpty())
	{
		std::string_view Content_TypeView{ m_httpresult.getContent_Type() };
		strBegin = Content_TypeView.cbegin();
		strEnd = Content_TypeView.cend();
		//字段值标准指令必须全小写
		while (strBegin != strEnd)
		{
			iter1Begin = std::find(strBegin, strEnd, '/');
			if (iter1Begin == strEnd)
				break;

			++iter1Begin;

			iter1End = std::find_if(iter1Begin, strEnd,
				std::bind(std::logical_or<bool>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
					std::bind(std::equal_to<>(), std::placeholders::_1, ',')));



			switch (std::distance(iter1Begin, iter1End))
			{
			case 3:
				if (!std::equal(iter1Begin, iter1End, xml.cbegin(), xml.cend()))
					return false;
				hasXml = true;
				goto after_parse_Content_Type;
				break;
			case 4:
				if (!std::equal(iter1Begin, iter1End, json.cbegin(), json.cend()))
					return false;
				hasJson = true;
				goto after_parse_Content_Type;
				break;
			case 9:
				if (!std::equal(iter1Begin, iter1End, multipart_form_data.cbegin(), multipart_form_data.cend()))
					return false;
				hasMultipart_form_data = true;
				goto after_parse_Content_Type;
				break;
			case 21:
				if (!std::equal(iter1Begin, iter1End, x_www_form_urlencoded.cbegin(), x_www_form_urlencoded.cend()))
					return false;
				hasX_www_form_urlencoded = true;
				goto after_parse_Content_Type;
				break;
			default:
				return false;
				break;
			}
		}
	}


after_parse_Content_Type:
	;


	return true;
}




//后台登录接口
void WEBSERVICE::loginBackGround()
{
	if (m_loginBackTime >= 3)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	if (m_httpresult.isBodyEmpty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::answer, STATICSTRING::answerLen))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	const char** BodyBuffer{ m_buffer->bodyPara() };

	//对于需要反复使用的场景，使用keyVec里面的string_view来存储参数解析结果
	std::string_view& keyView{ keyVec[0] };
	keyView=std::string_view( *(BodyBuffer),*(BodyBuffer + 1) - *(BodyBuffer) );

	if (keyView.empty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };

	//获取后台密码
	//从redis获取密码进行比对，再做后续操作，避免SQL注入，可以根据需要进行加盐操作
	static std::string_view bgp{ "bgp" };

	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		command.emplace_back(bgp);
		commandSize.emplace_back(2);

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handleloginBackGround, this, std::placeholders::_1, std::placeholders::_2);


		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
			startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
	}

}



void WEBSERVICE::handleloginBackGround(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		if (!resultVec.empty())
		{
			std::string_view& keyView{ keyVec[0] };

			STLtreeFast& st1{ m_STLtreeFastVec[0] };

			st1.reset();

			if (!st1.clear())
				return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());


			//返回{"result:"1","web":""}  
			//读取web的值实现跳转页面
			static std::string_view zero{ "0" }, web{ "web" }, result{ "result" }, one{"1"};
			if (std::equal(keyView.cbegin(), keyView.cend(), resultVec[0].cbegin(), resultVec[0].cend()))
			{
				//密码比对正确
				hasLoginBack = true;
				m_loginBackTime = 0;

				if (!st1.put(result.cbegin(), result.cend(), one.cbegin(), one.cend()))
					return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

				if (!st1.put(web.cbegin(), web.cend(), zero.cbegin(), zero.cend()))
					return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
			}
			else
			{
				++m_loginBackTime;

				//密码比对错误
				return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

			}

			makeSendJson(st1);
		}
		else
		{
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		}
	}
	else
	{
		handleERRORMESSAGE(em);
	}

}


//退出网页管理后台
void WEBSERVICE::exitBack()
{
	hasLoginBack = false;

	if (m_fileVec->size() < 2)
		return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

	return startWrite((*m_fileVec)[1].data(), (*m_fileVec)[1].size());
}



//用户注册接口1，填写手机号发送验证码
//测试功能，目前先在管理后台实现，
//首先进行权限校验，以防止被滥用
//限制了中国公网ip才能进行访问之后，
//在验证码有效期内存储ip和手机号的内存消耗可以控制在很小的消耗范围内
void WEBSERVICE::registration1()
{
	if (!hasLoginBack)
		return startWrite(WEBSERVICEANSWER::result4.data(), WEBSERVICEANSWER::result4.size());

	if (m_httpresult.isBodyEmpty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::phone, STATICSTRING::phoneLen))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	const char** BodyBuffer{ m_buffer->bodyPara() };

	//对于需要反复使用的场景，使用keyVec里面的string_view来存储参数解析结果
	std::string_view& phoneView{ keyVec[0] };
	phoneView = std::string_view(*(BodyBuffer), *(BodyBuffer + 1) - *(BodyBuffer));

	//判断是否是非法手机号
	if (!REGEXFUNCTION::isVaildPhone(phoneView))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view& ipView{ keyVec[1] };
	ipView = std::string_view(m_IP.data(), m_IP.size());

	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };

	//获取IP地址与手机号是否存在

	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::mget, REDISNAMESPACE::mgetLen));
		command.emplace_back(phoneView);
		command.emplace_back(ipView);
		commandSize.emplace_back(3);

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handleregistration1, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
	}
	catch (const std::exception& e)
	{
		return startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
	}
}


void WEBSERVICE::handleregistration1(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		if (resultVec.size() == 2)
		{
			//如果IP地址或手机号记录不为空
			//都为空的情况下，生成验证码，通过验证码模块下发到用户手机，使用setnx 设置IP地址 与 手机号记录，有效时间为1小时
			//其中IP地址记录值为1 手机号记录为X位验证码    
			//  IP地址记录表示 在1小时内，已经发送过验证码
			// 手机号记录表示 在1小时内，已经发送过验证码
			// 


			std::string_view& phoneView{ keyVec[0] };
			std::string_view& ipView{ keyVec[1] };
			
			//如果手机号  与   ip同时为空记录
			//IP记录设置为3表示已下发验证码
			//手机号填入验证码方便验证
			if (resultVec[0].empty() && resultVec[1].empty())
			{
				char* buf = m_MemoryPool.getMemory<char*>(6);

				if(!buf)
					return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
			
				m_randomCode->generate(buf, 6);

				std::string_view& verifyCodeView{ keyVec[2] };
				verifyCodeView = std::string_view(buf, 6);

				//先发送验证码，成功后再记录进redis
				if (!m_verifyCode->insertVerifyCode(verifyCodeView.data(), verifyCodeView.size(), phoneView))
					return startWrite(WEBSERVICEANSWER::result2verifyCode.data(), WEBSERVICEANSWER::result2verifyCode.size());

				static std::string_view set{ "set" }, EX{ "EX" }, sec3600{ "3600" }, NX{ "NX" }, one{ "1" }, three{ "3" };

				////////////////////////////////////////////////////

				std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
				redisResultTypeSW& thisRequest{ *redisRequest };

				std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
				std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
				std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
				std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };

				//将手机号与ip地址记录插入redis保存

				command.clear();
				commandSize.clear();
				resultVec.clear();
				resultNumVec.clear();
				try
				{
					command.emplace_back(set);
					command.emplace_back(ipView);
					command.emplace_back(three);
					command.emplace_back(EX);
					command.emplace_back(sec3600);
					command.emplace_back(NX);
					commandSize.emplace_back(6);
					command.emplace_back(set);
					command.emplace_back(phoneView);
					command.emplace_back(verifyCodeView);
					command.emplace_back(EX);
					command.emplace_back(sec3600);
					command.emplace_back(NX);
					commandSize.emplace_back(6);

					std::get<1>(thisRequest) = 2;
					std::get<3>(thisRequest) = 2;
					std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handleregistration11, this, std::placeholders::_1, std::placeholders::_2);

					if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
						return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
					return;
				}
				catch (const std::exception& e)
				{
					return startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
				}
				///////////////////////////////////////////////////
			}          
			
			 //如果手机号不为空
			if (!resultVec[0].empty())
			{
				//如果手机号记录只有1位，检查是1还是0还是2   
				// 1表示已经注册，可以直接登录   0表示已列入黑名单   
				// 如果需要表示多种状态就增加位数，但注意不要与验证码位数一样即可，处理时先判断位数
				// 再根据位进行判断
				//如果手机号记录为6位，返回1提示验证码已下发
				//高效复用手机号这个string  能表示所有状态，最大程度节省redis内存，不用加前缀表示
				//不要直接比对是不是1位，因为以后会加内容，为了扩展性比对不是验证码长度即可
				if (resultVec[0].size() != 6)
				{
					if(*(resultVec[0].cbegin())=='1')
						return startWrite(WEBSERVICEANSWER::result5.data(), WEBSERVICEANSWER::result5.size());
					else if (*(resultVec[0].cbegin()) == '0')
						return startWrite(WEBSERVICEANSWER::result6.data(), WEBSERVICEANSWER::result6.size());
					else
						return startWrite(WEBSERVICEANSWER::result7.data(), WEBSERVICEANSWER::result7.size());
				}
				
				return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());
			}

			//如果手机号为空  IP地址记录不为空，返回8，防止被恶意刷短信api，等待超时回收key后才能发起新验证码请求
			//短信api与钱直接相关，不能随便发
			if (!resultVec[1].empty())
			{
				return startWrite(WEBSERVICEANSWER::result8.data(), WEBSERVICEANSWER::result8.size());
			}
			
		}
		else
		{
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		}
	}
	else
	{
		handleERRORMESSAGE(em);
	}
}



void WEBSERVICE::handleregistration11(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		if (resultVec.size() != 2 || resultVec[0]!="OK" || resultVec[1]!="OK")
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());


		return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());

	}
	else
	{
		handleERRORMESSAGE(em);
	}
}



//检查手机号与验证码接口
//首先检查手机号是否是中国手机号段
//然后检查验证码是否是6位且字符与验证码生成模块的一致
//然后发送查询到redis中获取核对
// 
//判断手机号记录是否与验证码长度一致
//一致的情况下才进行判断
//多次判断也无需重新发送验证码
//因为短信api接口商都提供自定义短信模板的功能
//可以根据需要设置指定长度的验证码
//只要位数足够长，比军事级还安全，在数小时甚至数天内都无法被攻破
//配合频率限制效果更佳
//6位验证码仅供参考
//长远看还可以节省大量验证码发送次数

//测试功能，目前先在管理后台实现，
//首先进行权限校验，以防止被滥用
void WEBSERVICE::checkVerifyCode()
{
	if (!hasLoginBack)
		return startWrite(WEBSERVICEANSWER::result4.data(), WEBSERVICEANSWER::result4.size());

	if (m_httpresult.isBodyEmpty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::phone, STATICSTRING::phoneLen, STATICSTRING::verifyCode, STATICSTRING::verifyCodeLen))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	const char** BodyBuffer{ m_buffer->bodyPara() };

	//对于需要反复使用的场景，使用keyVec里面的string_view来存储参数解析结果
	std::string_view& phoneView{ keyVec[0] };
	phoneView = std::string_view(*(BodyBuffer), *(BodyBuffer + 1) - *(BodyBuffer));
	std::string_view& verifyCodeView{ keyVec[1] };
	verifyCodeView = std::string_view(*(BodyBuffer + 2), *(BodyBuffer + 3) - *(BodyBuffer + 2));

	//判断是否是非法手机号  非法验证码
	if (!REGEXFUNCTION::isVaildPhone(phoneView) || !REGEXFUNCTION::isVaildVerifyCode(verifyCodeView))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::mget, REDISNAMESPACE::mgetLen));
		command.emplace_back(phoneView);
		command.emplace_back(m_IP);
		commandSize.emplace_back(3);

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handlecheckVerifyCode, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
			startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
	}

}



void WEBSERVICE::handlecheckVerifyCode(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		if (resultVec.size() != 2)
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());

		   
		//如果手机号记录为空，
		// 如果IP也为空，表示手机号信息已经被redis回收掉，返回提示让用户重新发送验证码
		// 如果IP不为空，则可能为恶意刷短信api或者填写错误注册手机号，返回0即可
		//分情况判断
		if (resultVec[0].empty())
		{
			if(resultVec[1].empty())
				return startWrite(WEBSERVICEANSWER::result8.data(), WEBSERVICEANSWER::result8.size());
			else
				return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		}

		        // 如果手机号记录只有1位，检查是1还是0还是2   
				// 1表示已经注册，可以直接登录   0表示已列入黑名单  
				// 如果需要表示多种状态就增加位数，但注意不要与验证码位数一样即可，处理时先判断位数
				// 再根据位进行判断
				//如果手机号记录为6位，则进行验证码比对
		//不要直接比对是不是1位，因为以后会加内容，为了扩展性比对不是验证码长度即可
		if (resultVec[0].size() != 6)
		{
			if (*resultVec[0].cbegin() == '1')
				return startWrite(WEBSERVICEANSWER::result5.data(), WEBSERVICEANSWER::result5.size());
			else if (*resultVec[0].cbegin() == '0')
				return startWrite(WEBSERVICEANSWER::result6.data(), WEBSERVICEANSWER::result6.size());
			else
				return startWrite(WEBSERVICEANSWER::result7.data(), WEBSERVICEANSWER::result7.size());
		}
		
		//比对正确时，返回填写用户信息页面，错误时返回默认值0即可
		if (resultVec[0].size() == 6)
		{
			static std::string_view one{ "1" }, web{ "web" }, two{ "2" };
			std::string_view& phoneView{ keyVec[0] };
			std::string_view& verifyCodeView{ keyVec[1] };
			if (std::equal(resultVec[0].cbegin(), resultVec[0].cend(), verifyCodeView.cbegin(), verifyCodeView.cend()))
			{
				STLtreeFast& st1{ m_STLtreeFastVec[0] };

				st1.reset();

				if (!st1.clear())
					return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

				if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, one.cbegin(), one.cend()))
					return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

				if (!st1.put(web.cbegin(), web.cend(), two.cbegin(), two.cend()))
					return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

				//仅当用户成功验证手机号后才能调用发送用户注册信息接口
				//保存用户当前手机号，待提交用户信息时一并提交
				//为了节省用户填写信息时间，建议将用户注册协议等信息在注册手机号页面进行展示
				//用户填写信息页面仅设置账号名与密码还有确认密码三项即可，
				//其余信息可以在登录后再进行设置
				hasVerifyRegister = true;
				m_phone.assign(phoneView.cbegin(), phoneView.cend());
				return makeSendJson(st1);
			}
			else
				return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		}

		return startWrite(WEBSERVICEANSWER::result2unknown.data(), WEBSERVICEANSWER::result2unknown.size());

	}
	else
	{
		handleERRORMESSAGE(em);
	}

}


//提交用户注册信息
void WEBSERVICE::registration2()
{
	if (!hasLoginBack)
		return startWrite(WEBSERVICEANSWER::result4.data(), WEBSERVICEANSWER::result4.size());

	if(!hasVerifyRegister)
		return startWrite(WEBSERVICEANSWER::result5.data(), WEBSERVICEANSWER::result5.size());

	if(m_phone.empty())
		return startWrite(WEBSERVICEANSWER::result5.data(), WEBSERVICEANSWER::result5.size());

	if (m_httpresult.isBodyEmpty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::username, STATICSTRING::usernameLen, STATICSTRING::password, STATICSTRING::passwordLen))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	const char** BodyBuffer{ m_buffer->bodyPara() };

	//对于需要反复使用的场景，使用keyVec里面的string_view来存储参数解析结果
	std::string_view& usernameView{ keyVec[0] };
	usernameView = std::string_view(*(BodyBuffer), *(BodyBuffer + 1) - *(BodyBuffer));
	std::string_view& passwordView{ keyVec[1] };
	passwordView = std::string_view(*(BodyBuffer + 2), *(BodyBuffer + 3) - *(BodyBuffer + 2));
	std::string_view& mysqlPasswordView{ keyVec[2] };
	

	//判断是否是非法密码  至少8位长度  且包含数字大小写字母  且不能包含;字符
	if (usernameView.empty() || !REGEXFUNCTION::isVaildPassword(passwordView))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	if(!mysqlEscape(usernameView, usernameView,m_MemoryPool))
		return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

	if (!mysqlEscape<int, OTHERSW>(passwordView, mysqlPasswordView, m_MemoryPool))
		return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());


	std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

	resultTypeSW& thisRequest{ *sqlRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& rowField{ std::get<3>(thisRequest).get() };
	std::vector<std::string_view>& result{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& sqlNum{ std::get<6>(thisRequest).get() };


	command.clear();
	rowField.clear();
	result.clear();
	sqlNum.clear();

	try
	{
		
		command.emplace_back(SQLCOMMAND::insertUser1);
		command.emplace_back(usernameView);
		command.emplace_back(SQLCOMMAND::insertUser2);
		command.emplace_back(mysqlPasswordView);
		command.emplace_back(SQLCOMMAND::insertUser3);
		command.emplace_back(m_phone);
		command.emplace_back(SQLCOMMAND::insertUser4);


		//获取结果次数
		std::get<1>(thisRequest) = 1;
		//sql组成string_view个数
		sqlNum.emplace_back(7);
		std::get<5>(thisRequest) = std::bind(&WEBSERVICE::handleregistration21, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWMaster->insertSqlRequest<SQLFREE,int>(sqlRequest))
			return startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
	}
}


void WEBSERVICE::handleregistration21(bool result, ERRORMESSAGE em)
{
	if (result)
	{

		std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

		resultTypeSW& thisRequest{ *sqlRequest };

		std::vector<std::string_view>& resultvec{ std::get<4>(thisRequest).get() };
		

		if (resultvec.empty())
			return startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());

		//6  账号名冲突
		if (resultvec[0]!="success")
			return startWrite(WEBSERVICEANSWER::result6.data(), WEBSERVICEANSWER::result6.size());

		
		{   //对于需要反复使用的场景，使用keyVec里面的string_view来存储参数解析结果
			std::string_view& usernameView{ keyVec[0] };
			std::string_view& passwordView{ keyVec[1] };

			std::string_view account{ "account" }, password{ "password" }, phone{ "phone" }, one{"1"};

			std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
			redisResultTypeSW& thisRequest{ *redisRequest };

			std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
			std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
			std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
			std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };



			command.clear();
			commandSize.clear();
			resultVec.clear();
			resultNumVec.clear();
			try
			{
				command.emplace_back(std::string_view(REDISNAMESPACE::hset, REDISNAMESPACE::hsetLen));
				command.emplace_back(usernameView);
				command.emplace_back(account);
				command.emplace_back(usernameView);
				command.emplace_back(password);
				command.emplace_back(passwordView);
				command.emplace_back(phone);
				command.emplace_back(m_phone);
				commandSize.emplace_back(8);
				command.emplace_back(std::string_view(REDISNAMESPACE::set, REDISNAMESPACE::setLen));
				command.emplace_back(m_phone);
				command.emplace_back(one);
				commandSize.emplace_back(3);


				std::get<1>(thisRequest) = 2;
				std::get<3>(thisRequest) = 2;
				std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handleregistration22, this, std::placeholders::_1, std::placeholders::_2);

				if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
					startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());

				return;
			}
			catch (const std::exception& e)
			{
				return startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
			}
		}


		return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());
	}
	else
	{
		handleERRORMESSAGE(em);
	}

}



void WEBSERVICE::handleregistration22(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		//结果收到为2时先简单返回成功，后面根据需要完善判断逻辑
		//比如redis写入失败是否需要删除数据库注册信息
		if (resultVec.size() != 2)
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());

		hasVerifyRegister = false;

		m_phone.clear();
		
		return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());
	}
	else
	{
		handleERRORMESSAGE(em);
	}

}


//用户登录账号
void WEBSERVICE::userLogin()
{
	if(m_userLoginTime>=3)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	if (m_httpresult.isBodyEmpty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::username, STATICSTRING::usernameLen, STATICSTRING::password, STATICSTRING::passwordLen))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	const char** BodyBuffer{ m_buffer->bodyPara() };

	//对于需要反复使用的场景，使用keyVec里面的string_view来存储参数解析结果
	std::string_view& usernameView{ keyVec[0] };
	usernameView = std::string_view(*(BodyBuffer), *(BodyBuffer + 1) - *(BodyBuffer));
	std::string_view& passwordView{ keyVec[1] };
	passwordView = std::string_view(*(BodyBuffer + 2), *(BodyBuffer + 3) - *(BodyBuffer + 2));

	//判断是否是非法密码  至少8位长度  且包含数字大小写字母  且不能包含;字符
	if (usernameView.empty() || !REGEXFUNCTION::isVaildPassword(passwordView))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	if(!mysqlEscape(usernameView, usernameView,m_MemoryPool))
		return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };
	static std::string_view password{ "password" };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };

	//获取用户密码以及相关信息
	//从redis获取密码进行比对，再做后续操作，避免SQL注入，可以根据需要进行加盐操作

	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::hget, REDISNAMESPACE::hgetLen));
		command.emplace_back(usernameView);
		command.emplace_back(password);
		commandSize.emplace_back(3);

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handleuserLogin, this, std::placeholders::_1, std::placeholders::_2);


		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
			startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
	}

}



void WEBSERVICE::handleuserLogin(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		if (!resultVec.empty())
		{
			if(resultVec[0].empty())
				return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

			std::string_view& usernameView{ keyVec[0] };
			std::string_view& passwordView{ keyVec[1] };


			//比对密码返回
			if (std::equal(passwordView.cbegin(), passwordView.cend(), resultVec[0].cbegin(), resultVec[0].cend()))
			{
				hasUserLogin = true;
				m_userLoginTime = 0;
				m_userInfo.setAccount(usernameView);
				return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());
			}
			else
			{
				++m_userLoginTime;

				return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
			}

		}
		else
		{
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
		}
	}
	else
	{
		handleERRORMESSAGE(em);
	}

}


//用户退出账号登录
void WEBSERVICE::userLoginOut()
{
	hasUserLogin = false;

	return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());
}



//用户提交信息登记
void WEBSERVICE::userInfo()
{
	if(!hasUserLogin)
		return startWrite(WEBSERVICEANSWER::result5.data(), WEBSERVICEANSWER::result5.size());

	if (m_httpresult.isBodyEmpty())
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::name, STATICSTRING::nameLen, STATICSTRING::height, STATICSTRING::heightLen,
		STATICSTRING::age, STATICSTRING::ageLen, STATICSTRING::province, STATICSTRING::provinceLen, STATICSTRING::city, STATICSTRING::cityLen))
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	const char** BodyBuffer{ m_buffer->bodyPara() };

	std::string_view& nameView{ keyVec[0] };
	nameView = std::string_view(*(BodyBuffer), *(BodyBuffer + 1) - *(BodyBuffer));

	//0-300
	std::string_view& heightView{ keyVec[1] };
	heightView = std::string_view(*(BodyBuffer + 2), *(BodyBuffer + 3) - *(BodyBuffer + 2));

	//0-150
	std::string_view& ageView{ keyVec[2] };
	ageView = std::string_view(*(BodyBuffer + 4), *(BodyBuffer + 5) - *(BodyBuffer + 4));

	//1-34
	std::string_view& provinceView{ keyVec[3] };
	provinceView = std::string_view(*(BodyBuffer + 6), *(BodyBuffer + 7) - *(BodyBuffer + 6));

	//1-38
	std::string_view& cityView{ keyVec[4] };
	cityView = std::string_view(*(BodyBuffer + 8), *(BodyBuffer + 9) - *(BodyBuffer + 8));


	//校验参数
	if(nameView.empty() ||
		heightView.empty() || heightView.size() > 3 ||
		ageView.empty() || ageView.size() > 3 ||
		provinceView.empty() || provinceView.size() > 2 ||
		cityView.empty() || cityView.size() > 2)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	if(!mysqlEscape(nameView, nameView,m_MemoryPool))
		return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());


	//判断身高 0-300
	int index{ -1 }, num{ 1 }, sum{}, provinceNum{}, cityNum{};
	//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
	if (!std::all_of(std::make_reverse_iterator(heightView.cend()), std::make_reverse_iterator(heightView.cbegin()), [&index, &num, &sum](const char ch)
	{
		if (!std::isdigit(ch))
		{
			return false;
		}
		if (++index)
			num *= 10;
		sum += (ch - '0') * num;
		return true;
	}) || sum > 300)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());


	//判断年龄
	index = -1, num = 1, sum = 0;
	if (!std::all_of(std::make_reverse_iterator(ageView.cend()), std::make_reverse_iterator(ageView.cbegin()), [&index, &num, &sum](const char ch)
	{
		if (!std::isdigit(ch))
		{
			return false;
		}
		if (++index)
			num *= 10;
		sum += (ch - '0') * num;
		return true;
	}) || sum > 150)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());


	//判断省份代号
	index = -1, num = 1, sum = 0;
	if (!std::all_of(std::make_reverse_iterator(provinceView.cend()), std::make_reverse_iterator(provinceView.cbegin()), [&index, &num, &sum](const char ch)
	{
		if (!std::isdigit(ch))
		{
			return false;
		}
		if (++index)
			num *= 10;
		sum += (ch - '0') * num;
		return true;
	}) || !sum || sum > 34)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	provinceNum = sum;


	//判断城市代号
	index = -1, num = 1, sum = 0;
	if (!std::all_of(std::make_reverse_iterator(provinceView.cend()), std::make_reverse_iterator(provinceView.cbegin()), [&index, &num, &sum](const char ch)
	{
		if (!std::isdigit(ch))
		{
			return false;
		}
		if (++index)
			num *= 10;
		sum += (ch - '0') * num;
		return true;
	}) || !sum || sum > 38)
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

	cityNum = sum;
	

	//与前端省份  城市代号进行比对
	/*

	const provinces=[
[1,"北京市"],[2,"天津市"],[3,"河北省"],[4,"山西省"],[5,"内蒙古自治区"],
[6,"辽宁省"],[7,"吉林省"],[8,"黑龙江省"],[9,"上海市"],[10,"江苏省"],
[11,"浙江省"],[12,"安徽省"],[13,"福建省"],[14,"江西省"],[15,"山东省"],
[16,"河南省"],[17,"湖北省"],[18,"湖南省"],[19,"广东省"],[20,"广西壮族自治区"],
[21,"海南省"],[22,"重庆市"],[23,"四川省"],[24,"贵州省"],[25,"云南省"],
[26,"西藏自治区"],[27,"陕西省"],[28,"甘肃省"],[29,"青海省"],[30,"宁夏回族自治区"],
[31,"新疆维吾尔自治区"],[32,"香港特别行政区"],[33,"澳门特别行政区"],[34,"台湾省"]
];



	const cityData={
1:[[1,"东城区"],[2,"西城区"],[3,"朝阳区"],[4,"丰台区"],[5,"石景山区"],[6,"海淀区"],[7,"顺义区"],[8,"通州区"],[9,"大兴区"],[10,"房山区"],[11,"门头沟区"],[12,"昌平区"],[13,"平谷区"],[14,"密云区"],[15,"延庆区"]], 
2:[[1,"和平区"],[2,"河东区"],[3,"河西区"],[4,"南开区"],[5,"河北区"],[6,"红桥区"],[7,"东丽区"],[8,"西青区"],[9,"津南区"],[10,"北辰区"],[11,"武清区"],[12,"宝坻区"],[13,"滨海新区"],[14,"宁河区"],[15,"静海区"],[16,"蓟州区"]], 
3:[[1,"石家庄市"],[2,"唐山市"],[3,"秦皇岛市"],[4,"邯郸市"],[5,"邢台市"],[6,"保定市"],[7,"张家口市"],[8,"承德市"],[9,"沧州市"],[10,"廊坊市"],[11,"衡水市"]], 
4:[[1,"太原市"],[2,"大同市"],[3,"阳泉市"],[4,"长治市"],[5,"晋城市"],[6,"朔州市"],[7,"晋中市"],[8,"运城市"],[9,"忻州市"],[10,"临汾市"],[11,"吕梁市"]], 
5:[[1,"呼和浩特市"],[2,"包头市"],[3,"乌海市"],[4,"赤峰市"],[5,"通辽市"],[6,"鄂尔多斯市"],[7,"呼伦贝尔市"],[8,"巴彦淖尔市"],[9,"乌兰察布市"],[10,"兴安盟"],[11,"锡林郭勒盟"],[12,"阿拉善盟"]], 
6:[[1,"沈阳市"],[2,"大连市"],[3,"鞍山市"],[4,"抚顺市"],[5,"本溪市"],[6,"丹东市"],[7,"锦州市"],[8,"营口市"],[9,"阜新市"],[10,"辽阳市"],[11,"盘锦市"],[12,"铁岭市"],[13,"朝阳市"],[14,"葫芦岛市"]], 
7:[[1,"长春市"],[2,"吉林市"],[3,"四平市"],[4,"辽源市"],[5,"通化市"],[6,"白山市"],[7,"松原市"],[8,"白城市"],[9,"延边朝鲜族自治州"]], 
8:[[1,"哈尔滨市"],[2,"齐齐哈尔市"],[3,"鸡西市"],[4,"鹤岗市"],[5,"双鸭山市"],[6,"大庆市"],[7,"伊春市"],[8,"佳木斯市"],[9,"七台河市"],[10,"牡丹江市"],[11,"黑河市"],[12,"绥化市"],[13,"大兴安岭地区"]], 
9:[[1,"黄浦区"],[2,"徐汇区"],[3,"长宁区"],[4,"静安区"],[5,"普陀区"],[6,"虹口区"],[7,"杨浦区"],[8,"闵行区"],[9,"宝山区"],[10,"嘉定区"],[11,"浦东新区"],[12,"金山区"],[13,"松江区"],[14,"青浦区"],[15,"奉贤区"],[16,"崇明区"]], 
10:[[1,"南京市"],[2,"无锡市"],[3,"徐州市"],[4,"常州市"],[5,"苏州市"],[6,"南通市"],[7,"连云港市"],[8,"淮安市"],[9,"盐城市"],[10,"扬州市"],[11,"镇江市"],[12,"泰州市"],[13,"宿迁市"]], 
11:[[1,"杭州市"],[2,"宁波市"],[3,"温州市"],[4,"嘉兴市"],[5,"湖州市"],[6,"绍兴市"],[7,"金华市"],[8,"衢州市"],[9,"舟山市"],[10,"台州市"],[11,"丽水市"]], 
12:[[1,"合肥市"],[2,"芜湖市"],[3,"蚌埠市"],[4,"淮南市"],[5,"马鞍山市"],[6,"淮北市"],[7,"铜陵市"],[8,"安庆市"],[9,"黄山市"],[10,"滁州市"],[11,"阜阳市"],[12,"宿州市"],[13,"六安市"],[14,"亳州市"],[15,"池州市"],[16,"宣城市"]], 
13:[[1,"福州市"],[2,"厦门市"],[3,"莆田市"],[4,"三明市"],[5,"泉州市"],[6,"漳州市"],[7,"南平市"],[8,"龙岩市"],[9,"宁德市"]], 
14:[[1,"南昌市"],[2,"景德镇市"],[3,"萍乡市"],[4,"九江市"],[5,"新余市"],[6,"鹰潭市"],[7,"赣州市"],[8,"吉安市"],[9,"宜春市"],[10,"抚州市"],[11,"上饶市"]], 
15:[[1,"济南市"],[2,"青岛市"],[3,"淄博市"],[4,"枣庄市"],[5,"东营市"],[6,"烟台市"],[7,"潍坊市"],[8,"济宁市"],[9,"泰安市"],[10,"威海市"],[11,"日照市"],[12,"临沂市"],[13,"德州市"],[14,"聊城市"],[15,"滨州市"],[16,"菏泽市"]], 
16:[[1,"郑州市"],[2,"开封市"],[3,"洛阳市"],[4,"平顶山市"],[5,"安阳市"],[6,"鹤壁市"],[7,"新乡市"],[8,"焦作市"],[9,"濮阳市"],[10,"许昌市"],[11,"漯河市"],[12,"三门峡市"],[13,"南阳市"],[14,"商丘市"],[15,"信阳市"],[16,"周口市"],[17,"驻马店市"],[18,"济源市"]], 
17:[[1,"武汉市"],[2,"黄石市"],[3,"十堰市"],[4,"宜昌市"],[5,"襄阳市"],[6,"鄂州市"],[7,"荆门市"],[8,"孝感市"],[9,"荆州市"],[10,"黄冈市"],[11,"咸宁市"],[12,"随州市"],[13,"恩施土家族苗族自治州"],[14,"仙桃市"],[15,"潜江市"],[16,"天门市"],[17,"神农架林区"]], 
18:[[1,"长沙市"],[2,"株洲市"],[3,"湘潭市"],[4,"衡阳市"],[5,"邵阳市"],[6,"岳阳市"],[7,"常德市"],[8,"张家界市"],[9,"益阳市"],[10,"郴州市"],[11,"永州市"],[12,"怀化市"],[13,"娄底市"],[14,"湘西土家族苗族自治州"]], 
19:[[1,"广州市"],[2,"韶关市"],[3,"深圳市"],[4,"珠海市"],[5,"汕头市"],[6,"佛山市"],[7,"江门市"],[8,"湛江市"],[9,"茂名市"],[10,"肇庆市"],[11,"惠州市"],[12,"梅州市"],[13,"汕尾市"],[14,"河源市"],[15,"阳江市"],[16,"清远市"],[17,"东莞市"],[18,"中山市"],[19,"潮州市"],[20,"揭阳市"],[21,"云浮市"]], 
20:[[1,"南宁市"],[2,"柳州市"],[3,"桂林市"],[4,"梧州市"],[5,"北海市"],[6,"防城港市"],[7,"钦州市"],[8,"贵港市"],[9,"玉林市"],[10,"百色市"],[11,"贺州市"],[12,"河池市"],[13,"来宾市"],[14,"崇左市"]], 
21:[[1,"海口市"],[2,"三亚市"],[3,"三沙市"],[4,"儋州市"],[5,"五指山市"],[6,"琼海市"],[7,"文昌市"],[8,"万宁市"],[9,"东方市"],[10,"定安县"],[11,"屯昌县"],[12,"澄迈县"],[13,"临高县"],[14,"白沙黎族自治县"],[15,"昌江黎族自治县"],[16,"乐东黎族自治县"],[17,"陵水黎族自治县"],[18,"保亭黎族苗族自治县"],[19,"琼中黎族苗族自治县"]], 
22:[[1,"万州区"],[2,"涪陵区"],[3,"渝中区"],[4,"大渡口区"],[5,"江北区"],[6,"沙坪坝区"],[7,"九龙坡区"],[8,"南岸区"],[9,"北碚区"],[10,"綦江区"],[11,"大足区"],[12,"渝北区"],[13,"巴南区"],[14,"黔江区"],[15,"长寿区"],[16,"江津区"],[17,"合川区"],[18,"永川区"],[19,"南川区"],[20,"璧山区"],[21,"铜梁区"],[22,"潼南区"],[23,"荣昌区"],[24,"开州区"],[25,"梁平区"],[26,"武隆区"],[27,"城口县"],[28,"丰都县"],[29,"垫江县"],[30,"忠县"],[31,"云阳县"],[32,"奉节县"],[33,"巫山县"],[34,"巫溪县"],[35,"石柱土家族自治县"],[36,"秀山土家族苗族自治县"],[37,"酉阳土家族苗族自治县"],[38,"彭水苗族土家族自治县"]], 
23:[[1,"成都市"],[2,"自贡市"],[3,"攀枝花市"],[4,"泸州市"],[5,"德阳市"],[6,"绵阳市"],[7,"广元市"],[8,"遂宁市"],[9,"内江市"],[10,"乐山市"],[11,"南充市"],[12,"眉山市"],[13,"宜宾市"],[14,"广安市"],[15,"达州市"],[16,"雅安市"],[17,"巴中市"],[18,"资阳市"],[19,"阿坝藏族羌族自治州"],[20,"甘孜藏族自治州"],[21,"凉山彝族自治州"]], 
24:[[1,"贵阳市"],[2,"六盘水市"],[3,"遵义市"],[4,"安顺市"],[5,"毕节市"],[6,"铜仁市"],[7,"黔西南布依族苗族自治州"],[8,"黔东南苗族侗族自治州"],[9,"黔南布依族苗族自治州"]], 
25:[[1,"昆明市"],[2,"曲靖市"],[3,"玉溪市"],[4,"保山市"],[5,"昭通市"],[6,"丽江市"],[7,"普洱市"],[8,"临沧市"],[9,"楚雄彝族自治州"],[10,"红河哈尼族彝族自治州"],[11,"文山壮族苗族自治州"],[12,"西双版纳傣族自治州"],[13,"大理白族自治州"],[14,"德宏傣族景颇族自治州"],[15,"怒江傈僳族自治州"],[16,"迪庆藏族自治州"]], 
26:[[1,"拉萨市"],[2,"日喀则市"],[3,"昌都市"],[4,"林芝市"],[5,"山南市"],[6,"那曲市"],[7,"阿里地区"]], 
27:[[1,"西安市"],[2,"铜川市"],[3,"宝鸡市"],[4,"咸阳市"],[5,"渭南市"],[6,"延安市"],[7,"汉中市"],[8,"榆林市"],[9,"安康市"],[10,"商洛市"]], 
28:[[1,"兰州市"],[2,"嘉峪关市"],[3,"金昌市"],[4,"白银市"],[5,"天水市"],[6,"武威市"],[7,"张掖市"],[8,"平凉市"],[9,"酒泉市"],[10,"庆阳市"],[11,"定西市"],[12,"陇南市"],[13,"临夏回族自治州"],[14,"甘南藏族自治州"]], 
29:[[1,"西宁市"],[2,"海东市"],[3,"海北藏族自治州"],[4,"黄南藏族自治州"],[5,"海南藏族自治州"],[6,"果洛藏族自治州"],[7,"玉树藏族自治州"],[8,"海西蒙古族藏族自治州"]], 
30:[[1,"银川市"],[2,"石嘴山市"],[3,"吴忠市"],[4,"固原市"],[5,"中卫市"]], 
31:[[1,"乌鲁木齐市"],[2,"克拉玛依市"],[3,"吐鲁番市"],[4,"哈密市"],[5,"昌吉回族自治州"],[6,"博尔塔拉蒙古自治州"],[7,"巴音郭楞蒙古自治州"],[8,"阿克苏地区"],[9,"克孜勒苏柯尔克孜自治州"],[10,"喀什地区"],[11,"和田地区"],[12,"伊犁哈萨克自治州"],[13,"塔城地区"],[14,"阿勒泰地区"],[15,"石河子市"],[16,"阿拉尔市"],[17,"图木舒克市"],[18,"五家渠市"],[19,"北屯市"],[20,"铁门关市"],[21,"双河市"],[22,"可克达拉市"],[23,"昆玉市"],[24,"胡杨河市"]], 
32:[[1,"中西区"],[2,"湾仔区"],[3,"东区"],[4,"南区"],[5,"油尖旺区"],[6,"深水埗区"],[7,"九龙城区"],[8,"黄大仙区"],[9,"观塘区"],[10,"荃湾区"],[11,"屯门区"],[12,"元朗区"],[13,"北区"],[14,"大埔区"],[15,"西贡区"],[16,"沙田区"],[17,"葵青区"],[18,"离岛区"]], 
33:[[1,"花地玛堂区"],[2,"圣安多尼堂区"],[3,"大堂区"],[4,"望德堂区"],[5,"风顺堂区"],[6,"嘉模堂区"],[7,"圣方济各堂区"],[8,"路氹城"]], 
34:[[1,"台北市"],[2,"新北市"],[3,"桃园市"],[4,"台中市"],[5,"台南市"],[6,"高雄市"],[7,"基隆市"],[8,"新竹市"],[9,"嘉义市"],[10,"新竹县"],[11,"苗栗县"],[12,"彰化县"],[13,"南投县"],[14,"云林县"],[15,"嘉义县"],[16,"屏东县"],[17,"宜兰县"],[18,"花莲县"],[19,"台东县"],[20,"澎湖县"],[21,"金门县"],[22,"连江县"]]
};

	
	
	*/
	switch (provinceNum)
	{
	case 1:
		if (cityNum > 15)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 2:
		if (cityNum > 16)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 3:
		if (cityNum > 11)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 4:
		if (cityNum > 11)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 5:
		if (cityNum > 12)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 6:
		if (cityNum > 14)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 7:
		if (cityNum > 9)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 8:
		if (cityNum > 13)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 9:
		if (cityNum > 16)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 10:
		if (cityNum > 13)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 11:
		if (cityNum > 11)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 12:
		if (cityNum > 16)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 13:
		if (cityNum > 9)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 14:
		if (cityNum > 11)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 15:
		if (cityNum > 16)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 16:
		if (cityNum > 18)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 17:
		if (cityNum > 17)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 18:
		if (cityNum > 14)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 19:
		if (cityNum > 21)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 20:
		if (cityNum > 14)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 21:
		if (cityNum > 19)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 22:
		if (cityNum > 38)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 23:
		if (cityNum > 21)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 24:
		if (cityNum > 9)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 25:
		if (cityNum > 16)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 26:
		if (cityNum > 7)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 27:
		if (cityNum > 10)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 28:
		if (cityNum > 14)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 29:
		if (cityNum > 8)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 30:
		if (cityNum > 5)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 31:
		if (cityNum > 24)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 32:
		if (cityNum > 18)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 33:
		if (cityNum > 8)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	case 34:
		if (cityNum > 22)
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	default:
		return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());
		break;
	}

	////////////////////////////////////插入sql 等待管理员审核信息

	std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

	resultTypeSW& thisRequest{ *sqlRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& rowField{ std::get<3>(thisRequest).get() };
	std::vector<std::string_view>& result{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& sqlNum{ std::get<6>(thisRequest).get() };


	command.clear();
	rowField.clear();
	result.clear();
	sqlNum.clear();

	try
	{

		command.emplace_back(SQLCOMMAND::updateUser1);
		command.emplace_back(nameView);
		command.emplace_back(SQLCOMMAND::updateUser2);
		command.emplace_back(heightView);
		command.emplace_back(SQLCOMMAND::updateUser3);
		command.emplace_back(ageView);
		command.emplace_back(SQLCOMMAND::updateUser4);
		command.emplace_back(provinceView);
		command.emplace_back(SQLCOMMAND::updateUser5);
		command.emplace_back(cityView);
		command.emplace_back(SQLCOMMAND::updateUser6);
		command.emplace_back(m_userInfo.getAccountView());
		command.emplace_back(SQLCOMMAND::updateUser7);


		//获取结果次数
		std::get<1>(thisRequest) = 1;
		//sql组成string_view个数
		sqlNum.emplace_back(13);
		std::get<5>(thisRequest) = std::bind(&WEBSERVICE::handleuserInfo, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWMaster->insertSqlRequest<SQLFREE, int>(sqlRequest))
			return startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
	}

}



void WEBSERVICE::handleuserInfo(bool result, ERRORMESSAGE em)
{
	if (result)
	{

		std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

		resultTypeSW& thisRequest{ *sqlRequest };

		std::vector<std::string_view>& resultvec{ std::get<4>(thisRequest).get() };


		if (resultvec.empty() || resultvec[0] != "success")
			return startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());


		return startWrite(WEBSERVICEANSWER::result1.data(), WEBSERVICEANSWER::result1.size());
	}
	else
	{
		handleERRORMESSAGE(em);
	}


}



//获取用户五项信息
void WEBSERVICE::getUserInfo()
{
	if(!hasUserLogin)
		return startWrite(WEBSERVICEANSWER::result5.data(), WEBSERVICEANSWER::result5.size());

	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };

	//获取用户五项信息

	static std::string_view name{ "name" }, height{ "height" }, age{ "age" }, province{ "province" }, city{ "city" };

	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::hmget, REDISNAMESPACE::hmgetLen));
		command.emplace_back(m_userInfo.getAccountView());
		command.emplace_back(name);
		command.emplace_back(height);
		command.emplace_back(age);
		command.emplace_back(province);
		command.emplace_back(city);
		
		commandSize.emplace_back(command.size());

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&WEBSERVICE::handlegetUserInfo, this, std::placeholders::_1, std::placeholders::_2);


		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
			startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2stl.data(), WEBSERVICEANSWER::result2stl.size());
	}
}



void WEBSERVICE::handlegetUserInfo(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW>& redisRequest{ m_multiRedisRequestSWVec[0] };

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view>& resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& resultNumVec{ std::get<5>(thisRequest).get() };


	if (result)
	{
		if (resultVec.size() != 5)
			return startWrite(WEBSERVICEANSWER::result2redis.data(), WEBSERVICEANSWER::result2redis.size());


		static std::string_view name{ "name" }, height{ "height" }, age{ "age" }, province{ "province" }, city{ "city" }, 
			result{ "result" }, one{"1"};

		STLtreeFast& st1{ m_STLtreeFastVec[0] };

		st1.reset();

		if (!st1.clear())
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if (!st1.put(result.cbegin(), result.cend(), one.cbegin(), one.cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());


		if (!st1.put(name.cbegin(), name.cend(), resultVec[0].cbegin(), resultVec[0].cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if (!st1.put(height.cbegin(), height.cend(), resultVec[1].cbegin(), resultVec[1].cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if (!st1.put(age.cbegin(), age.cend(), resultVec[2].cbegin(), resultVec[2].cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if (!st1.put(province.cbegin(), province.cend(), resultVec[3].cbegin(), resultVec[3].cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if (!st1.put(city.cbegin(), city.cend(), resultVec[4].cbegin(), resultVec[4].cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());


		makeSendJson(st1);
		
	}
	else
	{
		handleERRORMESSAGE(em);
	}

}


//后台查询待审核用户信息
void WEBSERVICE::getUserInfoExamine()
{
	if(!hasLoginBack)
		return startWrite(WEBSERVICEANSWER::result4.data(), WEBSERVICEANSWER::result4.size());

	std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

	resultTypeSW& thisRequest{ *sqlRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int>& rowField{ std::get<3>(thisRequest).get() };
	std::vector<std::string_view>& result{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int>& sqlNum{ std::get<6>(thisRequest).get() };


	command.clear();
	rowField.clear();
	result.clear();
	sqlNum.clear();

	try
	{

		command.emplace_back(SQLCOMMAND::queryUserExamine);

		//获取结果次数
		std::get<1>(thisRequest) = 1;
		//sql组成string_view个数
		sqlNum.emplace_back(command.size());
		std::get<5>(thisRequest) = std::bind(&WEBSERVICE::handlegetUserInfoExamine, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWMaster->insertSqlRequest<SQLFREE, int>(sqlRequest))
			return startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());
	}
	catch (const std::exception& e)
	{
		startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
	}

}

void WEBSERVICE::handlegetUserInfoExamine(bool result, ERRORMESSAGE em)
{
	if (result)
	{
		std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

		resultTypeSW& thisRequest{ *sqlRequest };

		std::vector<std::string_view>& resultvec{ std::get<4>(thisRequest).get() };

		if(resultvec.size() % 2)
			return startWrite(WEBSERVICEANSWER::result2mysql.data(), WEBSERVICEANSWER::result2mysql.size());

		if (resultvec.empty())
			return startWrite(WEBSERVICEANSWER::result0.data(), WEBSERVICEANSWER::result0.size());

		STLtreeFast& st1{ m_STLtreeFastVec[0] }, &st2{ m_STLtreeFastVec[1] };

		st1.reset();
		st2.reset();
		

		if (!st1.clear())
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if (!st2.clear())
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());


		static std::string_view account{ "account" }, name{ "name" }, result{ "result" }, one{ "1" }, data{"data"};

		for (auto iter = resultvec.cbegin(); iter != resultvec.cend(); iter += 2)
		{
			if(!st1.put<TRANSFORMTYPE>(account.cbegin(), account.cend(), iter->cbegin(),iter->cend()))
				return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

			if (!st1.put<TRANSFORMTYPE>(name.cbegin(), name.cend(), (iter+1)->cbegin(), (iter+1)->cend()))
				return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

			if (!st2.push_back<TRANSFORMTYPE>(st1))
				return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

			if (!st1.clear())
				return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		}

		if (!st1.put<TRANSFORMTYPE>(result.cbegin(), result.cend(), one.cbegin(), one.cend()))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());

		if(!st1.putArray<TRANSFORMTYPE>(data.cbegin(), data.cend(),st2))
			return startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
		

		makeSendJson<TRANSFORMTYPE>(st1);

	}
	else
	{
		handleERRORMESSAGE(em);
	}

}





























