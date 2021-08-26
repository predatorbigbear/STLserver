#include "httpService.h"





HTTPSERVICE::HTTPSERVICE(std::shared_ptr<io_context> ioc, std::shared_ptr<LOG> log, 
	std::shared_ptr<MULTISQLREADSW>multiSqlReadSWSlave,
	std::shared_ptr<MULTISQLREADSW>multiSqlReadSWMaster, std::shared_ptr<MULTIREDISREAD>multiRedisReadSlave,
	std::shared_ptr<MULTIREDISREAD>multiRedisReadMaster,
	std::shared_ptr<MULTIREDISWRITE>multiRedisWriteMaster, std::shared_ptr<MULTISQLWRITESW>multiSqlWriteSWMaster,
	const unsigned int timeOut, bool & success,
	char *publicKeyBegin, char *publicKeyEnd, int publicKeyLen, char *privateKeyBegin, char *privateKeyEnd, int privateKeyLen,
	RSA* rsaPublic, RSA* rsaPrivate
	)
	:m_ioc(ioc), m_log(log),
	m_multiSqlReadSWSlave(multiSqlReadSWSlave),m_multiSqlReadSWMaster(multiSqlReadSWMaster),m_multiRedisReadSlave(multiRedisReadSlave), 
	m_multiRedisReadMaster(multiRedisReadMaster),m_multiRedisWriteMaster(multiRedisWriteMaster),m_multiSqlWriteSWMaster(multiSqlWriteSWMaster),
	m_timeOut(timeOut),
	m_publicKeyBegin(publicKeyBegin), m_publicKeyEnd(publicKeyEnd), m_publicKeyLen(publicKeyLen), m_privateKeyBegin(privateKeyBegin), m_privateKeyEnd(privateKeyEnd), m_privateKeyLen(privateKeyLen),
	m_rsaPublic(rsaPublic),m_rsaPrivate(rsaPrivate)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is nullptr");
		if (!log)
			throw std::runtime_error("log is nullptr");
		if(!timeOut)
			throw std::runtime_error("timeOut is invaild");
		if(!multiSqlReadSWSlave)
			throw std::runtime_error("multiSqlReadSWSlave is nullptr");
		if (!multiSqlReadSWMaster)
			throw std::runtime_error("multiSqlReadSWMaster is nullptr");
		if(!multiRedisReadSlave)
			throw std::runtime_error("multiRedisReadSlave is nullptr");
		if (!multiRedisReadMaster)
			throw std::runtime_error("multiRedisReadMaster is nullptr");
		if (!multiRedisWriteMaster)
			throw std::runtime_error("multiRedisWriteMaster is nullptr");
		if (!multiSqlWriteSWMaster)
			throw std::runtime_error("multiSqlWriteSWMaster is nullptr");
		if (!publicKeyBegin)
			throw std::runtime_error("publicKeyBegin is null");
		if (!publicKeyEnd)
			throw std::runtime_error("publicKeyEnd is null");
		if (!publicKeyLen)
			throw std::runtime_error("publicKeyLen is null");
		if (!privateKeyBegin)
			throw std::runtime_error("privateKeyBegin is null");
		if (!privateKeyEnd)
			throw std::runtime_error("privateKeyEnd is null");
		if (!privateKeyLen)
			throw std::runtime_error("privateKeyLen is null");
		if (!rsaPublic)
			throw std::runtime_error("rsaPublic is null");
		if (!rsaPrivate)
			throw std::runtime_error("rsaPrivate is null");

		m_randomString = new char[randomStringLen];

		std::copy(randomString, randomString + randomStringLen, m_randomString);

		m_timer.reset(new boost::asio::steady_timer(*m_ioc));

		m_buffer.reset(new ReadBuffer(m_ioc));

		m_verifyData.reset(new const char*[VerifyDataPos::maxBufferSize]);

		m_clientAES.reset(new char[maxAESKeyLen]);

		m_SessionID.reset(new char[128]);

		m_sessionMemory = 128;

		m_sessionLen = 0;

		m_business = std::bind(&HTTPSERVICE::sendOK, this);

		prepare();

		success = true;
	}
	catch (const std::exception &e)
	{
		success = false;
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
}




void HTTPSERVICE::setReady(const int index, std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>)>> clearFunction , std::shared_ptr<HTTPSERVICE> other)
{
	//m_buffer->getSock()->set_option(boost::asio::ip::tcp::no_delay(true), m_err);
	m_buffer->getSock()->set_option(boost::asio::socket_base::keep_alive(true), m_err);     https://www.cnblogs.com/xiao-tao/p/9718017.html
	m_index = index;
	m_clearFunction = clearFunction;
	m_mySelf = other;

	m_lock.lock();
	m_hasClean = false;
	m_time = std::chrono::high_resolution_clock::now();
	m_lock.unlock();

	run();
}




std::shared_ptr<HTTPSERVICE> *&HTTPSERVICE::getListIter()
{
	// TODO: 在此处插入 return 语句
	return mySelfIter;
}




bool HTTPSERVICE::checkTimeOut()
{
	m_lock.lock();
	if (!m_hasClean && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_time).count() >= m_timeOut)
	{
		m_lock.unlock();
		return true;
	}
	m_lock.unlock();
	return false;
}




void HTTPSERVICE::run()
{
	m_readBuffer = m_buffer->getBuffer();
	m_startPos = 0;
	m_maxReadLen = m_defaultReadLen;
	startRead();
}




void HTTPSERVICE::checkMethod()
{
	switch (m_buffer->getView().method())
	{
	case METHOD::POST:
		if (!m_buffer->getView().target().empty() &&
			std::all_of(m_buffer->getView().target().cbegin() + 1, m_buffer->getView().target().cend(),
				std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'), std::bind(std::less_equal<>(), std::placeholders::_1, '9'))))
		{
			switchPOSTInterface();
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
		}
		break;

	case METHOD::GET:
		if (!m_buffer->getView().target().empty())
		{
			testGET();
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
		}
		break;


	default:

		startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
		break;
	}
}




void HTTPSERVICE::switchPOSTInterface()
{
	int index{ -1 }, num{ 1 };
	switch (std::accumulate(std::make_reverse_iterator(m_buffer->getView().target().cend()), std::make_reverse_iterator(m_buffer->getView().target().cbegin() + 1), 0, [&index, &num](auto &sum, auto const ch)
	{
		if (++index)num *= 10;
		return sum += (ch - '0')*num;
	}))
	{

		//测试body解析，提取参数
	case INTERFACE::testBodyParse:
		testBody();
		break;


		//测试ping pong
	case INTERFACE::pingPong:
		testPingPong();
		break;


		//测试联合sql读取
	case INTERFACE::multiSqlReadSW:
		testmultiSqlReadSW();
		break;


		//测试redis 读取LOT_SIZE_STRING情况
	case INTERFACE::multiRedisReadLOT_SIZE_STRING:
		testMultiRedisReadLOT_SIZE_STRING();
		break;


		//测试redis 读取INTERGER情况
	case INTERFACE::multiRedisReadINTERGER:
		testMultiRedisReadINTERGER();
		break;


		//测试redis 读取ARRAY情况
	case INTERFACE::multiRedisReadARRAY:
		testMultiRedisReadARRAY();
		break;


		//测试body乱序解析情况
	case INTERFACE::testRandomBody:
		testRandomBody();
		break;



	case INTERFACE::testLogin:
		testLogin();
		break;


	case INTERFACE::testGetPublicKey:
		testGetPublicKey();
		break;


	case INTERFACE::testGetClientPublicKey:
		testGetClientPublicKey();
		break;


	case INTERFACE::testGetEncryptKey:
		testGetEncryptKey();
		break;


	case INTERFACE::testFirstTime:
		testFirstTime();
		break;


	case INTERFACE::testEncryptLogin:
		testEncryptLogin();
		break;


	case INTERFACE::testBusiness:
		m_business = std::bind(&HTTPSERVICE::testBusiness, this);
		testVerify<VERIFYINTERFACE>();
		break;


	case INTERFACE::testLogout:
		m_business = std::bind(&HTTPSERVICE::testLogout, this);
		testVerify<VERIFYINTERFACE>();
		break;


	case INTERFACE::testMultiPartFormData:
		testMultiPartFormData();
		//m_business = std::bind(&HTTPSERVICE::testMultiPartFormData, this);
		//testVerify<VERIFYINTERFACE>();
		break;


	case INTERFACE::successUpload:
		testSuccessUpload();
		break;



		//默认，不匹配任何接口情况
	default:
		startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
		break;
	}
}





void HTTPSERVICE::testBody()
{
	if (!hasBody)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
	
	if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);


	m_buffer->setBodyLen(m_len);


	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::test, STATICSTRING::testLen, STATICSTRING::parameter, STATICSTRING::parameterLen, STATICSTRING::number, STATICSTRING::numberLen))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
	
	startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
}



/*
POST /7 HTTP/1.1\r\n
Host: 192.168.80.128:8085\r\n
Content-Type: application/x-www-form-urlencoded\r\n
Content-Length: 46\r\n\r\n
number=1234&parameter=testDir&test=hello_world
*/

/*
POST /7 HTTP/1.1\r\n
Host: 192.168.80.128:8085\r\n
Content-Type: application/x-www-form-urlencoded\r\n
Content-Length: 46\r\n\r\n
number=1234&test=hello_world&parameter=testDir




POST /7?number&test=null&parameter= HTTP/1.1\r\n
Host: 192.168.80.128:8085\r\n\r\n



*/



void HTTPSERVICE::testRandomBody()
{
	if (hasPara)
	{
		if (!UrlDecodeWithTransChinese(m_buffer->getView().para().cbegin(), m_buffer->getView().para().size(), m_len))
			return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

		m_buffer->setBodyLen(m_len);

		const char *source{ &*m_buffer->getView().para().cbegin() };

		if (randomParseBody<HTTPINTERFACENUM::TESTRANDOMBODY, HTTPINTERFACENUM::TESTRANDOMBODY::max>(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), m_buffer->getBodyParaLen()))
		{
			const char **para{ m_buffer->bodyPara() };
			std::string_view testStr{}, parameterStr{}, numberStr{};
			if (*(para + HTTPINTERFACENUM::TESTRANDOMBODY::test))
			{
				std::string_view sw(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::test),
					std::distance(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::test), *(para + HTTPINTERFACENUM::TESTRANDOMBODY::test + 1)));
				testStr.swap(sw);
			}
			if (*(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter))
			{
				std::string_view sw(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter),
					std::distance(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter), *(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter + 1)));
				parameterStr.swap(sw);
			}
			if (*(para + HTTPINTERFACENUM::TESTRANDOMBODY::number))
			{
				std::string_view sw(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::number),
					std::distance(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::number), *(para + HTTPINTERFACENUM::TESTRANDOMBODY::number + 1)));
				numberStr.swap(sw);
			}

			startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
		}
	}



	else if(hasBody)
	{
		if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
			return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

		m_buffer->setBodyLen(m_len);

		const char *source{ &*m_buffer->getView().body().cbegin() };
		if (randomParseBody<HTTPINTERFACENUM::TESTRANDOMBODY, HTTPINTERFACENUM::TESTRANDOMBODY::max>(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), m_buffer->getBodyParaLen()))
		{
			const char **para{ m_buffer->bodyPara() };
			std::string_view testStr{}, parameterStr{}, numberStr{};
			if (*(para + HTTPINTERFACENUM::TESTRANDOMBODY::test))
			{
				std::string_view sw(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::test), 
					std::distance(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::test), *(para + HTTPINTERFACENUM::TESTRANDOMBODY::test + 1)));
				testStr.swap(sw);
			}
			if (*(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter))
			{
				std::string_view sw(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter), 
					std::distance(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter), *(para + HTTPINTERFACENUM::TESTRANDOMBODY::parameter + 1)));
				parameterStr.swap(sw);
			}
			if (*(para + HTTPINTERFACENUM::TESTRANDOMBODY::number))
			{
				std::string_view sw(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::number), 
					std::distance(*(para + HTTPINTERFACENUM::TESTRANDOMBODY::number), *(para + HTTPINTERFACENUM::TESTRANDOMBODY::number + 1)));
				numberStr.swap(sw);
			}

			startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
		}
	}
	else
	{
		//   没有Query Params ，也没有body，表明这个接口如果有参数的话，则参数全部为空的情况，自己做处理


		startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
	}
}






void HTTPSERVICE::testPingPong()
{
	int bodyLen{ m_buffer->getView().body().size() };
	if (!bodyLen)
	{
		startWrite(HTTPRESPONSEREADY::http10OKNoBody, HTTPRESPONSEREADY::http10OKNoBodyLen);
	}
	else
	{
		int needLen{ HTTPRESPONSEREADY::http10OKConyentLengthLen + stringLen(bodyLen) + HTTPRESPONSE::newlineLen };
		char *bodyBegin{ const_cast<char*>(&*(m_buffer->getView().body().cbegin())) }, *httpBegin{}, *httpEnd{};
		if (std::distance(m_buffer->getBuffer(), bodyBegin) >= needLen)
		{
			httpBegin = bodyBegin - needLen;
			httpEnd = bodyBegin + bodyLen;
			std::copy(HTTPRESPONSEREADY::http10OKConyentLength, HTTPRESPONSEREADY::http10OKConyentLength + HTTPRESPONSEREADY::http10OKConyentLengthLen, httpBegin);
			httpBegin += HTTPRESPONSEREADY::http10OKConyentLengthLen;

			if (bodyLen > 99999999)
				*httpBegin++ = bodyLen / 100000000 + '0';
			if (bodyLen > 9999999)
				*httpBegin++ = bodyLen / 10000000 % 10 + '0';
			if (bodyLen > 999999)
				*httpBegin++ = bodyLen / 1000000 % 10 + '0';
			if (bodyLen > 99999)
				*httpBegin++ = bodyLen / 100000 % 10 + '0';
			if (bodyLen > 9999)
				*httpBegin++ = bodyLen / 10000 % 10 + '0';
			if (bodyLen > 999)
				*httpBegin++ = bodyLen / 1000 % 10 + '0';
			if (bodyLen > 99)
				*httpBegin++ = bodyLen / 100 % 10 + '0';
			if (bodyLen > 9)
				*httpBegin++ = bodyLen / 10 % 10 + '0';
			*httpBegin++ = bodyLen % 10 + '0';

			std::copy(HTTPRESPONSE::newline, HTTPRESPONSE::newline + HTTPRESPONSE::newlineLen, httpBegin);
			httpBegin += HTTPRESPONSE::newlineLen;

			httpBegin -= needLen;

			startWrite(httpBegin, httpEnd- httpBegin);
		}
		else
		{
			char *newBuffer(m_charMemoryPool.getMemory(needLen + bodyLen));
			if (newBuffer)
			{
				httpBegin = newBuffer;

				std::copy(HTTPRESPONSEREADY::http10OKConyentLength, HTTPRESPONSEREADY::http10OKConyentLength + HTTPRESPONSEREADY::http10OKConyentLengthLen, httpBegin);
				httpBegin += HTTPRESPONSEREADY::http10OKConyentLengthLen;

				if (bodyLen > 99999999)
					*httpBegin++ = bodyLen / 100000000 + '0';
				if (bodyLen > 9999999)
					*httpBegin++ = bodyLen / 10000000 % 10 + '0';
				if (bodyLen > 999999)
					*httpBegin++ = bodyLen / 1000000 % 10 + '0';
				if (bodyLen > 99999)
					*httpBegin++ = bodyLen / 100000 % 10 + '0';
				if (bodyLen > 9999)
					*httpBegin++ = bodyLen / 10000 % 10 + '0';
				if (bodyLen > 999)
					*httpBegin++ = bodyLen / 1000 % 10 + '0';
				if (bodyLen > 99)
					*httpBegin++ = bodyLen / 100 % 10 + '0';
				if (bodyLen > 9)
					*httpBegin++ = bodyLen / 10 % 10 + '0';
				*httpBegin++ = bodyLen % 10 + '0';

				std::copy(HTTPRESPONSE::newline, HTTPRESPONSE::newline + HTTPRESPONSE::newlineLen, httpBegin);
				httpBegin += HTTPRESPONSE::newlineLen;

				std::copy(m_buffer->getView().body().cbegin(), m_buffer->getView().body().cend(), httpBegin);

				startWrite(newBuffer, needLen + bodyLen);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
	}
}




void HTTPSERVICE::testmultiSqlReadSW()
{
	std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*sqlRequest).get() };

	std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

	std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };
	
	command.clear(); 
	rowField.clear();
	result.clear();


	try
	{
		command.emplace_back(std::string_view(SQLCOMMAND::testSqlReadMemory, SQLCOMMAND::testSqlReadMemoryLen));

		std::get<1>(*sqlRequest) = 2;
		std::get<5>(*sqlRequest) = std::bind(&HTTPSERVICE::handleMultiSqlReadSW, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWSlave->insertSqlRequest(sqlRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertSql, HTTPRESPONSEREADY::httpFailToInsertSqlLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
	
}




void HTTPSERVICE::handleMultiSqlReadSW(bool result, ERRORMESSAGE em)
{
	if (result)
	{
		std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

		unsigned int commandSize{ std::get<1>(*sqlRequest) };

		std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

		std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

		if (rowField.empty() || result.empty() || (rowField.size() % 2))
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		
		std::vector<std::string_view>::const_iterator resultBegin{ result.cbegin() };
		std::vector<unsigned int>::const_iterator begin{ rowField.cbegin() }, end{ rowField.cend() }, iter{ begin };
		int rowNum{}, fieldNum{}, rowCount{}, fieldCount{}, index{ -1 };

		STLtreeFast &st1{ m_STLtreeFastVec[0] }, &st2{ m_STLtreeFastVec[1] },&st3{ m_STLtreeFastVec[2] }, &st4{ m_STLtreeFastVec[3] };

		if (!st1.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st2.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st3.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st4.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


		
		do
		{
			iter = begin;
			rowNum = *begin, fieldNum = *(begin + 1);

			rowCount = -1;
			while (++rowCount != rowNum)
			{
			
				if (!st1.put(STATICSTRING::id, STATICSTRING::id + STATICSTRING::idLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st1.put(STATICSTRING::name, STATICSTRING::name + STATICSTRING::nameLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st1.put(STATICSTRING::age, STATICSTRING::age + STATICSTRING::ageLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st1.put(STATICSTRING::province, STATICSTRING::province + STATICSTRING::provinceLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st1.put(STATICSTRING::city, STATICSTRING::city + STATICSTRING::cityLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st1.put(STATICSTRING::country, STATICSTRING::country + STATICSTRING::countryLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st1.put(STATICSTRING::phone, STATICSTRING::phone + STATICSTRING::phoneLen, &*resultBegin->cbegin(), &*resultBegin->cend()))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				++resultBegin;

				if (!st2.push_back(st1))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}


			
			if (!st3.put_child(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, st2))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st4.push_back(st3))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st3.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		

			begin += 2;
			//std::cout << "result " << ++index << " finished\n";
		}while(begin != end);

		if (!st1.put_child(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, st4))
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		

		makeSendJson(st1);
	}
	else
	{
		handleERRORMESSAGE(em);
	}
}



//  这里将foo 设置值为  set foo hello_world
//  测试从redis获取dLOT_SIZE_STRING类型数据

void HTTPSERVICE::testMultiRedisReadLOT_SIZE_STRING()
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::foo, REDISNAMESPACE::fooLen));
		commandSize.emplace_back(2);

		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadLOT_SIZE_STRING, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}




//这里返回，另外使用一个新发送函数
void HTTPSERVICE::handleMultiRedisReadLOT_SIZE_STRING(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };
	
	
	if (result)
	{
		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		if (!resultVec.empty())
		{
			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			makeSendJson(st1);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}
	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if(!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson(st1);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}
}




void HTTPSERVICE::testMultiRedisReadINTERGER()
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::age, REDISNAMESPACE::ageLen));
		commandSize.emplace_back(2);

		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadINTERGER, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}



void HTTPSERVICE::handleMultiRedisReadINTERGER(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	if (result)
	{
		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		if (!resultVec.empty())
		{
			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			makeSendJson(st1);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpNO_KEY, HTTPRESPONSEREADY::httpNO_KEYLen);
		}
	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson(st1);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}
}




void HTTPSERVICE::testMultiRedisReadARRAY()
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::mget, REDISNAMESPACE::mgetLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::foo, REDISNAMESPACE::fooLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::age, REDISNAMESPACE::ageLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::foo, REDISNAMESPACE::fooLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::age1, REDISNAMESPACE::age1Len));
		commandSize.emplace_back(5);

		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadARRAY, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}



void HTTPSERVICE::handleMultiRedisReadARRAY(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	if (result)
	{
		STLtreeFast &st1{ m_STLtreeFastVec[0] }, &st2{ m_STLtreeFastVec[1] };

		if (resultVec.size() == 4)
		{
			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			
			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[1].cbegin()), &*(resultVec[1].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[2].cbegin()), &*(resultVec[2].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[3].cbegin()), &*(resultVec[3].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.put_child(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, st2))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			

			makeSendJson(st1);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpNO_KEY, HTTPRESPONSEREADY::httpNO_KEYLen);
		}
	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson(st1);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}

}



//获取文件  应该支持若干个文件的获取，然后mget  chunk返回
void HTTPSERVICE::testGET()
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		if (std::equal(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), STATICSTRING::forwardSlash, STATICSTRING::forwardSlash + STATICSTRING::forwardSlashLen))
			command.emplace_back(std::string_view(STATICSTRING::loginHtml, STATICSTRING::loginHtmlLen));
		else
			command.emplace_back(m_buffer->getView().target());
		commandSize.emplace_back(2);

		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleTestGET, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}



void HTTPSERVICE::handleTestGET(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };
	if (result)
	{
		//首先判断制定文件是否存在，不存在返回错误
		if (em == ERRORMESSAGE::NO_KEY)
		{
			//std::cout << "no key\n";

			makeFileResPonse<REDISNOKEY>(MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen);

		}
		else
		{
			//std::cout << "has key\n";

			makeFileResPonse(MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen);

		}
	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (st1.clear() && st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					makeSendJson(st1);
				else
					startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}
}




void HTTPSERVICE::testLogin()
{
	if (!hasBody)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);
	
	
	if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	
	m_buffer->setBodyLen(m_len);

	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::username, STATICSTRING::usernameLen, STATICSTRING::password, STATICSTRING::passwordLen))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) },
		passwordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) };

	if (usernameView.empty() || passwordView.empty())
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	//hmget user password:usernameView  session:usernameView  sessionEX:usernameView
	// hmget user:usernameView password session
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();

	int passwordNeedLen{ STATICSTRING::passwordLen + 1 + usernameView.size() },
		sessionNeedLen{ STATICSTRING::sessionLen + 1 + usernameView.size() },
		sessionExpireNeedLen{ STATICSTRING::sessionExpireLen + 1 + usernameView.size() };
	int needLen{ passwordNeedLen + sessionNeedLen + sessionExpireNeedLen };
	char *buffer{ m_charMemoryPool.getMemory(needLen) };

	if (!buffer)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	char *bufferBegin{ buffer }, *passwordBegin{ buffer }, *sessionBegin{}, *sessionExpireBegin{};

	std::copy(STATICSTRING::password, STATICSTRING::password + STATICSTRING::passwordLen, bufferBegin);
	bufferBegin += STATICSTRING::passwordLen;
	*bufferBegin++ = ':';
	std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
	bufferBegin += usernameView.size();

	sessionBegin = bufferBegin;
	
	std::copy(STATICSTRING::session, STATICSTRING::session + STATICSTRING::sessionLen, bufferBegin);
	bufferBegin += STATICSTRING::sessionLen;
	*bufferBegin++ = ':';
	std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
	bufferBegin += usernameView.size();

	sessionExpireBegin = bufferBegin;

	std::copy(STATICSTRING::sessionExpire, STATICSTRING::sessionExpire + STATICSTRING::sessionExpireLen, bufferBegin);
	bufferBegin += STATICSTRING::sessionExpireLen;
	*bufferBegin++ = ':';
	std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
	bufferBegin += usernameView.size();


	try
	{
		command.emplace_back(std::string_view(STATICSTRING::hmget, STATICSTRING::hmgetLen));
		command.emplace_back(std::string_view(STATICSTRING::user, STATICSTRING::userLen));
		command.emplace_back(std::string_view(passwordBegin, passwordNeedLen));
		command.emplace_back(std::string_view(sessionBegin, sessionNeedLen));
		command.emplace_back(std::string_view(sessionExpireBegin, sessionExpireNeedLen));
		commandSize.emplace_back(5);

		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleTestLoginRedisSlave, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}



void HTTPSERVICE::handleTestLoginRedisSlave(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	if (result)
	{
		if (resultVec.size() != 3)
			return startWrite(HTTPRESPONSEREADY::httpNO_KEY, HTTPRESPONSEREADY::httpNO_KEYLen);;

		//获取对应的密码
		std::string_view &passwordView{ resultVec[0] }, &sessionView{ resultVec[1] }, &sessionExpireView{ resultVec[2] };

		if (passwordView.empty())
		{
			// 分redis中无此用户记录，去分sql进行查询确定到底有没有

			std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

			std::vector<std::string_view> &sqlCommand{ std::get<0>(*sqlRequest).get() };

			std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

			std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

			const char **BodyBuffer{ m_buffer->bodyPara() };

			std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

			sqlCommand.clear();
			rowField.clear();
			result.clear();

			int sqlNeedLen{ SQLCOMMAND::testTestLoginSQLLen + 1 + usernameView.size() };

			char *buffer{ m_charMemoryPool.getMemory(sqlNeedLen) }, *bufferBegin{};

			if (!buffer)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			bufferBegin = buffer;

			std::copy(SQLCOMMAND::testTestLoginSQL, SQLCOMMAND::testTestLoginSQL + SQLCOMMAND::testTestLoginSQLLen, bufferBegin);
			bufferBegin += SQLCOMMAND::testTestLoginSQLLen;

			std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
			bufferBegin += usernameView.size();

			*bufferBegin = '\'';


			try
			{
				sqlCommand.emplace_back(std::string_view(buffer, sqlNeedLen));

				std::get<1>(*sqlRequest) = 1;
				std::get<5>(*sqlRequest) = std::bind(&HTTPSERVICE::handleTestLoginSqlSlave, this, std::placeholders::_1, std::placeholders::_2);

				if (!m_multiSqlReadSWSlave->insertSqlRequest(sqlRequest))
					startWrite(HTTPRESPONSEREADY::httpFailToInsertSql, HTTPRESPONSEREADY::httpFailToInsertSqlLen);
			}
			catch (const std::exception &e)
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			return;
		}



		//比对密码
		const char **BodyBuffer{ m_buffer->bodyPara() };

		if (!std::equal(*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password), *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1), passwordView.cbegin(), passwordView.cend()))
			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);


		//密码匹配情况下查询有没有session
		//hmset  user  session:usernameView   session  sessionEX:usernameView   sessionExpire
		if (sessionView.empty() || sessionExpireView.empty())
		{
			//没有的情况下生成一个
			//生成session   随机24字符串
			//取前24位作为session
			std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

			m_sessionClock = std::chrono::system_clock::now();

			m_sessionClock += std::chrono::hours(24 * 31);

			m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

			std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
			
			std::string_view &user{ command[1] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

			int sessionExpireValueNeedLen{};

			char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};
			
			if (!buffer)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


			std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

			std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
			std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

			writeCommand.clear();
			writeCommandSize.clear();

			try
			{
				writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
				writeCommand.emplace_back(user);
				writeCommand.emplace_back(sessionView);
				writeCommand.emplace_back(std::string_view(m_randomString,24));
				writeCommand.emplace_back(sessionExpireView);
				writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
				std::get<1>(*redisWrite)= 1;
				writeCommandSize.emplace_back(6);
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);;
			}


			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
			m_sessionLen = 24;


			//返回包含set-cookie的http消息给客户端

			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

			int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + 24 + usernameView.size() + 1 + STATICSTRING::expiresLen + 1 + STATICSTRING::httpSetCookieExpireValueLen + 1 + STATICSTRING::pathLen + 2 };

			
			char *sendBuffer{};
			unsigned int sendLen{};


			if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
				[this, usernameView](char *&bufferBegin)
			{
				if (!bufferBegin)
					return;

				std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
				bufferBegin += MAKEJSON::SetCookieLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
				bufferBegin += STATICSTRING::sessionIDLen;

				*bufferBegin++ = '=';

				std::copy(m_SessionID.get(), m_SessionID.get() + m_sessionLen, bufferBegin);
				bufferBegin += m_sessionLen;

				std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
				bufferBegin += usernameView.size();

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::expires, STATICSTRING::expires + STATICSTRING::expiresLen, bufferBegin);
				bufferBegin += STATICSTRING::expiresLen;

				*bufferBegin++ = '=';

				//返回包含set-cookie的http消息给客户端
					//格林威治时间与北京时间相差四小时
					// DAY, DD MMM YYYY HH:MM:SS GMT
				m_sessionGMTClock = m_sessionClock + std::chrono::hours(4);

				m_sessionGMTTime = std::chrono::system_clock::to_time_t(m_sessionGMTClock);

				m_tmGMT = localtime(&m_sessionGMTTime);

				m_tmGMT->tm_year += 1900;

				std::copy(STATICSTRING::tm_wday[m_tmGMT->tm_wday], STATICSTRING::tm_wday[m_tmGMT->tm_wday] + STATICSTRING::tm_wdayLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_wdayLen;

				*bufferBegin++ = ',';
				*bufferBegin++ = ' ';


				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_mday], STATICSTRING::tm_sec[m_tmGMT->tm_mday] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ' ';

				std::copy(STATICSTRING::tm_mon[m_tmGMT->tm_mon], STATICSTRING::tm_mon[m_tmGMT->tm_mon] + STATICSTRING::tm_monLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_monLen;

				*bufferBegin++ = ' ';

				*bufferBegin++ = m_tmGMT->tm_year / 1000 + '0';
				*bufferBegin++ = m_tmGMT->tm_year / 100 % 10 + '0';
				*bufferBegin++ = m_tmGMT->tm_year / 10 % 10 + '0';
				*bufferBegin++ = m_tmGMT->tm_year % 10 + '0';

				*bufferBegin++ = ' ';

				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_hour], STATICSTRING::tm_sec[m_tmGMT->tm_hour] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_min], STATICSTRING::tm_sec[m_tmGMT->tm_min] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_sec], STATICSTRING::tm_sec[m_tmGMT->tm_sec] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ' ';

				std::copy(STATICSTRING::GMT, STATICSTRING::GMT + STATICSTRING::GMTLen, bufferBegin);
				bufferBegin += STATICSTRING::GMTLen;

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
				bufferBegin += STATICSTRING::pathLen;

				*bufferBegin++ = '=';

				*bufferBegin++ = '/';

			}, 
				MAKEJSON::SetCookieLen + 1 + setCookValueLen,nullptr,
				MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
			{
				startWrite(sendBuffer, sendLen);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			return;
		}


		//比对session过期时间
		int index{ -1 };
		time_t baseNum{ 1 };

		m_sessionTime = 0;

		std::accumulate(std::make_reverse_iterator(sessionExpireView.cend()), std::make_reverse_iterator(sessionExpireView.cbegin()), &m_sessionTime, [&index, &baseNum](auto &sum, const auto ch)
		{
			if (++index)baseNum *= 10;
			*sum += (ch - '0')*baseNum;
			return sum;
		});

		m_thisClock = std::chrono::system_clock::now();

		m_thisTime = std::chrono::system_clock::to_time_t(m_thisClock);

		if (m_thisTime >= m_sessionTime)
		{
			//session过期，需要重新生成
			std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

			m_sessionClock = m_thisClock + std::chrono::hours(24 * 31);

			m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

			std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

			std::string_view &user{ command[1] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

			int sessionExpireValueNeedLen{};

			char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

			if (!buffer)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


			std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

			std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
			std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

			writeCommand.clear();
			writeCommandSize.clear();

			try
			{
				writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
				writeCommand.emplace_back(user);
				writeCommand.emplace_back(sessionView);
				writeCommand.emplace_back(std::string_view(m_randomString, 24));
				writeCommand.emplace_back(sessionExpireView);
				writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
				std::get<1>(*redisWrite) = 1;
				writeCommandSize.emplace_back(6);
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);;
			}


			//  更新redis中的session  和  session过期时间
			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
			m_sessionLen = 24;

		}
		else
		{
			//没有过期的话返回

			try
			{
				if (sessionView.size() > m_sessionMemory)
				{
					m_SessionID.reset(new char[sessionView.size()]);
					m_sessionMemory = sessionView.size();
				}
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			std::copy(sessionView.cbegin(), sessionView.cend(), m_SessionID.get());
			m_sessionLen = sessionView.size();
		}




		//返回包含set-cookie的http消息给客户端

		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		if (!st1.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

		int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + 24 + usernameView.size() + 1 + STATICSTRING::expiresLen + 1 + STATICSTRING::httpSetCookieExpireValueLen + 1 + STATICSTRING::pathLen + 2 };


		char *sendBuffer{};
		unsigned int sendLen{};


		if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
			[this, usernameView](char *&bufferBegin)
		{
			if (!bufferBegin)
				return;

			std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
			bufferBegin += MAKEJSON::SetCookieLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
			bufferBegin += STATICSTRING::sessionIDLen;

			*bufferBegin++ = '=';

			std::copy(m_SessionID.get(), m_SessionID.get() + m_sessionLen, bufferBegin);
			bufferBegin += m_sessionLen;

			std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
			bufferBegin += usernameView.size();

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::expires, STATICSTRING::expires + STATICSTRING::expiresLen, bufferBegin);
			bufferBegin += STATICSTRING::expiresLen;

			*bufferBegin++ = '=';

			//返回包含set-cookie的http消息给客户端
			//格林威治时间与北京时间相差四小时
			// DAY, DD MMM YYYY HH:MM:SS GMT
			m_sessionGMTTime = m_sessionTime + STATICSTRING::fourHourSec;

			m_tmGMT = localtime(&m_sessionGMTTime);

			m_tmGMT->tm_year += 1900;

			std::copy(STATICSTRING::tm_wday[m_tmGMT->tm_wday], STATICSTRING::tm_wday[m_tmGMT->tm_wday] + STATICSTRING::tm_wdayLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_wdayLen;

			*bufferBegin++ = ',';
			*bufferBegin++ = ' ';


			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_mday], STATICSTRING::tm_sec[m_tmGMT->tm_mday] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_mon[m_tmGMT->tm_mon], STATICSTRING::tm_mon[m_tmGMT->tm_mon] + STATICSTRING::tm_monLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_monLen;

			*bufferBegin++ = ' ';

			*bufferBegin++ = m_tmGMT->tm_year / 1000 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 100 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 10 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year % 10 + '0';

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_hour], STATICSTRING::tm_sec[m_tmGMT->tm_hour] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_min], STATICSTRING::tm_sec[m_tmGMT->tm_min] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_sec], STATICSTRING::tm_sec[m_tmGMT->tm_sec] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::GMT, STATICSTRING::GMT + STATICSTRING::GMTLen, bufferBegin);
			bufferBegin += STATICSTRING::GMTLen;

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
			bufferBegin += STATICSTRING::pathLen;

			*bufferBegin++ = '=';

			*bufferBegin++ = '/';

		},
			MAKEJSON::SetCookieLen + 1 + setCookValueLen,nullptr,
			MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
			MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
			MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
			MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
		{
			startWrite(sendBuffer, sendLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}


	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson(st1);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}
}







void HTTPSERVICE::handleTestLoginSqlSlave(bool result, ERRORMESSAGE em)
{
	if (result)
	{
		std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

		unsigned int commandSize{ std::get<1>(*sqlRequest) };

		std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

		std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

		if (rowField.empty() || result.empty() || (rowField.size() % 2))   //  无此记录
			return startWrite(HTTPRESPONSEREADY::httpNoRecordInSql, HTTPRESPONSEREADY::httpNoRecordInSqlLen);

		const char **BodyBuffer{ m_buffer->bodyPara() };

		std::string_view CheckpasswordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) },
			&RealpasswordView{ result[0] };

		if (!std::equal(CheckpasswordView.cbegin(), CheckpasswordView.cend(), RealpasswordView.cbegin(), RealpasswordView.cend()))
		{
			do
			{
				//取前24位作为session
				std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

				m_sessionClock = m_thisClock + std::chrono::hours(24 * 31);

				m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

				std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

				std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

				std::string_view &user{ command[1] }, &passwordView{ command[2] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

				int sessionExpireValueNeedLen{};

				char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

				if (!buffer)
					break;


				std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

				std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
				std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

				writeCommand.clear();
				writeCommandSize.clear();

				try
				{
					writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
					writeCommand.emplace_back(user);
					writeCommand.emplace_back(passwordView);
					writeCommand.emplace_back(RealpasswordView);
					writeCommand.emplace_back(sessionView);
					writeCommand.emplace_back(std::string_view(m_randomString, 24));
					writeCommand.emplace_back(sessionExpireView);
					writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
					std::get<1>(*redisWrite) = 1;
					writeCommandSize.emplace_back(8);
				}
				catch (const std::exception &e)
				{
					break;
				}


				//  更新redis中的session  和  session过期时间
				if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
					break;

			} while (false);


			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);
		}


		//取前24位作为session
		std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

		m_sessionClock = m_thisClock + std::chrono::hours(24 * 31);

		m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

		std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

		std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

		std::string_view &user{ command[1] }, &passwordView{ command[2] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

		int sessionExpireValueNeedLen{};

		char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

		if (!buffer)
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


		std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

		std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
		std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

		writeCommand.clear();
		writeCommandSize.clear();

		try
		{
			writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
			writeCommand.emplace_back(user);
			writeCommand.emplace_back(passwordView);
			writeCommand.emplace_back(RealpasswordView);
			writeCommand.emplace_back(sessionView);
			writeCommand.emplace_back(std::string_view(m_randomString, 24));
			writeCommand.emplace_back(sessionExpireView);
			writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
			std::get<1>(*redisWrite) = 1;
			writeCommandSize.emplace_back(8);
		}
		catch (const std::exception &e)
		{
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}


		//  更新redis中的session  和  session过期时间
		if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
			return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
		m_sessionLen = 24;
		

		//返回包含set-cookie的http消息给客户端

		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		if (!st1.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

		int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + 24 + usernameView.size() + 1 + STATICSTRING::expiresLen + 1 + STATICSTRING::httpSetCookieExpireValueLen + 1 + STATICSTRING::pathLen + 2 };


		char *sendBuffer{};
		unsigned int sendLen{};



		if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
			[this, usernameView](char *&bufferBegin)
		{
			if (!bufferBegin)
				return;

			std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
			bufferBegin += MAKEJSON::SetCookieLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
			bufferBegin += STATICSTRING::sessionIDLen;

			*bufferBegin++ = '=';

			std::copy(m_SessionID.get(), m_SessionID.get()+m_sessionLen, bufferBegin);
			bufferBegin += m_sessionLen;

			std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
			bufferBegin += usernameView.size();

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::expires, STATICSTRING::expires + STATICSTRING::expiresLen, bufferBegin);
			bufferBegin += STATICSTRING::expiresLen;

			*bufferBegin++ = '=';

			//返回包含set-cookie的http消息给客户端
				//格林威治时间与北京时间相差四小时
				// DAY, DD MMM YYYY HH:MM:SS GMT
			m_sessionGMTTime = m_sessionTime + STATICSTRING::fourHourSec;

			m_tmGMT = localtime(&m_sessionGMTTime);

			m_tmGMT->tm_year += 1900;

			std::copy(STATICSTRING::tm_wday[m_tmGMT->tm_wday], STATICSTRING::tm_wday[m_tmGMT->tm_wday] + STATICSTRING::tm_wdayLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_wdayLen;

			*bufferBegin++ = ',';
			*bufferBegin++ = ' ';


			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_mday], STATICSTRING::tm_sec[m_tmGMT->tm_mday] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_mon[m_tmGMT->tm_mon], STATICSTRING::tm_mon[m_tmGMT->tm_mon] + STATICSTRING::tm_monLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_monLen;

			*bufferBegin++ = ' ';

			*bufferBegin++ = m_tmGMT->tm_year / 1000 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 100 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 10 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year % 10 + '0';

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_hour], STATICSTRING::tm_sec[m_tmGMT->tm_hour] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_min], STATICSTRING::tm_sec[m_tmGMT->tm_min] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_sec], STATICSTRING::tm_sec[m_tmGMT->tm_sec] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::GMT, STATICSTRING::GMT + STATICSTRING::GMTLen, bufferBegin);
			bufferBegin += STATICSTRING::GMTLen;

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
			bufferBegin += STATICSTRING::pathLen;

			*bufferBegin++ = '=';

			*bufferBegin++ = '/';

		},
			MAKEJSON::SetCookieLen + 1 + setCookValueLen,nullptr,
			MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
			MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
			MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
			MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
		{
			startWrite(sendBuffer, sendLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}

	}
	else
	{
		handleERRORMESSAGE(em);
	}
}



void HTTPSERVICE::testLogout()
{
	// 向redis发送清除session 命令

	std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

	std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
	std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

	writeCommand.clear();
	writeCommandSize.clear();

	const char *userNameBegin{ *(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) },
		*userNameEnd{ *(m_verifyData.get() + VerifyDataPos::httpSessionUserNameEnd) };

	std::string_view userNameView{ userNameBegin ,userNameEnd - userNameBegin };

	char *sessionBuf{ m_charMemoryPool.getMemory(STATICSTRING::sessionLen + 2 + 2 * userNameView.size() + STATICSTRING::sessionExpireLen) };

	if (!sessionBuf)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	char *bufBegin{ sessionBuf };
	std::copy(STATICSTRING::session, STATICSTRING::session + STATICSTRING::sessionLen, bufBegin);
	bufBegin += STATICSTRING::sessionLen;

	*bufBegin++ = ':';
	std::copy(userNameView.cbegin(), userNameView.cend(), bufBegin);
	bufBegin += userNameView.size();

	char *sessionExBegin{ bufBegin };

	std::copy(STATICSTRING::sessionExpire, STATICSTRING::sessionExpire + STATICSTRING::sessionExpireLen, sessionExBegin);
	sessionExBegin += STATICSTRING::sessionExpireLen;
	*sessionExBegin++ = ':';
	std::copy(userNameView.cbegin(), userNameView.cend(), sessionExBegin);
	sessionExBegin += userNameView.size();


	try
	{
		writeCommand.emplace_back(std::string_view(STATICSTRING::hdel, STATICSTRING::hdelLen));
		writeCommand.emplace_back(std::string_view(STATICSTRING::user, STATICSTRING::userLen));
		writeCommand.emplace_back(std::string_view(sessionBuf, bufBegin - sessionBuf));
		writeCommand.emplace_back(std::string_view(bufBegin, sessionExBegin - bufBegin));
		std::get<1>(*redisWrite) = 1;
		writeCommandSize.emplace_back(4);
	}
	catch (const std::exception &e)
	{
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}


	if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
	{
		return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}

	m_thisTime = 0, m_sessionLen = 0;


	//////////////  向客户端发出清除cookie消息，让客户端重新进行登陆操作

	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


	const char *sessionBegin{ *(m_verifyData.get() + VerifyDataPos::httpAllSessionBegin) },
		*cookieEnd{ *(m_verifyData.get() + VerifyDataPos::httpAllSessionEnd) };

	char *sendBuffer{};
	unsigned int sendLen{};

	int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + cookieEnd - sessionBegin + 1 + STATICSTRING::Max_AgeLen + 3 + STATICSTRING::pathLen + 2 };

	if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
		[this, sessionBegin, cookieEnd](char *&bufferBegin)
	{
		if (!bufferBegin)
			return;

		std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
		bufferBegin += MAKEJSON::SetCookieLen;

		*bufferBegin++ = ':';

		std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
		bufferBegin += STATICSTRING::sessionIDLen;

		*bufferBegin++ = '=';

		std::copy(sessionBegin, cookieEnd, bufferBegin);
		bufferBegin += cookieEnd - sessionBegin;

		*bufferBegin++ = ';';

		std::copy(STATICSTRING::Max_Age, STATICSTRING::Max_Age + STATICSTRING::Max_AgeLen, bufferBegin);
		bufferBegin += STATICSTRING::Max_AgeLen;

		*bufferBegin++ = '=';

		*bufferBegin++ = '0';

		*bufferBegin++ = ';';

		std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
		bufferBegin += STATICSTRING::pathLen;

		*bufferBegin++ = '=';

		*bufferBegin++ = '/';
	},
		MAKEJSON::SetCookieLen + 1 + setCookValueLen, nullptr,
		MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
		MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
		MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
		MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
	{
		startWrite(sendBuffer, sendLen);
	}
	else
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}

}


//  

void HTTPSERVICE::testGetPublicKey()
{
	if(m_hasMakeRandom)
		return startWrite(HTTPRESPONSEREADY::httpFailToVerify, HTTPRESPONSEREADY::httpFailToVerifyLen);

	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put<TRANSFORMTYPE>(STATICSTRING::publicKey, STATICSTRING::publicKey + STATICSTRING::publicKeyLen, m_publicKeyBegin, m_publicKeyEnd))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

	if (!st1.put(STATICSTRING::randomString, STATICSTRING::randomString + STATICSTRING::randomStringLen, m_randomString, m_randomString + randomStringLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson<TRANSFORMTYPE>(st1);

	m_hasMakeRandom = true;
}




void HTTPSERVICE::testGetClientPublicKey()
{
	if (!m_hasMakeRandom)
		return startWrite(HTTPRESPONSEREADY::httpFailToVerify, HTTPRESPONSEREADY::httpFailToVerifyLen);

	if (!hasBody)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);


	m_buffer->setBodyLen(m_len);


	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::publicKey, STATICSTRING::publicKeyLen,
		STATICSTRING::hashValue, STATICSTRING::hashValueLen, STATICSTRING::randomString, STATICSTRING::randomStringLen))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);


	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view publicKeyView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::publicKey),*(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::publicKey + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::publicKey) },
		hashValueView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::hashValue),*(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::hashValue + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::hashValue) },
		randomStringView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::randomString),*(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::randomString + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::randomString) };


	//校对hash值是否为客户端所发
	
	///////////////////////////////////////////////////////////

	if(hashValueView.size()!= STATICSTRING::serverHashLen)
		return startWrite(HTTPRESPONSEREADY::http10InvaildHash, HTTPRESPONSEREADY::http10InvaildHashLen);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//先判断服务器随机字符串长度与RSA_size的关系 
	int checkStatus{};

	if (randomStringLen < STATICSTRING::serverRSASize)
		checkStatus = 1;
	else if (randomStringLen == STATICSTRING::serverRSASize)
		checkStatus = 2;
	else
		checkStatus = 3;

	char *randomBuff{}, *randomRSAEnd{};


	int RSAIndex{}, RSALen{};
	if (checkStatus != 3)
		RSAIndex = 1;
	else
		RSAIndex = randomStringLen / STATICSTRING::serverRSASize + 1;

	//RSA加密字符串总长度，后面有用
	RSALen = RSAIndex * STATICSTRING::serverRSASize;
	int ramdomBuffLen{ RSALen + STATICSTRING::serverHashLen + STATICSTRING::serverHashLen / 2 };
	randomBuff = m_charMemoryPool.getMemory(ramdomBuffLen);


	if (!randomBuff)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	randomRSAEnd = randomBuff + RSALen;

	std::copy(m_randomString, m_randomString + randomStringLen, randomBuff);

	if (checkStatus != 2)
		std::fill(randomBuff + randomStringLen, randomRSAEnd, 0);


	char *transBegin{ randomBuff }, *transEnd{ randomRSAEnd }, *iter{ transBegin }, *iterEnd{ transBegin };

	int everyLen{}, index{ -1 };
	while (iter != transEnd)
	{
		everyLen = std::distance(iter, transEnd);
		everyLen = everyLen >= STATICSTRING::helloWorldLen ? STATICSTRING::helloWorldLen : everyLen;
		iterEnd = iter + everyLen;

		switch (++index % 4)
		{
		case 0:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::plus<>());
			break;
		case 1:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::minus<>());
			break;
		case 2:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::multiplies<>());
			break;
		case 3:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::divides<>());
			break;
		default:
			break;
		}
		iter = iterEnd;
	}


	iter = randomBuff, iterEnd = randomRSAEnd;

	int resultLen{};

	while (iter != iterEnd)
	{
		resultLen = RSA_public_encrypt(STATICSTRING::serverRSASize, reinterpret_cast<unsigned char*>(iter), reinterpret_cast<unsigned char*>(iter), m_rsaPublic, RSA_NO_PADDING);
		if (resultLen != STATICSTRING::serverRSASize)
			return startWrite(HTTPRESPONSEREADY::httpRSAencryptFail, HTTPRESPONSEREADY::httpRSAencryptFailLen);
		iter += resultLen;
	}

	//加密完毕转md5
	MD5_Init(&m_ctx);
	MD5_Update(&m_ctx, randomBuff, RSALen);
	MD5_Final(reinterpret_cast<unsigned char*>(randomRSAEnd), &m_ctx);


	transBegin = randomRSAEnd, transEnd = transBegin + STATICSTRING::serverHashLen / 2;

	iter = iterEnd = transEnd;
	int tens{}, ones{};
	while (transBegin != transEnd)
	{
		tens = *transBegin / 16;
		ones = *transBegin % 16;
		*iterEnd++ = tens < 10 ? (tens + '0') : (tens - 10 + 'A');
		*iterEnd++ = ones < 10 ? (ones + '0') : (ones - 10 + 'A');
	}

	
	//完全匹配用这个
	if (!std::equal(iter, iterEnd, hashValueView.cbegin(), hashValueView.cend()))
	{
		return startWrite(HTTPRESPONSEREADY::http10InvaildHash, HTTPRESPONSEREADY::http10InvaildHashLen);
		m_hasMakeRandom = false;
	}

	//不区分大小写用这个
	//if (!std::equal(iter, iterEnd, hashValueView.cbegin(), hashValueView.cend(),std::bind(std::equal_to<>(),std::bind(::toupper,std::placeholders::_2), std::placeholders::_1)))
	//	return startWrite(HTTPRESPONSEREADY::http10InvaildHash, HTTPRESPONSEREADY::http10InvaildHashLen);



	// 尝试加载客户端公钥
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if (m_bio)
	{
		BIO_free_all(m_bio);
		m_bio = nullptr;
	}

	if (m_rsaClientPublic)
	{
		RSA_free(m_rsaClientPublic);
		m_rsaClientPublic = nullptr;
	}

	if (!(m_bio = BIO_new_mem_buf(&*publicKeyView.cbegin(), publicKeyView.size())))       //从字符串读取RSA公钥
		return startWrite(HTTPRESPONSEREADY::http10WrongPublicKey, HTTPRESPONSEREADY::http10WrongPublicKeyLen);


	m_rsaClientPublic = PEM_read_bio_RSA_PUBKEY(m_bio, nullptr, nullptr, nullptr);   //从bio结构中得到rsa结构
	if (!m_rsaClientPublic)
	{
		BIO_free_all(m_bio);
		return startWrite(HTTPRESPONSEREADY::http10WrongPublicKey, HTTPRESPONSEREADY::http10WrongPublicKeyLen);
	}

	m_rsaClientSize = RSA_size(m_rsaClientPublic);

	BIO_free_all(m_bio);
	m_bio = nullptr;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 将客户端的随机字符串进行处理后用客户端的公钥进行加密，然后返回结果哈希值给客户端
	// 验证客户端真正的公钥传递到真正的服务端
	// 先判断字符串分段，再根据分段进行


	if (randomStringView.size() < m_rsaClientSize)
		checkStatus = 1;
	else if (randomStringView.size() == m_rsaClientSize)
		checkStatus = 2;
	else
		checkStatus = 3;


	if (checkStatus != 3)
		RSAIndex = 1;
	else
		RSAIndex = randomStringLen / m_rsaClientSize + 1;

	//RSA加密字符串总长度，后面有用
	RSALen = RSAIndex * m_rsaClientSize;

	int clientRamdomBuffLen{ RSALen + STATICSTRING::serverHashLen + STATICSTRING::serverHashLen / 2 };

	if (clientRamdomBuffLen > ramdomBuffLen)
	{
		randomBuff = m_charMemoryPool.getMemory(clientRamdomBuffLen);
		if (!randomBuff)
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}


	randomRSAEnd = randomBuff + RSALen;

	std::copy(randomStringView.cbegin(), randomStringView.cend(), randomBuff);

	if (checkStatus != 2)
		std::fill(randomBuff + randomStringView.size(), randomRSAEnd, 0);


	transBegin = randomBuff , transEnd = randomRSAEnd , iter = transBegin , iterEnd = transBegin ;

	index = -1 ;
	while (iter != transEnd)
	{
		everyLen = std::distance(iter, transEnd);
		everyLen = everyLen >= STATICSTRING::helloWorldLen ? STATICSTRING::helloWorldLen : everyLen;
		iterEnd = iter + everyLen;

		switch (++index % 4)
		{
		case 0:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::plus<>());
			break;
		case 1:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::minus<>());
			break;
		case 2:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::multiplies<>());
			break;
		case 3:
			std::transform(iter, iterEnd, STATICSTRING::helloWorld, iter, std::divides<>());
			break;
		default:
			break;
		}
		iter = iterEnd;
	}


	iter = randomBuff, iterEnd = randomRSAEnd;

	while (iter != iterEnd)
	{
		resultLen = RSA_public_encrypt(m_rsaClientSize, reinterpret_cast<unsigned char*>(iter), reinterpret_cast<unsigned char*>(iter), m_rsaPublic, RSA_NO_PADDING);
		if (resultLen != m_rsaClientSize)
			return startWrite(HTTPRESPONSEREADY::httpRSAencryptFail, HTTPRESPONSEREADY::httpRSAencryptFailLen);
		iter += resultLen;
	}


	//加密完毕转md5
	MD5_Init(&m_ctx);
	MD5_Update(&m_ctx, randomBuff, RSALen);
	MD5_Final(reinterpret_cast<unsigned char*>(randomRSAEnd), &m_ctx);


	transBegin = randomRSAEnd, transEnd = transBegin + STATICSTRING::serverHashLen / 2;

	iter = iterEnd = transEnd;
	while (transBegin != transEnd)
	{
		tens = *transBegin / 16;
		ones = *transBegin % 16;
		*iterEnd++ = tens < 10 ? (tens + '0') : (tens - 10 + 'A');
		*iterEnd++ = ones < 10 ? (ones + '0') : (ones - 10 + 'A');
	}


	// iter    iterEnd返回给客户端校验哈希值

	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(STATICSTRING::hashValue, STATICSTRING::hashValue + STATICSTRING::hashValueLen, iter, iterEnd))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1);

	m_hasMakeRandom = false;
}





void HTTPSERVICE::testFirstTime()
{
	m_firstTime = std::chrono::system_clock::to_time_t(std::chrono::high_resolution_clock::now());

	return startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
}



void HTTPSERVICE::testGetEncryptKey()
{
	if(!m_firstTime || !hasBody || !UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	
	m_buffer->setBodyLen(m_len);


	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::encryptKey, STATICSTRING::encryptKeyLen))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);


	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view encryptKeyView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETENCRYPTKEY::encryptKey),*(BodyBuffer + HTTPINTERFACENUM::TESTGETENCRYPTKEY::encryptKey + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETENCRYPTKEY::encryptKey) };

	if (encryptKeyView.size() != STATICSTRING::serverRSASize)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);


	int resultLen = RSA_private_decrypt(STATICSTRING::serverRSASize, reinterpret_cast<unsigned char*>(const_cast<char*>(&*encryptKeyView.cbegin())),
		reinterpret_cast<unsigned char*>(const_cast<char*>(&*encryptKeyView.cbegin())), m_rsaPrivate, RSA_NO_PADDING);

	if (resultLen != STATICSTRING::serverRSASize)
		return startWrite(HTTPRESPONSEREADY::httpRSAdecryptFail, HTTPRESPONSEREADY::httpRSAdecryptFailLen);

	std::string_view::const_iterator AESBegin{ encryptKeyView.cbegin() }, AESEnd{}, keyTimeBegin{}, keyTimeEnd{};

	if((AESEnd=std::find(AESBegin, encryptKeyView.cend(),0))== encryptKeyView.cend())
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	if((keyTimeBegin=std::find_if(AESEnd, encryptKeyView.cend(),std::bind(std::logical_and<>(),std::bind(std::greater_equal<>(),std::placeholders::_1,'0'),
		std::bind(std::less_equal<>(), std::placeholders::_1, '9'))))== encryptKeyView.cend())
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	keyTimeEnd = std::find_if_not(keyTimeBegin, encryptKeyView.cend(), std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'),
		std::bind(std::less_equal<>(), std::placeholders::_1, '9')));

	int AESLen{ AESEnd - AESBegin };

	if(AESLen!=16 || AESLen!=24 || AESLen!=32)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	int index{ -1 };
	time_t baseNum{ 1 }, ten{ 10 };
	m_thisTime = std::accumulate(std::make_reverse_iterator(keyTimeEnd), std::make_reverse_iterator(keyTimeEnd), static_cast<time_t>(0), [&index, &baseNum, &ten](time_t &sum, auto const ch)
	{
		if (++index)baseNum *= ten;
		sum += (ch - '0')*baseNum;
		return sum;
	});
	
	if(m_thisTime <= m_firstTime)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	if(AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(&*AESBegin), AESLen * 8, &aes_encryptkey))
		return startWrite(HTTPRESPONSEREADY::httpAESsetKeyFail, HTTPRESPONSEREADY::httpAESsetKeyFailLen);

	if (AES_set_encrypt_key(reinterpret_cast<const unsigned char*>(&*AESBegin), AESLen * 8, &aes_decryptkey))
		return startWrite(HTTPRESPONSEREADY::httpAESsetKeyFail, HTTPRESPONSEREADY::httpAESsetKeyFailLen);


	m_clientAESLen = AESLen;

	std::copy(AESBegin, AESEnd, m_clientAES.get());




	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1);
}




void HTTPSERVICE::testEncryptLogin()
{
	if(!m_clientAESLen)
		return startWrite(HTTPRESPONSEREADY::httpFailToVerify, HTTPRESPONSEREADY::httpFailToVerifyLen);

	if(!hasBody || !UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	m_buffer->setBodyLen(m_len);

	if(std::distance(m_buffer->getView().body().cbegin(), m_buffer->getView().body().cbegin()+ m_buffer->getBodyLen()) % 16)
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	char *decryptBegin{ const_cast<char*>(&*m_buffer->getView().body().cbegin()) }, *decryptEnd{ const_cast<char*>(&*m_buffer->getView().body().cbegin()) + m_buffer->getBodyLen() };

	while (decryptBegin != decryptEnd)
	{
		AES_ecb_encrypt(const_cast<const unsigned char*>(reinterpret_cast<unsigned char*>(decryptBegin)), reinterpret_cast<unsigned char*>(decryptBegin), &aes_decryptkey, AES_DECRYPT);
		decryptBegin += 16;
	}

	std::string_view::const_iterator iterNotZero{ std::find_if(m_buffer->getView().body().cbegin(),m_buffer->getView().body().cend(),std::bind(std::not_equal_to<>(),std::placeholders::_1,0)) };

	m_buffer->setBodyFrontLen(std::distance(m_buffer->getView().body().cbegin(), iterNotZero));

	const char *source{ &*m_buffer->getView().body().cbegin()+ m_buffer->getBodyFrontLen() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::username, STATICSTRING::usernameLen, STATICSTRING::password, STATICSTRING::passwordLen))
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);

	//////////////////////////////////////////////////////////////////////////////////
	//接下来处理逻辑和testLogin的类似

	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) },
		passwordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) };

	if (usernameView.empty() || passwordView.empty())
		return startWrite(HTTPRESPONSEREADY::http10invaild, HTTPRESPONSEREADY::http10invaildLen);


	//hmget user password:usernameView  session:usernameView  sessionEX:usernameView
	// hmget user:usernameView password session
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();

	int passwordNeedLen{ STATICSTRING::passwordLen + 1 + usernameView.size() },
		sessionNeedLen{ STATICSTRING::sessionLen + 1 + usernameView.size() },
		sessionExpireNeedLen{ STATICSTRING::sessionExpireLen + 1 + usernameView.size() };
	int needLen{ passwordNeedLen + sessionNeedLen + sessionExpireNeedLen };
	char *buffer{ m_charMemoryPool.getMemory(needLen) };

	if (!buffer)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	char *bufferBegin{ buffer }, *passwordBegin{ buffer }, *sessionBegin{}, *sessionExpireBegin{};

	std::copy(STATICSTRING::password, STATICSTRING::password + STATICSTRING::passwordLen, bufferBegin);
	bufferBegin += STATICSTRING::passwordLen;
	*bufferBegin++ = ':';
	std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
	bufferBegin += usernameView.size();

	sessionBegin = bufferBegin;

	std::copy(STATICSTRING::session, STATICSTRING::session + STATICSTRING::sessionLen, bufferBegin);
	bufferBegin += STATICSTRING::sessionLen;
	*bufferBegin++ = ':';
	std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
	bufferBegin += usernameView.size();

	sessionExpireBegin = bufferBegin;

	std::copy(STATICSTRING::sessionExpire, STATICSTRING::sessionExpire + STATICSTRING::sessionExpireLen, bufferBegin);
	bufferBegin += STATICSTRING::sessionExpireLen;
	*bufferBegin++ = ':';
	std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
	bufferBegin += usernameView.size();


	try
	{
		command.emplace_back(std::string_view(STATICSTRING::hmget, STATICSTRING::hmgetLen));
		command.emplace_back(std::string_view(STATICSTRING::user, STATICSTRING::userLen));
		command.emplace_back(std::string_view(passwordBegin, passwordNeedLen));
		command.emplace_back(std::string_view(sessionBegin, sessionNeedLen));
		command.emplace_back(std::string_view(sessionExpireBegin, sessionExpireNeedLen));
		commandSize.emplace_back(5);

		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleTestLoginRedisSlaveEncrypt, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}




void HTTPSERVICE::handleTestLoginRedisSlaveEncrypt(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	if (result)
	{
		if (resultVec.size() != 3)
			return startWrite(HTTPRESPONSEREADY::httpNO_KEY, HTTPRESPONSEREADY::httpNO_KEYLen);;

		//获取对应的密码
		std::string_view &passwordView{ resultVec[0] }, &sessionView{ resultVec[1] }, &sessionExpireView{ resultVec[2] };

		if (passwordView.empty())
		{
			// 分redis中无此用户记录，去分sql进行查询确定到底有没有

			std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

			std::vector<std::string_view> &sqlCommand{ std::get<0>(*sqlRequest).get() };

			std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

			std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

			const char **BodyBuffer{ m_buffer->bodyPara() };

			std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

			sqlCommand.clear();
			rowField.clear();
			result.clear();

			int sqlNeedLen{ SQLCOMMAND::testTestLoginSQLLen + 1 + usernameView.size() };

			char *buffer{ m_charMemoryPool.getMemory(sqlNeedLen) }, *bufferBegin{};

			if (!buffer)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			bufferBegin = buffer;

			std::copy(SQLCOMMAND::testTestLoginSQL, SQLCOMMAND::testTestLoginSQL + SQLCOMMAND::testTestLoginSQLLen, bufferBegin);
			bufferBegin += SQLCOMMAND::testTestLoginSQLLen;

			std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
			bufferBegin += usernameView.size();

			*bufferBegin = '\'';


			try
			{
				sqlCommand.emplace_back(std::string_view(buffer, sqlNeedLen));

				std::get<1>(*sqlRequest) = 1;
				std::get<5>(*sqlRequest) = std::bind(&HTTPSERVICE::handleTestLoginSqlSlaveEncrypt, this, std::placeholders::_1, std::placeholders::_2);

				if (!m_multiSqlReadSWSlave->insertSqlRequest(sqlRequest))
					startWrite(HTTPRESPONSEREADY::httpFailToInsertSql, HTTPRESPONSEREADY::httpFailToInsertSqlLen);
			}
			catch (const std::exception &e)
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			return;
		}



		//比对密码
		const char **BodyBuffer{ m_buffer->bodyPara() };

		if (!std::equal(*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password), *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1), passwordView.cbegin(), passwordView.cend()))
			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);


		//密码匹配情况下查询有没有session
		//hmset  user  session:usernameView   session  sessionEX:usernameView   sessionExpire
		if (sessionView.empty() || sessionExpireView.empty())
		{
			//没有的情况下生成一个
			//生成session   随机24字符串
			//取前24位作为session
			std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

			m_sessionClock = std::chrono::system_clock::now();

			m_sessionClock += std::chrono::hours(24 * 31);

			m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

			std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

			std::string_view &user{ command[1] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

			int sessionExpireValueNeedLen{};

			char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

			if (!buffer)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


			std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

			std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
			std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

			writeCommand.clear();
			writeCommandSize.clear();

			try
			{
				writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
				writeCommand.emplace_back(user);
				writeCommand.emplace_back(sessionView);
				writeCommand.emplace_back(std::string_view(m_randomString, 24));
				writeCommand.emplace_back(sessionExpireView);
				writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
				std::get<1>(*redisWrite) = 1;
				writeCommandSize.emplace_back(6);
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);;
			}


			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
			m_sessionLen = 24;


			//返回包含set-cookie的http消息给客户端

			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

			int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + 24 + usernameView.size() + 1 + STATICSTRING::expiresLen + 1 + STATICSTRING::httpSetCookieExpireValueLen + 1 + STATICSTRING::pathLen + 2 };


			char *sendBuffer{};
			unsigned int sendLen{};


			if (st1.make_json<void, CUSTOMTAG, AESECB>(sendBuffer, sendLen,
				[this, usernameView](char *&bufferBegin)
			{
				if (!bufferBegin)
					return;

				std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
				bufferBegin += MAKEJSON::SetCookieLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
				bufferBegin += STATICSTRING::sessionIDLen;

				*bufferBegin++ = '=';

				std::copy(m_SessionID.get(), m_SessionID.get() + m_sessionLen, bufferBegin);
				bufferBegin += m_sessionLen;

				std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
				bufferBegin += usernameView.size();

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::expires, STATICSTRING::expires + STATICSTRING::expiresLen, bufferBegin);
				bufferBegin += STATICSTRING::expiresLen;

				*bufferBegin++ = '=';

				//返回包含set-cookie的http消息给客户端
					//格林威治时间与北京时间相差四小时
					// DAY, DD MMM YYYY HH:MM:SS GMT
				m_sessionGMTClock = m_sessionClock + std::chrono::hours(4);

				m_sessionGMTTime = std::chrono::system_clock::to_time_t(m_sessionGMTClock);

				m_tmGMT = localtime(&m_sessionGMTTime);

				m_tmGMT->tm_year += 1900;

				std::copy(STATICSTRING::tm_wday[m_tmGMT->tm_wday], STATICSTRING::tm_wday[m_tmGMT->tm_wday] + STATICSTRING::tm_wdayLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_wdayLen;

				*bufferBegin++ = ',';
				*bufferBegin++ = ' ';


				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_mday], STATICSTRING::tm_sec[m_tmGMT->tm_mday] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ' ';

				std::copy(STATICSTRING::tm_mon[m_tmGMT->tm_mon], STATICSTRING::tm_mon[m_tmGMT->tm_mon] + STATICSTRING::tm_monLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_monLen;

				*bufferBegin++ = ' ';

				*bufferBegin++ = m_tmGMT->tm_year / 1000 + '0';
				*bufferBegin++ = m_tmGMT->tm_year / 100 % 10 + '0';
				*bufferBegin++ = m_tmGMT->tm_year / 10 % 10 + '0';
				*bufferBegin++ = m_tmGMT->tm_year % 10 + '0';

				*bufferBegin++ = ' ';

				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_hour], STATICSTRING::tm_sec[m_tmGMT->tm_hour] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_min], STATICSTRING::tm_sec[m_tmGMT->tm_min] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_sec], STATICSTRING::tm_sec[m_tmGMT->tm_sec] + STATICSTRING::tm_secLen, bufferBegin);
				bufferBegin += STATICSTRING::tm_secLen;

				*bufferBegin++ = ' ';

				std::copy(STATICSTRING::GMT, STATICSTRING::GMT + STATICSTRING::GMTLen, bufferBegin);
				bufferBegin += STATICSTRING::GMTLen;

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
				bufferBegin += STATICSTRING::pathLen;

				*bufferBegin++ = '=';

				*bufferBegin++ = '/';

			},
				MAKEJSON::SetCookieLen + 1 + setCookValueLen, &aes_encryptkey,
				MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
			{
				startWrite(sendBuffer, sendLen);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			return;
		}


		//比对session过期时间
		int index{ -1 };
		time_t baseNum{ 1 };

		m_sessionTime = 0;

		std::accumulate(std::make_reverse_iterator(sessionExpireView.cend()), std::make_reverse_iterator(sessionExpireView.cbegin()), &m_sessionTime, [&index, &baseNum](auto &sum, const auto ch)
		{
			if (++index)baseNum *= 10;
			*sum += (ch - '0')*baseNum;
			return sum;
		});

		m_thisClock = std::chrono::system_clock::now();

		m_thisTime = std::chrono::system_clock::to_time_t(m_thisClock);

		if (m_thisTime >= m_sessionTime)
		{
			//session过期，需要重新生成
			std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

			m_sessionClock = m_thisClock + std::chrono::hours(24 * 31);

			m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

			std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

			std::string_view &user{ command[1] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

			int sessionExpireValueNeedLen{};

			char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

			if (!buffer)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


			std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

			std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
			std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

			writeCommand.clear();
			writeCommandSize.clear();

			try
			{
				writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
				writeCommand.emplace_back(user);
				writeCommand.emplace_back(sessionView);
				writeCommand.emplace_back(std::string_view(m_randomString, 24));
				writeCommand.emplace_back(sessionExpireView);
				writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
				std::get<1>(*redisWrite) = 1;
				writeCommandSize.emplace_back(6);
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);;
			}


			//  更新redis中的session  和  session过期时间
			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
			m_sessionLen = 24;

		}
		else
		{
			//没有过期的话返回

			try
			{
				if (sessionView.size() > m_sessionMemory)
				{
					m_SessionID.reset(new char[sessionView.size()]);
					m_sessionMemory = sessionView.size();
				}
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			std::copy(sessionView.cbegin(), sessionView.cend(), m_SessionID.get());
			m_sessionLen = sessionView.size();
		}




		//返回包含set-cookie的http消息给客户端

		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		if (!st1.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

		int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + 24 + usernameView.size() + 1 + STATICSTRING::expiresLen + 1 + STATICSTRING::httpSetCookieExpireValueLen + 1 + STATICSTRING::pathLen + 2 };


		char *sendBuffer{};
		unsigned int sendLen{};


		if (st1.make_json<void, CUSTOMTAG, AESECB>(sendBuffer, sendLen,
			[this, usernameView](char *&bufferBegin)
		{
			if (!bufferBegin)
				return;

			std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
			bufferBegin += MAKEJSON::SetCookieLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
			bufferBegin += STATICSTRING::sessionIDLen;

			*bufferBegin++ = '=';

			std::copy(m_SessionID.get(), m_SessionID.get() + m_sessionLen, bufferBegin);
			bufferBegin += m_sessionLen;

			std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
			bufferBegin += usernameView.size();

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::expires, STATICSTRING::expires + STATICSTRING::expiresLen, bufferBegin);
			bufferBegin += STATICSTRING::expiresLen;

			*bufferBegin++ = '=';

			//返回包含set-cookie的http消息给客户端
			//格林威治时间与北京时间相差四小时
			// DAY, DD MMM YYYY HH:MM:SS GMT
			m_sessionGMTTime = m_sessionTime + STATICSTRING::fourHourSec;

			m_tmGMT = localtime(&m_sessionGMTTime);

			m_tmGMT->tm_year += 1900;

			std::copy(STATICSTRING::tm_wday[m_tmGMT->tm_wday], STATICSTRING::tm_wday[m_tmGMT->tm_wday] + STATICSTRING::tm_wdayLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_wdayLen;

			*bufferBegin++ = ',';
			*bufferBegin++ = ' ';


			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_mday], STATICSTRING::tm_sec[m_tmGMT->tm_mday] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_mon[m_tmGMT->tm_mon], STATICSTRING::tm_mon[m_tmGMT->tm_mon] + STATICSTRING::tm_monLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_monLen;

			*bufferBegin++ = ' ';

			*bufferBegin++ = m_tmGMT->tm_year / 1000 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 100 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 10 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year % 10 + '0';

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_hour], STATICSTRING::tm_sec[m_tmGMT->tm_hour] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_min], STATICSTRING::tm_sec[m_tmGMT->tm_min] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_sec], STATICSTRING::tm_sec[m_tmGMT->tm_sec] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::GMT, STATICSTRING::GMT + STATICSTRING::GMTLen, bufferBegin);
			bufferBegin += STATICSTRING::GMTLen;

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
			bufferBegin += STATICSTRING::pathLen;

			*bufferBegin++ = '=';

			*bufferBegin++ = '/';

		},
			MAKEJSON::SetCookieLen + 1 + setCookValueLen, &aes_encryptkey,
			MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
			MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
			MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
			MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
		{
			startWrite(sendBuffer, sendLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}


	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson<void, void, AESECB>(st1,nullptr,0,&aes_encryptkey);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}
}




void HTTPSERVICE::handleTestLoginSqlSlaveEncrypt(bool result, ERRORMESSAGE em)
{
	if (result)
	{
		std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

		unsigned int commandSize{ std::get<1>(*sqlRequest) };

		std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

		std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

		if (rowField.empty() || result.empty() || (rowField.size() % 2))   //  无此记录
			return startWrite(HTTPRESPONSEREADY::httpNoRecordInSql, HTTPRESPONSEREADY::httpNoRecordInSqlLen);

		const char **BodyBuffer{ m_buffer->bodyPara() };

		std::string_view CheckpasswordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) },
			&RealpasswordView{ result[0] };

		if (!std::equal(CheckpasswordView.cbegin(), CheckpasswordView.cend(), RealpasswordView.cbegin(), RealpasswordView.cend()))
		{
			do
			{
				//取前24位作为session
				std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

				m_sessionClock = m_thisClock + std::chrono::hours(24 * 31);

				m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

				std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

				std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

				std::string_view &user{ command[1] }, &passwordView{ command[2] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

				int sessionExpireValueNeedLen{};

				char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

				if (!buffer)
					break;


				std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

				std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
				std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

				writeCommand.clear();
				writeCommandSize.clear();

				try
				{
					writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
					writeCommand.emplace_back(user);
					writeCommand.emplace_back(passwordView);
					writeCommand.emplace_back(RealpasswordView);
					writeCommand.emplace_back(sessionView);
					writeCommand.emplace_back(std::string_view(m_randomString, 24));
					writeCommand.emplace_back(sessionExpireView);
					writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
					std::get<1>(*redisWrite) = 1;
					writeCommandSize.emplace_back(8);
				}
				catch (const std::exception &e)
				{
					break;
				}


				//  更新redis中的session  和  session过期时间
				if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
					break;

			} while (false);


			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);
		}


		//取前24位作为session
		std::shuffle(m_randomString, m_randomString + randomStringLen, rdEngine);

		m_sessionClock = m_thisClock + std::chrono::hours(24 * 31);

		m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

		std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

		std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };

		std::string_view &user{ command[1] }, &passwordView{ command[2] }, &sessionView{ command[3] }, &sessionExpireView{ command[4] };

		int sessionExpireValueNeedLen{};

		char *buffer{ NumToString(m_sessionTime,sessionExpireValueNeedLen,m_charMemoryPool) }, *bufferBegin{};

		if (!buffer)
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


		std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

		std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
		std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

		writeCommand.clear();
		writeCommandSize.clear();

		try
		{
			writeCommand.emplace_back(std::string_view(STATICSTRING::hmset, STATICSTRING::hmsetLen));
			writeCommand.emplace_back(user);
			writeCommand.emplace_back(passwordView);
			writeCommand.emplace_back(RealpasswordView);
			writeCommand.emplace_back(sessionView);
			writeCommand.emplace_back(std::string_view(m_randomString, 24));
			writeCommand.emplace_back(sessionExpireView);
			writeCommand.emplace_back(std::string_view(buffer, sessionExpireValueNeedLen));
			std::get<1>(*redisWrite) = 1;
			writeCommandSize.emplace_back(8);
		}
		catch (const std::exception &e)
		{
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}


		//  更新redis中的session  和  session过期时间
		if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
			return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
		m_sessionLen = 24;


		//返回包含set-cookie的http消息给客户端

		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		if (!st1.clear())
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success, STATICSTRING::success + STATICSTRING::successLen))
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

		int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + 24 + usernameView.size() + 1 + STATICSTRING::expiresLen + 1 + STATICSTRING::httpSetCookieExpireValueLen + 1 + STATICSTRING::pathLen + 2 };


		char *sendBuffer{};
		unsigned int sendLen{};



		if (st1.make_json<void, CUSTOMTAG, AESECB>(sendBuffer, sendLen,
			[this, usernameView](char *&bufferBegin)
		{
			if (!bufferBegin)
				return;

			std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
			bufferBegin += MAKEJSON::SetCookieLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
			bufferBegin += STATICSTRING::sessionIDLen;

			*bufferBegin++ = '=';

			std::copy(m_SessionID.get(), m_SessionID.get() + m_sessionLen, bufferBegin);
			bufferBegin += m_sessionLen;

			std::copy(usernameView.cbegin(), usernameView.cend(), bufferBegin);
			bufferBegin += usernameView.size();

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::expires, STATICSTRING::expires + STATICSTRING::expiresLen, bufferBegin);
			bufferBegin += STATICSTRING::expiresLen;

			*bufferBegin++ = '=';

			//返回包含set-cookie的http消息给客户端
				//格林威治时间与北京时间相差四小时
				// DAY, DD MMM YYYY HH:MM:SS GMT
			m_sessionGMTTime = m_sessionTime + STATICSTRING::fourHourSec;

			m_tmGMT = localtime(&m_sessionGMTTime);

			m_tmGMT->tm_year += 1900;

			std::copy(STATICSTRING::tm_wday[m_tmGMT->tm_wday], STATICSTRING::tm_wday[m_tmGMT->tm_wday] + STATICSTRING::tm_wdayLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_wdayLen;

			*bufferBegin++ = ',';
			*bufferBegin++ = ' ';


			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_mday], STATICSTRING::tm_sec[m_tmGMT->tm_mday] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_mon[m_tmGMT->tm_mon], STATICSTRING::tm_mon[m_tmGMT->tm_mon] + STATICSTRING::tm_monLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_monLen;

			*bufferBegin++ = ' ';

			*bufferBegin++ = m_tmGMT->tm_year / 1000 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 100 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year / 10 % 10 + '0';
			*bufferBegin++ = m_tmGMT->tm_year % 10 + '0';

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_hour], STATICSTRING::tm_sec[m_tmGMT->tm_hour] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_min], STATICSTRING::tm_sec[m_tmGMT->tm_min] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ':';

			std::copy(STATICSTRING::tm_sec[m_tmGMT->tm_sec], STATICSTRING::tm_sec[m_tmGMT->tm_sec] + STATICSTRING::tm_secLen, bufferBegin);
			bufferBegin += STATICSTRING::tm_secLen;

			*bufferBegin++ = ' ';

			std::copy(STATICSTRING::GMT, STATICSTRING::GMT + STATICSTRING::GMTLen, bufferBegin);
			bufferBegin += STATICSTRING::GMTLen;

			*bufferBegin++ = ';';

			std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
			bufferBegin += STATICSTRING::pathLen;

			*bufferBegin++ = '=';

			*bufferBegin++ = '/';

		},
			MAKEJSON::SetCookieLen + 1 + setCookValueLen, &aes_encryptkey,
			MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
			MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
			MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
			MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
		{
			startWrite(sendBuffer, sendLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		}

	}
	else
	{
		handleERRORMESSAGE(em);
	}
}









void HTTPSERVICE::testBusiness()
{
	startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
}



void HTTPSERVICE::readyParseMultiPartFormData()
{
	m_boundaryLen = m_boundaryEnd - m_boundaryBegin + 4;
	char *buffer{ m_charMemoryPool.getMemory(m_boundaryLen) }, *iter{};

	if (!buffer)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	iter = buffer;
	*iter++ = '\r';
	*iter++ = '\n';
	*iter++ = '-';
	*iter++ = '-';

	std::copy(m_boundaryBegin, m_boundaryEnd, iter);

	m_boundaryBegin = buffer + 2, m_boundaryEnd = buffer + m_boundaryLen;

	m_boundaryLen -= 2;

	m_parseStatus = PARSERESULT::begin_checkBoundaryBegin;

	parseReadData(*(m_verifyData.get() + VerifyDataPos::customPos1Begin), *(m_verifyData.get() + VerifyDataPos::customPos1End) - *(m_verifyData.get() + VerifyDataPos::customPos1Begin));
}




void HTTPSERVICE::testMultiPartFormData()
{
	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success_upload, STATICSTRING::success_upload + STATICSTRING::success_uploadLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1, nullptr, 0, nullptr);
}



void HTTPSERVICE::testSuccessUpload()
{
	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success_upload, STATICSTRING::success_upload + STATICSTRING::success_uploadLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1, nullptr, 0, nullptr);
}



void HTTPSERVICE::readyParseChunkData()
{
	m_parseStatus = PARSERESULT::begin_checkChunkData;

	parseReadData(m_readBuffer, m_bodyEnd - m_bodyBegin);
}



void HTTPSERVICE::resetVerifyData()
{
	std::fill(m_verifyData.get(), m_verifyData.get() + VerifyDataPos::maxBufferSize, nullptr);
}



bool HTTPSERVICE::verifyAES()
{
	return m_clientAESLen;
}







void HTTPSERVICE::verifySessionBackWard(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	if (result)
	{
		if (resultVec.size() != 2)
			return startWrite(HTTPRESPONSEREADY::httpNO_KEY, HTTPRESPONSEREADY::httpNO_KEYLen);;

		std::string_view &sessionView{ resultVec[0] }, &sessionExpireView{ resultVec[1] };

		std::string_view httpAllSessionView{ *(m_verifyData.get()+ VerifyDataPos::httpAllSessionBegin) ,*(m_verifyData.get() + VerifyDataPos::httpAllSessionEnd) - *(m_verifyData.get() + VerifyDataPos::httpAllSessionBegin) }, 
			httpSessionView{ *(m_verifyData.get() + VerifyDataPos::httpSessionBegin) ,*(m_verifyData.get() + VerifyDataPos::httpSessionEnd) - *(m_verifyData.get() + VerifyDataPos::httpSessionBegin) },
			httpSessionUserNameView{ *(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) ,*(m_verifyData.get() + VerifyDataPos::httpSessionUserNameEnd) - *(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) };


		//如果为空则表明还没设置session
		if (sessionView.empty() || sessionExpireView.empty())
		{
			m_thisTime = 0, m_sessionLen = 0;

			///////////////////////////////////////
			//  向客户端发出清除cookie消息，让客户端重新进行登陆操作

			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


			char *sendBuffer{};
			unsigned int sendLen{};

			int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + httpAllSessionView.size() + 1 + STATICSTRING::Max_AgeLen + 3 + STATICSTRING::pathLen + 2 };

			if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
				[this, httpAllSessionView](char *&bufferBegin)
			{
				if (!bufferBegin)
					return;

				std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
				bufferBegin += MAKEJSON::SetCookieLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
				bufferBegin += STATICSTRING::sessionIDLen;

				*bufferBegin++ = '=';

				std::copy(httpAllSessionView.cbegin(), httpAllSessionView.cend(), bufferBegin);
				bufferBegin += httpAllSessionView.size();

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::Max_Age, STATICSTRING::Max_Age + STATICSTRING::Max_AgeLen, bufferBegin);
				bufferBegin += STATICSTRING::Max_AgeLen;

				*bufferBegin++ = '=';

				*bufferBegin++ = '0';

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
				bufferBegin += STATICSTRING::pathLen;

				*bufferBegin++ = '=';

				*bufferBegin++ = '/';
			},
				MAKEJSON::SetCookieLen + 1 + setCookValueLen, nullptr,
				MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
			{
				startWrite(sendBuffer, sendLen);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			return;
		}


		//  不为空则进行比对
		//  先判断是否超时，因为一旦超时再进行其他比较已经毫无意义


		//比对session过期时间
		int index{ -1 };
		time_t baseNum{ 1 };

		m_sessionTime = 0;

		std::accumulate(std::make_reverse_iterator(sessionExpireView.cend()), std::make_reverse_iterator(sessionExpireView.cbegin()), &m_sessionTime, [&index, &baseNum](auto &sum, const auto ch)
		{
			if (++index)baseNum *= 10;
			*sum += (ch - '0')*baseNum;
			return sum;
		});

		m_thisClock = std::chrono::system_clock::now();

		m_thisTime = std::chrono::system_clock::to_time_t(m_thisClock);

		if (m_thisTime >= m_sessionTime)
		{
			m_sessionTime = 0;
			// 清除redis中session缓存，发送给客户端清除session标志
			std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

			std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
			std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

			writeCommand.clear();
			writeCommandSize.clear();

			char *sessionBuf{ m_charMemoryPool.getMemory(STATICSTRING::sessionLen + 2 + 2 * httpSessionUserNameView.size() + STATICSTRING::sessionExpireLen) };

			if (!sessionBuf)
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			char *bufBegin{ sessionBuf };
			std::copy(STATICSTRING::session, STATICSTRING::session + STATICSTRING::sessionLen, bufBegin);
			bufBegin += STATICSTRING::sessionLen;

			*bufBegin++ = ':';
			std::copy(httpSessionUserNameView.cbegin(), httpSessionUserNameView.cend(), bufBegin);
			bufBegin += httpSessionUserNameView.size();

			char *sessionExBegin{ bufBegin };

			std::copy(STATICSTRING::sessionExpire, STATICSTRING::sessionExpire + STATICSTRING::sessionExpireLen, sessionExBegin);
			sessionExBegin += STATICSTRING::sessionExpireLen;
			*sessionExBegin++ = ':';
			std::copy(httpSessionUserNameView.cbegin(), httpSessionUserNameView.cend(), sessionExBegin);
			sessionExBegin += httpSessionUserNameView.size();


			try
			{
				writeCommand.emplace_back(std::string_view(STATICSTRING::hdel, STATICSTRING::hdelLen));
				writeCommand.emplace_back(std::string_view(STATICSTRING::user, STATICSTRING::userLen));
				writeCommand.emplace_back(std::string_view(sessionBuf, bufBegin - sessionBuf));
				writeCommand.emplace_back(std::string_view(bufBegin, sessionExBegin - bufBegin));
				std::get<1>(*redisWrite) = 1;
				writeCommandSize.emplace_back(4);
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}


			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			// 发送给客户端清除session标志

			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			char *sendBuffer{};
			unsigned int sendLen{};

			int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + httpAllSessionView.size() + 1 + STATICSTRING::Max_AgeLen + 3 + STATICSTRING::pathLen + 2 };

			if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
				[this, httpAllSessionView](char *&bufferBegin)
			{
				if (!bufferBegin)
					return;

				std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
				bufferBegin += MAKEJSON::SetCookieLen;

				*bufferBegin++ = ':';

				std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
				bufferBegin += STATICSTRING::sessionIDLen;

				*bufferBegin++ = '=';

				std::copy(httpAllSessionView.cbegin(), httpAllSessionView.cend(), bufferBegin);
				bufferBegin += httpAllSessionView.size();

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::Max_Age, STATICSTRING::Max_Age + STATICSTRING::Max_AgeLen, bufferBegin);
				bufferBegin += STATICSTRING::Max_AgeLen;

				*bufferBegin++ = '=';

				*bufferBegin++ = '0';

				*bufferBegin++ = ';';

				std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
				bufferBegin += STATICSTRING::pathLen;

				*bufferBegin++ = '=';

				*bufferBegin++ = '/';
			},
				MAKEJSON::SetCookieLen + 1 + setCookValueLen, nullptr,
				MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
			{
				startWrite(sendBuffer, sendLen);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}

			return;
		}

		//
		// 比对session是否一致

		if (sessionView.size() > m_sessionMemory)
		{
			try
			{
				m_SessionID.reset(new char[sessionView.size()]);
				m_sessionMemory = sessionView.size();
			}
			catch (const std::exception &e)
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}

		std::copy(sessionView.cbegin(), sessionView.cend(), m_SessionID.get());
		m_sessionLen = sessionView.size();


		if (!std::equal(sessionView.cbegin(), sessionView.cend(), httpSessionView.cbegin(), httpSessionView.cend()))
			return startWrite(HTTPRESPONSEREADY::httpFailToVerify, HTTPRESPONSEREADY::httpFailToVerifyLen);


		m_business();

	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!resultVec.empty())
			{
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson<void, void, AESECB>(st1, nullptr, 0, &aes_encryptkey);
			}
			else
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
			handleERRORMESSAGE(em);
	}
}



void HTTPSERVICE::verifySessionBackWardHttp(bool result, ERRORMESSAGE em)
{
	bool success = false;

	do
	{
		std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
		std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
		std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


		if (result)
		{
			if (resultVec.size() != 2)
			{
				m_sendBuffer = HTTPRESPONSEREADY::httpNO_KEY, m_sendLen = HTTPRESPONSEREADY::httpNO_KEYLen;
				break;
			}

			std::string_view &sessionView{ resultVec[0] }, &sessionExpireView{ resultVec[1] };

			std::string_view httpAllSessionView{ *(m_verifyData.get() + VerifyDataPos::httpAllSessionBegin) ,*(m_verifyData.get() + VerifyDataPos::httpAllSessionEnd) - *(m_verifyData.get() + VerifyDataPos::httpAllSessionBegin) },
				httpSessionView{ *(m_verifyData.get() + VerifyDataPos::httpSessionBegin) ,*(m_verifyData.get() + VerifyDataPos::httpSessionEnd) - *(m_verifyData.get() + VerifyDataPos::httpSessionBegin) },
				httpSessionUserNameView{ *(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) ,*(m_verifyData.get() + VerifyDataPos::httpSessionUserNameEnd) - *(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) };


			//如果为空则表明还没设置session
			if (sessionView.empty() || sessionExpireView.empty())
			{
				m_thisTime = 0, m_sessionLen = 0;

				///////////////////////////////////////
				//  向客户端发出清除cookie消息，让客户端重新进行登陆操作

				STLtreeFast &st1{ m_STLtreeFastVec[0] };

				if (!st1.clear())
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
					break;
				}


				char *sendBuffer{};
				unsigned int sendLen{};

				int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + httpAllSessionView.size() + 1 + STATICSTRING::Max_AgeLen + 3 + STATICSTRING::pathLen + 2 };

				if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
					[this, httpAllSessionView](char *&bufferBegin)
				{
					if (!bufferBegin)
						return;

					std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
					bufferBegin += MAKEJSON::SetCookieLen;

					*bufferBegin++ = ':';

					std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
					bufferBegin += STATICSTRING::sessionIDLen;

					*bufferBegin++ = '=';

					std::copy(httpAllSessionView.cbegin(), httpAllSessionView.cend(), bufferBegin);
					bufferBegin += httpAllSessionView.size();

					*bufferBegin++ = ';';

					std::copy(STATICSTRING::Max_Age, STATICSTRING::Max_Age + STATICSTRING::Max_AgeLen, bufferBegin);
					bufferBegin += STATICSTRING::Max_AgeLen;

					*bufferBegin++ = '=';

					*bufferBegin++ = '0';

					*bufferBegin++ = ';';

					std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
					bufferBegin += STATICSTRING::pathLen;

					*bufferBegin++ = '=';

					*bufferBegin++ = '/';
				},
					MAKEJSON::SetCookieLen + 1 + setCookValueLen, nullptr,
					MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
					MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
					MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
					MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
				{
					m_sendBuffer = sendBuffer, m_sendLen = sendLen;
				}
				else
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
				}

				break;
			}


			//  不为空则进行比对
			//  先判断是否超时，因为一旦超时再进行其他比较已经毫无意义


			//比对session过期时间
			int index{ -1 };
			time_t baseNum{ 1 };

			m_sessionTime = 0;

			std::accumulate(std::make_reverse_iterator(sessionExpireView.cend()), std::make_reverse_iterator(sessionExpireView.cbegin()), &m_sessionTime, [&index, &baseNum](auto &sum, const auto ch)
			{
				if (++index)baseNum *= 10;
				*sum += (ch - '0')*baseNum;
				return sum;
			});

			m_thisClock = std::chrono::system_clock::now();

			m_thisTime = std::chrono::system_clock::to_time_t(m_thisClock);

			if (m_thisTime >= m_sessionTime)
			{
				m_sessionTime = 0;
				// 清除redis中session缓存，发送给客户端清除session标志
				std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

				std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
				std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

				writeCommand.clear();
				writeCommandSize.clear();

				char *sessionBuf{ m_charMemoryPool.getMemory(STATICSTRING::sessionLen + 2 + 2 * httpSessionUserNameView.size() + STATICSTRING::sessionExpireLen) };

				if (!sessionBuf)
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
					break;
				}

				char *bufBegin{ sessionBuf };
				std::copy(STATICSTRING::session, STATICSTRING::session + STATICSTRING::sessionLen, bufBegin);
				bufBegin += STATICSTRING::sessionLen;

				*bufBegin++ = ':';
				std::copy(httpSessionUserNameView.cbegin(), httpSessionUserNameView.cend(), bufBegin);
				bufBegin += httpSessionUserNameView.size();

				char *sessionExBegin{ bufBegin };

				std::copy(STATICSTRING::sessionExpire, STATICSTRING::sessionExpire + STATICSTRING::sessionExpireLen, sessionExBegin);
				sessionExBegin += STATICSTRING::sessionExpireLen;
				*sessionExBegin++ = ':';
				std::copy(httpSessionUserNameView.cbegin(), httpSessionUserNameView.cend(), sessionExBegin);
				sessionExBegin += httpSessionUserNameView.size();


				try
				{
					writeCommand.emplace_back(std::string_view(STATICSTRING::hdel, STATICSTRING::hdelLen));
					writeCommand.emplace_back(std::string_view(STATICSTRING::user, STATICSTRING::userLen));
					writeCommand.emplace_back(std::string_view(sessionBuf, bufBegin - sessionBuf));
					writeCommand.emplace_back(std::string_view(bufBegin, sessionExBegin - bufBegin));
					std::get<1>(*redisWrite) = 1;
					writeCommandSize.emplace_back(4);
				}
				catch (const std::exception &e)
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
					break;
				}


				if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpFailToInsertRedis, m_sendLen = HTTPRESPONSEREADY::httpFailToInsertRedisLen;
					break;
				}


				// 发送给客户端清除session标志

				STLtreeFast &st1{ m_STLtreeFastVec[0] };

				if (!st1.clear())
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
					break;
				}

				char *sendBuffer{};
				unsigned int sendLen{};

				int setCookValueLen{ STATICSTRING::sessionIDLen + 1 + httpAllSessionView.size() + 1 + STATICSTRING::Max_AgeLen + 3 + STATICSTRING::pathLen + 2 };

				if (st1.make_json<void, CUSTOMTAG>(sendBuffer, sendLen,
					[this, httpAllSessionView](char *&bufferBegin)
				{
					if (!bufferBegin)
						return;

					std::copy(MAKEJSON::SetCookie, MAKEJSON::SetCookie + MAKEJSON::SetCookieLen, bufferBegin);
					bufferBegin += MAKEJSON::SetCookieLen;

					*bufferBegin++ = ':';

					std::copy(STATICSTRING::sessionID, STATICSTRING::sessionID + STATICSTRING::sessionIDLen, bufferBegin);
					bufferBegin += STATICSTRING::sessionIDLen;

					*bufferBegin++ = '=';

					std::copy(httpAllSessionView.cbegin(), httpAllSessionView.cend(), bufferBegin);
					bufferBegin += httpAllSessionView.size();

					*bufferBegin++ = ';';

					std::copy(STATICSTRING::Max_Age, STATICSTRING::Max_Age + STATICSTRING::Max_AgeLen, bufferBegin);
					bufferBegin += STATICSTRING::Max_AgeLen;

					*bufferBegin++ = '=';

					*bufferBegin++ = '0';

					*bufferBegin++ = ';';

					std::copy(STATICSTRING::path, STATICSTRING::path + STATICSTRING::pathLen, bufferBegin);
					bufferBegin += STATICSTRING::pathLen;

					*bufferBegin++ = '=';

					*bufferBegin++ = '/';
				},
					MAKEJSON::SetCookieLen + 1 + setCookValueLen, nullptr,
					MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
					MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
					MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
					MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
				{
					m_sendBuffer = sendBuffer, m_sendLen = sendLen;
				}
				else
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
				}

				break;
			}

			//
			// 比对session是否一致

			if (sessionView.size() > m_sessionMemory)
			{
				try
				{
					m_SessionID.reset(new char[sessionView.size()]);
					m_sessionMemory = sessionView.size();
				}
				catch (const std::exception &e)
				{
					m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
					break;
				}
			}

			std::copy(sessionView.cbegin(), sessionView.cend(), m_SessionID.get());
			m_sessionLen = sessionView.size();


			if (!std::equal(sessionView.cbegin(), sessionView.cend(), httpSessionView.cbegin(), httpSessionView.cend()))
			{
				m_sendBuffer = HTTPRESPONSEREADY::httpFailToVerify, m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
				break;
			}


			m_business();

		}
		else
		{
		m_sendBuffer = HTTPRESPONSEREADY::httpFailToVerify, m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
		break;
		}

		success = true;
	}while (false);
	if (success)
	{
		m_business();
	}
	else
	{
		m_startPos = 0;
		hasChunk = false;
		m_readBuffer = m_buffer->getBuffer();
		m_maxReadLen = m_defaultReadLen;
		m_dataBufferVec.clear();
		m_availableLen = m_buffer->getSock()->available();
		cleanData();
	}
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


void HTTPSERVICE::handleERRORMESSAGE(ERRORMESSAGE em)
{
	switch (em)
	{
	case ERRORMESSAGE::OK:
		startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
		break;

	case ERRORMESSAGE::SQL_QUERY_ERROR:
		startWrite(HTTPRESPONSEREADY::http10SqlQueryError, HTTPRESPONSEREADY::http10SqlQueryErrorLen);
		break;

	case ERRORMESSAGE::SQL_NET_ASYNC_ERROR:
		startWrite(HTTPRESPONSEREADY::http10SqlNetAsyncError, HTTPRESPONSEREADY::http10SqlNetAsyncErrorLen);
		break;

	case ERRORMESSAGE::SQL_MYSQL_RES_NULL:
		startWrite(HTTPRESPONSEREADY::http10SqlResNull, HTTPRESPONSEREADY::http10SqlResNullLen);
		break;

	case ERRORMESSAGE::SQL_QUERY_ROW_ZERO:
		startWrite(HTTPRESPONSEREADY::http10SqlQueryRowZero, HTTPRESPONSEREADY::http10SqlQueryRowZeroLen);
		break;

	case ERRORMESSAGE::SQL_QUERY_FIELD_ZERO:
		startWrite(HTTPRESPONSEREADY::http10SqlQueryFieldZero, HTTPRESPONSEREADY::http10SqlQueryFieldZeroLen);
		break;

	case ERRORMESSAGE::SQL_RESULT_TOO_LAGGE:
		startWrite(HTTPRESPONSEREADY::http10sqlSizeTooBig, HTTPRESPONSEREADY::http10sqlSizeTooBigLen);
		break;

	case ERRORMESSAGE::REDIS_ASYNC_WRITE_ERROR:
		startWrite(HTTPRESPONSEREADY::httpREDIS_ASYNC_WRITE_ERROR, HTTPRESPONSEREADY::httpREDIS_ASYNC_WRITE_ERRORLen);
		break;

	case ERRORMESSAGE::REDIS_ASYNC_READ_ERROR:
		startWrite(HTTPRESPONSEREADY::httpREDIS_ASYNC_READ_ERROR, HTTPRESPONSEREADY::httpREDIS_ASYNC_READ_ERRORLen);
		break;

	case ERRORMESSAGE::CHECK_REDIS_MESSAGE_ERROR:
		startWrite(HTTPRESPONSEREADY::httpCHECK_REDIS_MESSAGE_ERROR, HTTPRESPONSEREADY::httpCHECK_REDIS_MESSAGE_ERRORLen);
		break;


	case ERRORMESSAGE::REDIS_READY_QUERY_ERROR:
		startWrite(HTTPRESPONSEREADY::httpREDIS_READY_QUERY_ERROR, HTTPRESPONSEREADY::httpREDIS_READY_QUERY_ERRORLen);
		break;

	case ERRORMESSAGE::NO_KEY:
		startWrite(HTTPRESPONSEREADY::httpNO_KEY, HTTPRESPONSEREADY::httpNO_KEYLen);
		break;


	case ERRORMESSAGE::STD_BADALLOC:
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
		break;


	default:
		startWrite(HTTPRESPONSEREADY::httpUnknownError, HTTPRESPONSEREADY::httpUnknownErrorLen);
		break;

	}
}





void HTTPSERVICE::clean()
{
	//cout << "start clean\n";
	m_lock.lock();
	m_hasClean = true;
	m_lock.unlock();

	m_sessionLen = 0;
	m_startPos = 0;
	if (m_rsaClientPublic)
	{
		RSA_free(m_rsaClientPublic);
		m_rsaClientPublic = nullptr;
	}
	m_hasMakeRandom = false;
	m_clientAESLen = 0;
	m_firstTime = 0;

	for (auto &st : m_STLtreeFastVec)
	{
		st.reset();
	}

	m_buffer->getHasRead() = 0;
	{
		do
		{
			m_buffer->getErrCode() = {};
			m_buffer->getSock()->cancel(m_buffer->getErrCode());
			if (m_buffer->getErrCode())
			{
				//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
			}
		} while (m_buffer->getErrCode());
		do
		{
			m_buffer->getErrCode() = {};
			m_buffer->getSock()->shutdown(boost::asio::socket_base::shutdown_send, m_buffer->getErrCode());
			if (m_buffer->getErrCode())
			{
				if (m_buffer->getErrCode().message() == "Transport endpoint is not connected")
				{
					break;
				}
				//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
			}
		} while (m_buffer->getErrCode());
		do
		{
			m_buffer->getErrCode() = {};
			m_buffer->getSock()->close(m_buffer->getErrCode());
			if (m_buffer->getErrCode())
			{
				//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
			}
		} while (m_buffer->getErrCode());
	}

	m_charMemoryPool.reset();
	m_charPointerMemoryPool.reset();
	m_sendMemoryPool.reset();
	(*m_clearFunction)(m_mySelf);
}



void HTTPSERVICE::sendOK()
{
	startWrite(HTTPRESPONSEREADY::http10OK, HTTPRESPONSEREADY::http10OKLen);
}



void HTTPSERVICE::cleanData()
{
	if (m_availableLen)
	{
		m_buffer->getSock()->async_read_some(boost::asio::buffer(m_readBuffer, m_availableLen >= m_maxReadLen ? m_maxReadLen: m_availableLen), [this](const boost::system::error_code &err, std::size_t size)
		{
			if (err)
			{
				//m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
			}
			else
			{
				m_availableLen -= size;
				cleanData();
			}
		});
	}
	else
	{
		startWrite(m_sendBuffer, m_sendLen);
	}
}







void HTTPSERVICE::prepare()
{
	m_httpHeaderMap.reset(new const char*[HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN]);
	std::fill(m_httpHeaderMap.get(), m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN, nullptr);

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

	m_BodyBegin= m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Body, m_BodyEnd= m_BodyBegin + 1;

	m_Transfer_EncodingBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::Transfer_Encoding, m_Transfer_EncodingEnd = m_Transfer_EncodingBegin + 1;


	m_Boundary_ContentDispositionBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_ContentDisposition, m_Boundary_ContentDispositionEnd = m_Boundary_ContentDispositionBegin + 1;

	m_Boundary_NameBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_Name, m_Boundary_NameEnd = m_Boundary_NameBegin + 1;

	m_Boundary_FilenameBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_Filename, m_Boundary_FilenameEnd = m_Boundary_FilenameBegin + 1;

	m_Boundary_ContentTypeBegin = m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::boundary_ContentType, m_Boundary_ContentTypeEnd = m_Boundary_ContentTypeBegin + 1;




	int i = -1, j{}, z{};

	m_stringViewVec.resize(15, std::vector<std::string_view>{});
	m_mysqlResVec.resize(3, std::vector<MYSQL_RES*>{});
	m_unsignedIntVec.resize(12, std::vector<unsigned int>{});

	while (++i != 5)
	{
		m_STLtreeFastVec.emplace_back(STLtreeFast(&m_charPointerMemoryPool, &m_sendMemoryPool));
	}
	

	i = -1, j = -1, z = -1;
	while (++i != 3)
	{
		m_multiSqlRequestSWVec.emplace_back(std::make_shared<resultTypeSW>(m_stringViewVec[++j], 0, m_mysqlResVec[i], m_unsignedIntVec[++z], m_stringViewVec[++j], nullptr));
		m_multiRedisRequestSWVec.emplace_back(std::make_shared<redisResultTypeSW>(m_stringViewVec[++j], 0, m_unsignedIntVec[++z], 0, m_stringViewVec[++j], m_unsignedIntVec[++z], nullptr));
		m_multiRedisWriteSWVec.emplace_back(std::make_shared<redisWriteTypeSW>(m_stringViewVec[++j], 0, m_unsignedIntVec[++z]));
	}
}




void HTTPSERVICE::startRead()
{
	m_lock.lock();
	m_time=std::chrono::high_resolution_clock::now();
	m_hasClean = false;
	m_lock.unlock();
	m_buffer->getSock()->async_read_some(boost::asio::buffer(m_readBuffer + m_startPos, m_maxReadLen - m_startPos), [this](const boost::system::error_code &err, std::size_t size)
	{
		if (err)
		{
			if (err != boost::asio::error::operation_aborted)
			{
				m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
			}
		}
		else
		{
			m_lock.lock();
			m_hasClean = true;
			m_lock.unlock();
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



//发生异常时还原置位，开启新的监听

void HTTPSERVICE::parseReadData(const char *source, const int size)
{
	try
	{
		switch ((m_parseStatus = parseHttp(source, size)))
		{
		case PARSERESULT::complete:
			m_startPos = 0;
			m_readBuffer = m_buffer->getBuffer();
			m_maxReadLen = m_defaultReadLen;
			m_dataBufferVec.clear();
			checkMethod();
			break;


		case PARSERESULT::check_messageComplete:
			m_startPos = 0;
			m_dataBufferVec.clear();
			checkMethod();
			break;


		case PARSERESULT::invaild:
			m_startPos = 0;
			hasChunk = false;
			m_readBuffer = m_buffer->getBuffer();
			m_maxReadLen = m_defaultReadLen;
			m_dataBufferVec.clear();
			m_availableLen = m_buffer->getSock()->available();
			m_sendBuffer = HTTPRESPONSEREADY::http10invaild, m_sendLen = HTTPRESPONSEREADY::http10invaildLen;
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
	catch (const std::exception &e)
	{
		m_startPos = 0;
		hasChunk = false;
		m_readBuffer = m_buffer->getBuffer();
		m_maxReadLen = m_defaultReadLen;
		m_dataBufferVec.clear();
		m_availableLen = m_buffer->getSock()->available();
		m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
		cleanData();
	}
}




//常规http解析对于分包处理是将数据append然后处理
//这里为了更快，每次解析不全时记录下当前状态位，下一次收到数据包时可以根据状态位恢复到最近的地方继续解析
//有需要才进行拷贝，没有的话只是记录下位置以备需要，同时兼顾性能与扩展，仅当关键数据段跨越两块内存区时才进行拷贝合成，否则直接用指针指向
//支持POST  GET
// 支持分包解析
// 除了支持常规body  chunk解析和multipartFromdata两种上传格式解析

int HTTPSERVICE::parseHttp(const char * source, const int size)
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
#define MAXHEADLEN 19
#define MAXWORDBEGINLEN 10
#define MAXWORDLEN 256
#define MAXCHUNKNUMBERLEN 6
#define MAXCHUNKNUMENDLEN 10
#define MAXCHUNKFinalEndLEN 4
#define MAXTHIRDLENLEN 2
#define MAXBOUNDARYHEADERNLEN 30

	static std::string HTTPStr{ "HTTP" };
	static std::string HTTP10{ "1.0" }, HTTP11{ "1.1" };
	static std::string lineStr{ "\r\n\r\n" };
	static std::string PUTStr{ "PUT" }, GETStr{ "GET" };                                         // 3                
	static std::string POSTStr{ "POST" }, HEADStr{ "HEAD" };                    // 4                     
	static std::string TRACEStr{ "TRACE" };                                     // 5
	static std::string DELETEStr{ "DELETE " };                                  // 6
	static std::string CONNECTStr{ "CONNECT" }, OPTIONSStr{ "OPTIONS" };        // 7
	//针对每一个关键位置的长度作限制，避免遇到超长无效内容被浪费性能

	/////////////////////////////////////////////////////////////////////////////////////
	static std::string Accept{ "accept" };
	static std::string Accept_Charset{ "accept-charset" };
	static std::string Accept_Encoding{ "accept-encoding" };
	static std::string Accept_Language{ "accept-language" };
	static std::string Accept_Ranges{ "accept-ranges" };
	static std::string Authorization{ "authorization" };
	static std::string Cache_Control{ "cache-control" };
	static std::string Connection{ "connection" };
	static std::string Cookie{ "cookie" };
	static std::string Content_Type{ "content-type" };
	static std::string Date{ "date" };
	static std::string Expect{ "expect" };
	static std::string From{ "from" };
	static std::string Host{ "host" };
	static std::string If_Match{ "if-match" };
	static std::string If_Modified_Since{ "if-modified-since" };
	static std::string If_None_Match{ "if-none-match" };
	static std::string If_Range{ "if-range" };
	static std::string If_Unmodified_Since{ "if-unmodified-since" };
	static std::string Max_Forwards{ "max-forwards" };
	static std::string Pragma{ "Pragma" };
	static std::string Proxy_Authorization{ "proxy-authorization" };
	static std::string Range{ "range" };
	static std::string Referer{ "referer" };
	static std::string Upgrade{ "upgrade" };
	static std::string User_Agent{ "user-agent" };
	static std::string Via{ "via" };
	static std::string Warning{ "warning" };
	static std::string TE{ "te" };
	static std::string ContentLength{ "content-length" };
	static std::string Transfer_Encoding{ "transfer-encoding" };
	static std::string chunked{ "chunked" };
	static std::string multipartform_data{ "multipart/form-data" };
	static std::string boundary{ "boundary=" };

	static std::string Content_Disposition{ "content-disposition" };
	static std::string Name{ "name" };
	static std::string Filename{ "filename" };





	///////////////////////////////////////////////////////////////////////////////////////
	int len{}, num{}, index{};                   //累计body长度
	const char **headerPos{ m_httpHeaderMap.get() };
	char *newBuffer{}, *newBufferBegin{}, *newBufferEnd{}, *newBufferIter{};
	bool isDoubleQuotation{};   //判断boundaryWord首字符是否是\"

	const char *iterFindBegin{ source }, *iterFindEnd{ source + size }, *iterFinalEnd{ m_readBuffer + m_maxReadLen }, *iterFindThisEnd{};

	const char *&funBegin{ m_funBegin }, *&funEnd{ m_funEnd }, *&finalFunBegin{ m_finalFunBegin }, *&finalFunEnd{ m_finalFunEnd },
		*&targetBegin{ m_targetBegin }, *&targetEnd{ m_targetEnd }, *&finalTargetBegin{ m_finalTargetBegin }, *&finalTargetEnd{ m_finalTargetEnd },
		*&paraBegin{ m_paraBegin }, *&paraEnd{ m_paraEnd }, *&finalParaBegin{ m_finalParaBegin }, *&finalParaEnd{ m_finalParaEnd },
		*&bodyBegin{ m_bodyBegin }, *&bodyEnd{ m_bodyEnd }, *&finalBodyBegin{ m_finalBodyBegin }, *&finalBodyEnd{ m_finalBodyEnd },
		*&versionBegin{ m_versionBegin }, *&versionEnd{ m_versionEnd }, *&finalVersionBegin{ m_finalVersionBegin }, *&finalVersionEnd{ m_finalVersionEnd },
		*&headBegin{ m_headBegin }, *&headEnd{ m_headEnd }, *&finalHeadBegin{ m_finalHeadBegin }, *&finalHeadEnd{ m_finalHeadEnd },
		*&wordBegin{ m_wordBegin }, *&wordEnd{ m_wordEnd }, *&finalWordBegin{ m_finalWordBegin }, *&finalWordEnd{ m_finalWordEnd },
		*&httpBegin{ m_httpBegin }, *&httpEnd{ m_httpEnd }, *&finalHttpBegin{ m_finalHttpBegin }, *&finalHttpEnd{ m_finalHttpEnd },
		*&lineBegin{ m_lineBegin }, *&lineEnd{ m_lineEnd }, *&finalLineBegin{ m_finalLineBegin }, *&finalLineEnd{ m_finalLineEnd },
		*&chunkNumBegin{ m_chunkNumBegin }, *&chunkNumEnd{ m_chunkNumEnd }, *&finalChunkNumBegin{ m_finalChunkNumBegin }, *&finalChunkNumEnd{ m_finalChunkNumEnd },
		*&chunkDataBegin{ m_chunkDataBegin }, *&chunkDataEnd{ m_chunkDataEnd }, *&finalChunkDataBegin{ m_finalChunkDataBegin }, *&finalChunkDataEnd{ m_finalChunkDataEnd },
		*&boundaryBegin{ m_boundaryBegin }, *&boundaryEnd{ m_boundaryEnd },
		*&findBoundaryBegin{ m_findBoundaryBegin }, *&findBoundaryEnd{ m_findBoundaryEnd },
		*&boundaryHeaderBegin{ m_boundaryHeaderBegin }, *&boundaryHeaderEnd{ m_boundaryHeaderEnd },
		*&boundaryWordBegin{ m_boundaryWordBegin }, *&boundaryWordEnd{ m_boundaryWordEnd }, *&thisBoundaryWordBegin{ m_thisBoundaryWordBegin }, *&thisBoundaryWordEnd{ m_thisBoundaryWordEnd }
	;



	unsigned int &accumulateLen{ m_accumulateLen };
	std::vector<std::pair<const char*, const char*>>&dataBufferVec{ m_dataBufferVec };
	std::vector<std::pair<const char*, const char*>>::const_iterator dataBufferVecBegin{}, dataBufferVecEnd{};


	switch (m_parseStatus)
	{
	case PARSERESULT::check_messageComplete:
	case PARSERESULT::invaild:
	case PARSERESULT::complete:
		m_buffer->getView().clear();
		hasBody = hasChunk = hasPara = false;
		m_bodyLen = 0;
		m_boundaryHeaderPos = 0;
		m_buffer->getView().method() = 0;
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
		iterFindBegin = bodyBegin;
		goto find_finalBodyEnd;
		break;


	case PARSERESULT::begin_checkBoundaryBegin:
		m_findBoundaryBegin = iterFindBegin;
		goto begin_checkBoundaryBegin;
		break;


	case PARSERESULT::begin_checkChunkData:
		goto begin_checkChunkData;
		break;


	case PARSERESULT::find_fistCharacter:
		goto find_fistCharacter;
		break;


	case PARSERESULT::find_chunkNumEnd:
		chunkNumBegin = chunkNumEnd = iterFindBegin = m_readBuffer;
		goto find_chunkNumEnd;
		break;


	case PARSERESULT::find_thirdLineEnd:
		chunkNumEnd = iterFindBegin = m_readBuffer;
		goto find_thirdLineEnd;
		break;


	case PARSERESULT::find_chunkDataEnd:
		chunkDataBegin = iterFindBegin = m_readBuffer;
		goto find_chunkDataEnd;
		break;


	case PARSERESULT::find_fourthLineBegin:
		chunkDataEnd = iterFindBegin = m_readBuffer;
		goto find_fourthLineBegin;
		break;


	case PARSERESULT::find_fourthLineEnd:
		chunkDataEnd = iterFindBegin = m_readBuffer;
		goto find_fourthLineEnd;
		break;


	case PARSERESULT::find_boundaryBegin:
		m_findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto find_boundaryBegin;
		break;


	case PARSERESULT::find_halfBoundaryBegin:
		m_findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto find_halfBoundaryBegin;
		break;


	case PARSERESULT::find_boundaryHeaderBegin:
		m_findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto find_boundaryHeaderBegin;
		break;


	case PARSERESULT::find_boundaryHeaderBegin2:
		boundaryHeaderBegin = iterFindBegin = m_readBuffer;
		goto find_boundaryHeaderBegin2;
		break;


	case PARSERESULT::find_nextboundaryHeaderBegin:
		boundaryHeaderBegin = boundaryWordEnd = iterFindBegin = m_readBuffer;
		goto find_nextboundaryHeaderBegin;
		break;

	case PARSERESULT::find_nextboundaryHeaderBegin2:
		boundaryWordEnd = iterFindBegin = m_readBuffer;
		goto find_nextboundaryHeaderBegin2;
		break;


	case PARSERESULT::find_nextboundaryHeaderBegin3:
		boundaryHeaderBegin = iterFindBegin = m_readBuffer;
		goto find_nextboundaryHeaderBegin3;
		break;


	case PARSERESULT::find_boundaryHeaderEnd:
		m_findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto find_boundaryHeaderEnd;
		break;


	case PARSERESULT::find_boundaryWordBegin:
		m_boundaryWordBegin = iterFindBegin = m_readBuffer;
		goto find_boundaryWordBegin;
		break;


	case PARSERESULT::find_boundaryWordBegin2:
		boundaryWordBegin = iterFindBegin = m_readBuffer;
		goto find_boundaryWordBegin2;
		break;


	case PARSERESULT::find_boundaryWordEnd:
		m_boundaryWordBegin = m_boundaryWordEnd = iterFindBegin = m_readBuffer;
		goto find_boundaryWordEnd;
		break;


	case PARSERESULT::find_boundaryWordEnd2:
		m_boundaryWordBegin = m_boundaryWordEnd = iterFindBegin = m_readBuffer;
		goto find_boundaryWordEnd2;
		break;


	case PARSERESULT::find_fileBegin:
		boundaryHeaderBegin = iterFindBegin = m_readBuffer;
		goto find_fileBegin;
		break;


	case PARSERESULT::find_fileBoundaryBegin:
		findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto find_fileBoundaryBegin;
		break;


	case PARSERESULT::check_fileBoundaryEnd:
		findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto check_fileBoundaryEnd;
		break;


	case PARSERESULT::check_fileBoundaryEnd2:
		findBoundaryBegin = iterFindBegin = m_readBuffer;
		goto check_fileBoundaryEnd2;
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
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funBegin !isupper");
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
		if (accumulateLen + len >= MAXMETHODLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXMETHODLEN - accumulateLen);

			funEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_not<>(), std::bind(isupper, std::placeholders::_1)));

			if (funEnd == iterFindThisEnd)
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd funEnd == iterFindThisEnd");
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
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
					return PARSERESULT::find_funEnd;
				}
				else
				{
					if (dataBufferVec.empty())
						dataBufferVec.emplace_back(std::make_pair(funBegin, iterFindEnd));
					else
						dataBufferVec.emplace_back(std::make_pair(m_readBuffer, m_readBuffer + m_maxReadLen));


					newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

					if (!newBuffer)
					{
						m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_funEnd;
				}
			}
		}

		if (*funEnd != ' ')
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd *funEnd != ' '");
			return PARSERESULT::invaild;
		}


		if (!dataBufferVec.empty())
		{
			//因为dataBufferVec非空时，在存入的时候已经累加了长度，因此只需要加上std::distance(m_readBuffer, const_cast<char*>(funEnd)) 本次解析长度即可
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(funEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			index = 0;

			newBufferIter = newBuffer;

			for (auto const &singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(funEnd), newBufferIter);

			finalFunBegin = newBuffer, finalFunEnd = newBuffer + accumulateLen;

			dataBufferVec.clear();
		}
		else
		{
			finalFunBegin = funBegin, finalFunEnd = funEnd;
		}


		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


		switch (std::distance(finalFunBegin, finalFunEnd))
		{
		case HTTPHEADER::THREE:
			switch (*finalFunBegin)
			{
			case 'P':
				if (!std::equal(finalFunBegin, finalFunEnd, PUTStr.cbegin(), PUTStr.cend()))
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal PUTStr");
					return PARSERESULT::invaild;
				}
				m_buffer->getView().method() = METHOD::PUT;
				m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::THREE);
				break;
			case 'G':
				if (!std::equal(finalFunBegin, finalFunEnd, GETStr.cbegin(), GETStr.cend()))
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal GETStr");
					return PARSERESULT::invaild;
				}
				m_buffer->getView().method() = METHOD::GET;
				m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::THREE);
				break;
			default:
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd THREE default");
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
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal POSTStr");
					return PARSERESULT::invaild;
				}
				m_buffer->getView().method() = METHOD::POST;
				m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::FOUR);
				break;
			case 'H':
				if (!std::equal(finalFunBegin, finalFunEnd, HEADStr.cbegin(), HEADStr.cend()))
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal HEADStr");
					return PARSERESULT::invaild;
				}
				m_buffer->getView().method() = METHOD::HEAD;
				m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::FOUR);
				break;
			default:
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd FOUR default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		case HTTPHEADER::FIVE:
			if (!std::equal(finalFunBegin, finalFunEnd, TRACEStr.cbegin(), TRACEStr.cend()))
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal TRACEStr");
				return PARSERESULT::invaild;
			}
			m_buffer->getView().method() = METHOD::TRACE;
			m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::FIVE);
			break;
		case HTTPHEADER::SIX:
			if (!std::equal(finalFunBegin, finalFunEnd, DELETEStr.cbegin(), DELETEStr.cend()))
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal DELETEStr");
				return PARSERESULT::invaild;
			}
			m_buffer->getView().method() = METHOD::DELETE;
			m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::SIX);
			break;
		case HTTPHEADER::SEVEN:
			switch (*finalFunBegin)
			{
			case 'C':
				if (!std::equal(finalFunBegin, finalFunEnd, CONNECTStr.cbegin(), CONNECTStr.cend()))
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal CONNECTStr");
					return PARSERESULT::invaild;
				}
				m_buffer->getView().method() = METHOD::CONNECT;
				m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::SEVEN);
				break;
			case 'O':
				if (!std::equal(finalFunBegin, finalFunEnd, OPTIONSStr.cbegin(), OPTIONSStr.cend()))
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal OPTIONSStr");
					return PARSERESULT::invaild;
				}
				m_buffer->getView().method() = METHOD::OPTIONS;
				m_buffer->getView().setMethod(finalFunBegin, HTTPHEADER::SEVEN);
				break;
			default:
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd SEVEN default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		default:
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd default");
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
				newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

				if (!newBuffer)
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_targetBegin;
			}
		}

		if (*iterFindBegin == '\r')
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetBegin *iterFindBegin == '\r'");
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
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd targetEnd == iterFindThisEnd");
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

					newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

					if (!newBuffer)
					{
						m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd !newBuffer");
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
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd *targetEnd == '\r'");
			return PARSERESULT::invaild;
		}

		//如果很长，横跨了不同分段，那么进行整合再处理
		if (!dataBufferVec.empty())
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(targetEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			index = 0;

			newBufferIter = newBuffer;

			for (auto const &singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(targetEnd), newBufferIter);

			finalTargetBegin = newBuffer, finalTargetEnd = newBuffer + accumulateLen;

			dataBufferVec.clear();
		}
		else
		{
			finalTargetBegin = targetBegin, finalTargetEnd = targetEnd;
		}


		m_buffer->getView().setTarget(finalTargetBegin, finalTargetEnd - finalTargetBegin);
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
					newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

					if (!newBuffer)
					{
						m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraBegin !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_targetBegin;
				}
			}

			if (*iterFindBegin == '\r')
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraBegin *iterFindBegin == '\r'");
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
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd paraEnd == iterFindThisEnd");
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


						newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

						if (!newBuffer)
						{
							m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd !newBuffer");
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
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd *paraEnd == '\r'");
				return PARSERESULT::invaild;
			}

			//如果很长，横跨了不同分段，那么进行整合再处理
			if (!dataBufferVec.empty())
			{

				accumulateLen += std::distance(m_readBuffer, const_cast<char*>(paraEnd));

				newBuffer = m_charMemoryPool.getMemory(accumulateLen);

				if (!newBuffer)
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd !newBuffer");
					return PARSERESULT::invaild;
				}

				index = 0;

				newBufferIter = newBuffer;

				for (auto const &singlePair : dataBufferVec)
				{
					std::copy(singlePair.first, singlePair.second, newBufferIter);
					newBufferIter += std::distance(singlePair.first, singlePair.second);
				}
				std::copy(m_readBuffer, const_cast<char*>(paraEnd), newBufferIter);

				finalParaBegin = newBuffer, finalParaEnd = newBuffer + accumulateLen;

				dataBufferVec.clear();
			}
			else
			{
				finalParaBegin = paraBegin, finalParaEnd = paraEnd;
			}


			m_buffer->getView().setPara(finalParaBegin, finalParaEnd - finalParaBegin);
			hasPara = true;
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
				newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

				if (!newBuffer)
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_httpBegin;
			}
		}

		if (*iterFindBegin != 'H')
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpBegin *iterFindBegin != 'H'");
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
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd httpEnd == iterFindThisEnd");
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


					newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

					if (!newBuffer)
					{
						m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd !newBuffer");
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
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd *httpEnd != '/'");
			return PARSERESULT::invaild;
		}

		//如果很长，横跨了不同分段，那么进行整合再处理
		if (!dataBufferVec.empty())
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(httpEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			index = 0;

			newBufferIter = newBuffer;

			for (auto const &singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(httpEnd), newBufferIter);

			finalHttpBegin = newBuffer, finalHttpEnd = newBuffer + accumulateLen;

			dataBufferVec.clear();
		}
		else
		{
			finalHttpBegin = httpBegin, finalHttpEnd = httpEnd;
		}

		if (!std::equal(finalHttpBegin, finalHttpEnd, HTTPStr.cbegin(), HTTPStr.cend()))
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd !equal HTTPStr");
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
				newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

				if (!newBuffer)
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_versionBegin;
			}
		}


		if (*iterFindBegin != '1' && *iterFindBegin != '2')
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionBegin *iterFindBegin != '1' && *iterFindBegin != '2'");
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
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd versionEnd == iterFindThisEnd");
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


					newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

					if (!newBuffer)
					{
						m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd !newBuffer");
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
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd *versionEnd != '\r'");
			return PARSERESULT::invaild;
		}

		//如果很长，横跨了不同分段，那么进行整合再处理
		if (!dataBufferVec.empty())
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(versionEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			index = 0;

			newBufferIter = newBuffer;

			for (auto const &singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(versionEnd), newBufferIter);

			finalVersionBegin = newBuffer, finalVersionEnd = newBuffer + accumulateLen;

			dataBufferVec.clear();
		}
		else
		{
			finalVersionBegin = versionBegin, finalVersionEnd = versionEnd;
		}


		if (!std::equal(finalVersionBegin, finalVersionEnd, HTTP10.cbegin(), HTTP10.cend()) &&
			!std::equal(finalVersionBegin, finalVersionEnd, HTTP11.cbegin(), HTTP11.cend()))
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd !equal HTTP10 HTTP11");
			return PARSERESULT::invaild;
		}


		m_buffer->getView().setVersion(finalVersionBegin, finalVersionEnd - finalVersionBegin);

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
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd lineEnd == iterFindThisEnd");
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
							m_messageBegin = lineEnd;
							goto check_messageComplete;
						}
					}
					else
					{
						if (std::distance(dataBufferVec.front().first, dataBufferVec.front().second) + std::distance(m_readBuffer, const_cast<char*>(lineEnd)) == 4)
						{
							m_messageBegin = lineEnd;
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


					newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

					if (!newBuffer)
					{
						m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_lineEnd;
				}
			}
		}


		if (!dataBufferVec.empty())
		{
			
			accumulateLen+= std::distance(m_readBuffer, const_cast<char*>(lineEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			index = 0;

			newBufferIter = newBuffer;

			for (auto const &singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(lineEnd), newBufferIter);

			finalLineBegin = newBuffer, finalLineEnd = newBuffer + accumulateLen;

			dataBufferVec.clear();
		}
		else
		{
			finalLineBegin = lineBegin, finalLineEnd = lineEnd;
		}


		len = distance(lineBegin, lineEnd);
		if (len != 2 && len != 4)
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd len != 2 && len != 4");
			return PARSERESULT::invaild;
		}

		accumulateLen = 0;

		break;
	}

	if (len == 2)
	{
		if (!std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cbegin() + 2))
		{
			m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd len ==2 !equal lineStr");
			return PARSERESULT::invaild;
		}
		while (std::distance(finalLineBegin, finalLineEnd) != 4)
		{
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
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd headEnd == iterFindThisEnd");
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


						newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

						if (!newBuffer)
						{
							m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd !newBuffer");
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
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd *headEnd == '\r'");
				return PARSERESULT::invaild;
			}

			if (!dataBufferVec.empty())
			{
				
				accumulateLen+= std::distance(m_readBuffer, const_cast<char*>(headEnd));

				newBuffer = m_charMemoryPool.getMemory(accumulateLen);

				if (!newBuffer)
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd !newBuffer");
					return PARSERESULT::invaild;
				}

				index = 0;

				newBufferIter = newBuffer;

				for (auto const &singlePair : dataBufferVec)
				{
					std::copy(singlePair.first, singlePair.second, newBufferIter);
					newBufferIter += std::distance(singlePair.first, singlePair.second);
				}
				std::copy(m_readBuffer, const_cast<char*>(headEnd), newBufferIter);

				finalHeadBegin = newBuffer, finalHeadEnd = newBuffer + accumulateLen;

				dataBufferVec.clear();
			}
			else
			{
				finalHeadBegin = headBegin, finalHeadEnd = headEnd;
			}


			iterFindBegin = headEnd + 1;
			accumulateLen = 0;


		find_wordBegin:
			len = std::distance(iterFindBegin, iterFindEnd);
			if (accumulateLen + len >= MAXWORDBEGINLEN)
			{
				iterFindThisEnd = iterFindBegin + (MAXWORDBEGINLEN - accumulateLen);

				wordBegin = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::not_equal_to<>(), std::placeholders::_1, ' '));

				if (wordBegin == iterFindThisEnd)
				{
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordBegin wordBegin == iterFindThisEnd");
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
						newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

						if (!newBuffer)
						{
							m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordBegin !newBuffer");
							return PARSERESULT::invaild;
						}

						m_readBuffer = newBuffer;
						m_startPos = 0;
						return PARSERESULT::find_wordBegin;
					}
				}
			}


			if (*wordBegin == '\r')
			{
				m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordBegin *wordBegin == '\r'");
				return PARSERESULT::invaild;
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
					m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordEnd wordEnd == iterFindThisEnd");
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


						newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

						if (!newBuffer)
						{
							m_log->writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordEnd !newBuffer");
							return PARSERESULT::invaild;
						}

						m_readBuffer = newBuffer;
						m_startPos = 0;
						return PARSERESULT::find_wordEnd;
					}
				}
			}


			//如果很长，横跨了不同分段，那么进行整合再处理
			if (!dataBufferVec.empty())
			{
				
				accumulateLen+= std::distance(m_readBuffer, const_cast<char*>(wordEnd));

				newBuffer = m_charMemoryPool.getMemory(accumulateLen);

				if (!newBuffer)
					return PARSERESULT::invaild;

				index = 0;

				newBufferIter = newBuffer;

				for (auto const &singlePair : dataBufferVec)
				{
					std::copy(singlePair.first, singlePair.second, newBufferIter);
					newBufferIter += std::distance(singlePair.first, singlePair.second);
				}
				std::copy(m_readBuffer, const_cast<char*>(wordEnd), newBufferIter);

				finalWordBegin = newBuffer, finalWordEnd = newBuffer + accumulateLen;

				dataBufferVec.clear();
			}
			else
			{
				finalWordBegin = wordBegin, finalWordEnd = wordEnd;
			}

			//////////////////////////////////////////////////////////////////


			switch (std::distance(finalHeadBegin, finalHeadEnd))
			{
			case HTTPHEADER::TWO:
				//"TE"
				if (std::equal(finalHeadBegin, finalHeadEnd, TE.cbegin(), TE.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (*m_TEBegin)
						return PARSERESULT::invaild;
					*m_TEBegin = finalWordBegin;
					*m_TEEnd = finalWordEnd;
				}

				break;

			case HTTPHEADER::THREE:
				//"Via"
				if (std::equal(finalHeadBegin, finalHeadEnd, Via.cbegin(), Via.cend(),
					std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
				{
					if (*m_ViaBegin)
						return PARSERESULT::invaild;
					*m_ViaBegin = finalWordBegin;
					*m_ViaEnd = finalWordEnd;
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
						if (*m_DateBegin)
							return PARSERESULT::invaild;
						*m_DateBegin = finalWordBegin;
						*m_DateEnd = finalWordEnd;
					}

					break;

				case 'f':
				case 'F':
					//"From"
					if (std::equal(finalHeadBegin, finalHeadEnd, From.cbegin(), From.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_FromBegin)
							return PARSERESULT::invaild;
						*m_FromBegin = finalWordBegin;
						*m_FromEnd = finalWordEnd;
					}

					break;

				case 'h':
				case 'H':
					//"Host"
					if (std::equal(finalHeadBegin, finalHeadEnd, Host.cbegin(), Host.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_HostBegin)
							return PARSERESULT::invaild;
						*m_HostBegin = finalWordBegin;
						*m_HostEnd = finalWordEnd;
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
					if (*m_RangeBegin)
						return PARSERESULT::invaild;
					*m_RangeBegin = finalWordBegin;
					*m_RangeEnd = finalWordEnd;
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
						if (*m_AcceptBegin)
							return PARSERESULT::invaild;
						*m_AcceptBegin = finalWordBegin;
						*m_AcceptEnd = finalWordEnd;
					}

					break;

				case 'c':
				case 'C':
					//"Cookie"
					if (std::equal(finalHeadBegin, finalHeadEnd, Cookie.cbegin(), Cookie.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_CookieBegin)
							return PARSERESULT::invaild;
						*m_CookieBegin = finalWordBegin;
						*m_CookieEnd = finalWordEnd;
					}

					break;

				case 'e':
				case 'E':
					//"Expect"
					if (std::equal(finalHeadBegin, finalHeadEnd, Expect.cbegin(), Expect.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_ExpectBegin)
							return PARSERESULT::invaild;
						*m_ExpectBegin = finalWordBegin;
						*m_ExpectEnd = finalWordEnd;
					}

					break;

				case 'p':
				case 'P':
					//"Pragma"
					if (std::equal(finalHeadBegin, finalHeadEnd, Pragma.cbegin(), Pragma.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_PragmaBegin)
							return PARSERESULT::invaild;
						*m_PragmaBegin = finalWordBegin;
						*m_PragmaEnd = finalWordEnd;
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
						if (*m_RefererBegin)
							return PARSERESULT::invaild;
						*m_RefererBegin = finalWordBegin;
						*m_RefererEnd = finalWordEnd;
					}

					break;

				case 'u':
				case 'U':
					//"Upgrade"
					if (std::equal(finalHeadBegin, finalHeadEnd, Upgrade.cbegin(), Upgrade.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_UpgradeBegin)
							return PARSERESULT::invaild;
						*m_UpgradeBegin = finalWordBegin;
						*m_UpgradeEnd = finalWordEnd;
					}

					break;

				case 'w':
				case 'W':
					//"Warning"
					if (std::equal(finalHeadBegin, finalHeadEnd, Warning.cbegin(), Warning.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_WarningBegin)
							return PARSERESULT::invaild;
						*m_WarningBegin = finalWordBegin;
						*m_WarningEnd = finalWordEnd;
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
						if (*m_If_MatchBegin)
							return PARSERESULT::invaild;
						*m_If_MatchBegin = finalWordBegin;
						*m_If_MatchEnd = finalWordEnd;
					}

					break;

				case 'r':
				case 'R':
					//"If-Range"
					if (std::equal(finalHeadBegin, finalHeadEnd, If_Range.cbegin(), If_Range.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_If_RangeBegin)
							return PARSERESULT::invaild;
						*m_If_RangeBegin = finalWordBegin;
						*m_If_RangeEnd = finalWordEnd;
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
						if (*m_ConnectionBegin)
							return PARSERESULT::invaild;
						*m_ConnectionBegin = finalWordBegin;
						*m_ConnectionEnd = finalWordEnd;
					}

					break;

				case 'U':
				case 'u':
					//"User-Agent"
					if (std::equal(finalHeadBegin, finalHeadEnd, User_Agent.cbegin(), User_Agent.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_User_AgentBegin)
							return PARSERESULT::invaild;
						*m_User_AgentBegin = finalWordBegin;
						*m_User_AgentEnd = finalWordEnd;
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
						if (*m_Content_TypeBegin)
							return PARSERESULT::invaild;
						*m_Content_TypeBegin = finalWordBegin;
						*m_Content_TypeEnd = finalWordEnd;
					}

					break;

				case 'm':
				case 'M':
					//"Max-Forwards"
					if (std::equal(finalHeadBegin, finalHeadEnd, Max_Forwards.cbegin(), Max_Forwards.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Max_ForwardsBegin)
							return PARSERESULT::invaild;
						*m_Max_ForwardsBegin = finalWordBegin;
						*m_Max_ForwardsEnd = finalWordEnd;
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
						if (*m_Accept_RangesBegin)
							return PARSERESULT::invaild;
						*m_Accept_RangesBegin = finalWordBegin;
						*m_Accept_RangesEnd = finalWordEnd;
					}

					break;

				case 'u':
				case 'U':
					//"Authorization"
					if (std::equal(finalHeadBegin, finalHeadEnd, Authorization.cbegin(), Authorization.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_AuthorizationBegin)
							return PARSERESULT::invaild;
						*m_AuthorizationBegin = finalWordBegin;
						*m_AuthorizationEnd = finalWordEnd;
					}

					break;

				case 'a':
				case 'A':
					// "Cache-Control"
					if (std::equal(finalHeadBegin, finalHeadEnd, Cache_Control.cbegin(), Cache_Control.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Cache_ControlBegin)
							return PARSERESULT::invaild;
						*m_Cache_ControlBegin = finalWordBegin;
						*m_Cache_ControlEnd = finalWordEnd;
					}

					break;

				case 'f':
				case 'F':
					// "If-None-Match"
					if (std::equal(finalHeadBegin, finalHeadEnd, If_None_Match.cbegin(), If_None_Match.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_If_None_MatchBegin)
							return PARSERESULT::invaild;
						*m_If_None_MatchBegin = finalWordBegin;
						*m_If_None_MatchEnd = finalWordEnd;
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
						if (*m_Accept_CharsetBegin)
							return PARSERESULT::invaild;
						*m_Accept_CharsetBegin = finalWordBegin;
						*m_Accept_CharsetEnd = finalWordEnd;
					}

					break;

				case 'c':
				case 'C':
					//"Content-Length"

					if (std::equal(finalHeadBegin, finalHeadEnd, ContentLength.cbegin(), ContentLength.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Content_LengthBegin)
							return PARSERESULT::invaild;

						if (!std::distance(finalWordBegin, finalWordEnd))
							return PARSERESULT::invaild;


						if (std::all_of(finalWordBegin, finalWordEnd, std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'),
							std::bind(std::less_equal<>(), std::placeholders::_1, '9')
						)))
						{
							index = -1, num = 1;
							m_bodyLen = std::accumulate(std::make_reverse_iterator(finalWordEnd), std::make_reverse_iterator(finalWordBegin), 0, [&index, &num](auto &sum, auto const ch)
							{
								if (++index)num *= 10;
								return sum += (ch - '0')*num;
							});
						}

						*m_Content_LengthBegin = finalWordBegin;
						*m_Content_LengthEnd = finalWordEnd;
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
						if (*m_Accept_EncodingBegin)
							return PARSERESULT::invaild;
						*m_Accept_EncodingBegin = finalWordBegin;
						*m_Accept_EncodingEnd = finalWordEnd;
					}

					break;

				case 'l':
				case 'L':
					//"Accept-Language"
					if (std::equal(finalHeadBegin, finalHeadEnd, Accept_Language.cbegin(), Accept_Language.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Accept_LanguageBegin)
							return PARSERESULT::invaild;
						*m_Accept_LanguageBegin = finalWordBegin;
						*m_Accept_LanguageEnd = finalWordEnd;
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
						if (*m_If_Modified_SinceBegin)
							return PARSERESULT::invaild;
						*m_If_Modified_SinceBegin = finalWordBegin;
						*m_If_Modified_SinceEnd = finalWordEnd;
					}

					break;

				case 't':
				case 'T':
					//Transfer-Encoding
					if (std::equal(finalHeadBegin, finalHeadEnd, Transfer_Encoding.cbegin(), Transfer_Encoding.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Transfer_EncodingBegin)
							return PARSERESULT::invaild;
						if (std::equal(finalWordBegin, finalWordEnd, chunked.cbegin(), chunked.cend()))
							hasChunk = true;
						*m_Transfer_EncodingBegin = finalWordBegin;
						*m_Transfer_EncodingEnd = finalWordEnd;
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
						if (*m_If_Unmodified_SinceBegin)
							return PARSERESULT::invaild;
						*m_If_Unmodified_SinceBegin = finalWordBegin;
						*m_If_Unmodified_SinceEnd = finalWordEnd;
					}

					break;

				case 'p':
				case 'P':
					// "Proxy-Authorization"
					if (std::equal(finalHeadBegin, finalHeadEnd, Proxy_Authorization.cbegin(), Proxy_Authorization.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Proxy_AuthorizationBegin)
							return PARSERESULT::invaild;
						*m_Proxy_AuthorizationBegin = finalWordBegin;
						*m_Proxy_AuthorizationEnd = finalWordEnd;
					}

					break;

				default:

					break;
				}
				break;


			default:

				break;
			}



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
					return PARSERESULT::invaild;

			}
			else
			{
				
				lineEnd = std::find_if(iterFindBegin, iterFindEnd, std::bind(std::logical_and<>(), std::bind(std::not_equal_to<>(), std::placeholders::_1, '\r'),
					std::bind(std::not_equal_to<>(), std::placeholders::_1, '\n')));

				if (lineEnd == iterFindEnd)
				{
					if (!m_bodyLen && !hasChunk)
					{
						if (dataBufferVec.empty())
						{
							if (std::distance(lineBegin, lineEnd) == 4)
							{
								m_messageBegin = lineEnd;
								goto check_messageComplete;
							}
						}
						else
						{
							if (std::distance(dataBufferVec.front().first, dataBufferVec.front().second) + std::distance(m_readBuffer, const_cast<char*>(lineEnd)) == 4)
							{
								m_messageBegin = lineEnd;
								goto check_messageComplete;
							}
						}
					}
					accumulateLen += len;
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


						newBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

						if (!newBuffer)
							return PARSERESULT::invaild;

						m_readBuffer = newBuffer;
						m_startPos = 0;
						return PARSERESULT::find_secondLineEnd;
					}
				}
			}


			//如果很长，横跨了不同分段，那么进行整合再处理
			if (!dataBufferVec.empty())
			{
				
				accumulateLen+= std::distance(m_readBuffer, const_cast<char*>(lineEnd));

				newBuffer = m_charMemoryPool.getMemory(accumulateLen);

				if (!newBuffer)
					return PARSERESULT::invaild;

				index = 0;

				newBufferIter = newBuffer;

				for (auto const &singlePair : dataBufferVec)
				{
					std::copy(singlePair.first, singlePair.second, newBufferIter);
					newBufferIter += std::distance(singlePair.first, singlePair.second);
				}
				std::copy(m_readBuffer, const_cast<char*>(lineEnd), newBufferIter);

				finalLineBegin = newBuffer, finalLineEnd = newBuffer + accumulateLen;

				dataBufferVec.clear();
			}
			else
			{
				finalLineBegin = lineBegin, finalLineEnd = lineEnd;
			}


			len = std::distance(finalLineBegin, finalLineEnd);

			if (len != 2 && len != 4)
				return PARSERESULT::invaild;


			if (len == 2 && !std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cbegin() + 2))
				return PARSERESULT::invaild;
		}
	}


	//在content-lengeh有长度时首先考虑是否是MultiPartFormData文件上传格式，之后才考虑常规body格式
	//无长度时考虑是否是chunk
	if (len == 4)
	{
		if (!std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cend()))
			return PARSERESULT::invaild;


		if (m_bodyLen)
		{
			if (*m_Content_TypeBegin && *m_Content_TypeEnd)
			{
				if (**m_Content_TypeBegin == 'm' && (m_boundaryBegin = std::search(*m_Content_TypeBegin, *m_Content_TypeEnd, boundary.cbegin(), boundary.cend())) != *m_Content_TypeEnd)
				{
					*(m_verifyData.get() + VerifyDataPos::customPos1Begin) = lineEnd;
					*(m_verifyData.get() + VerifyDataPos::customPos1End) = iterFindEnd;
					m_boundaryBegin += boundary.size();
					m_boundaryEnd = *m_Content_TypeEnd;
					return PARSERESULT::parseMultiPartFormData;
				}
			}


			iterFindBegin = lineEnd;


		find_finalBodyEnd:
			if (std::distance(iterFindBegin, iterFindEnd) >= m_bodyLen)
			{
				finalBodyBegin = iterFindBegin, finalBodyEnd = finalBodyBegin + m_bodyLen;
				m_buffer->getView().setBody(finalBodyBegin, m_bodyLen);
				hasBody = true;
				m_messageBegin = iterFindBegin + m_bodyLen;
				goto check_messageComplete;
			}
			else
			{
				if (std::distance(iterFindBegin, iterFinalEnd) >= m_bodyLen)
				{
					bodyBegin = iterFindBegin;
					m_startPos = std::distance(m_readBuffer, const_cast<char*>(iterFindEnd));
				}
				else
				{
					char *newReadBuffer{ m_charMemoryPool.getMemory(m_bodyLen) };
					if (!newReadBuffer)
						return PARSERESULT::invaild;

					std::copy(iterFindBegin, iterFindEnd, newReadBuffer);

					m_startPos = std::distance(iterFindBegin, iterFindEnd);
					finalBodyBegin = m_readBuffer = newReadBuffer;
					m_maxReadLen = m_bodyLen;
				}
				return PARSERESULT::find_finalBodyEnd;
			}
		}
		else if (hasChunk)
		{
			bodyBegin = lineEnd;
			bodyEnd = iterFindEnd;

			return PARSERESULT::parseChunk;


		begin_checkChunkData:
			m_readBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

			if (!m_readBuffer)
				return PARSERESULT::invaild;

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
					return PARSERESULT::invaild;

				chunkNumBegin = iterFindBegin;


			find_chunkNumEnd:
				len = std::distance(chunkNumBegin, iterFindEnd);
				if (len >= MAXCHUNKNUMBERLEN)
				{
					iterFindThisEnd = chunkNumBegin + MAXCHUNKNUMBERLEN;

					if ((chunkNumEnd = std::find_if_not(chunkNumBegin, iterFindThisEnd, std::bind(::isalnum, std::placeholders::_1))) == iterFindThisEnd)
						return PARSERESULT::invaild;
				}
				else
				{

					if ((chunkNumEnd = std::find_if_not(chunkNumBegin, iterFindEnd, std::bind(::isalnum, std::placeholders::_1))) == iterFindEnd)
					{
						std::copy(chunkNumBegin, iterFindEnd, m_readBuffer);
						m_startPos = len;
						return PARSERESULT::find_chunkNumEnd;
					}
				}


				if (*chunkNumEnd != '\r')
					return PARSERESULT::invaild;


				index = -1, num = 1;
				m_chunkLen = std::accumulate(std::make_reverse_iterator(chunkNumEnd), std::make_reverse_iterator(chunkNumBegin), 0, [&index, &num](auto &sum, auto const ch)
				{
					if (++index)num *= 16;
					return sum += (std::isdigit(ch) ? ch - '0' : std::isupper(ch) ? (ch - 'A') : (ch - 'a'))*num;
				});



			find_thirdLineEnd:
				// \r\n   \r\n\r\n
				len = (m_chunkLen ? 2 : 4);
				index = std::distance(chunkNumEnd, iterFindEnd);
				if (index < len)
				{
					std::copy(chunkNumEnd, iterFindEnd, m_readBuffer);
					m_startPos = index;
					return PARSERESULT::find_thirdLineEnd;
				}


				if (!std::equal(lineStr.cbegin(), lineStr.cbegin() + len, chunkNumEnd, chunkNumEnd + len))
					return PARSERESULT::invaild;

				if (!m_chunkLen)
				{
					m_messageBegin = chunkNumEnd + len;
					goto check_messageComplete;
				}


			find_chunkDataBegin:
				chunkDataBegin = chunkNumEnd + len;



			find_chunkDataEnd:
				len = std::distance(chunkDataBegin, iterFindEnd);
				if (len < m_chunkLen)
				{
					//  chunkDataBegin, iterFindEnd  为接收的数据，需要保存
					m_chunkLen -= len;
					m_startPos = 0;
					return PARSERESULT::find_chunkDataEnd;
				}

				//  chunkDataBegin, chunkDataEnd  为接收的数据，需要保存
				chunkDataEnd = chunkDataBegin + m_chunkLen;



			find_fourthLineBegin:
				if (chunkDataEnd == iterFindEnd)
				{
					m_startPos = 0;
					return PARSERESULT::find_fourthLineBegin;
				}


			find_fourthLineEnd:
				len = std::distance(chunkDataEnd, iterFindEnd);
				if (len < 2)
				{
					std::copy(chunkDataEnd, iterFindEnd, m_readBuffer);
					m_startPos = len;
					return PARSERESULT::find_fourthLineEnd;
				}


				if (!std::equal(chunkDataEnd, chunkDataEnd + 2, lineStr.cbegin(), lineStr.cbegin() + 2))
					return PARSERESULT::invaild;


				iterFindBegin = chunkDataEnd + 2;

			}



		}
		m_messageBegin = finalLineEnd;
		goto check_messageComplete;
	}
	else
		return PARSERESULT::invaild;


	
	// 表单上传解析  multipart/form-data
	// 首先定位boundary开始位置   --加上boundary内容
	// 之后寻找 \r\n--加上boundary内容
	begin_checkBoundaryBegin:
		m_readBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

		if (!m_readBuffer)
			return PARSERESULT::invaild;

		m_boundaryHeaderPos = 0;
		*m_Boundary_NameBegin = *m_Boundary_FilenameBegin = *m_Boundary_ContentTypeBegin = *m_Boundary_ContentDispositionBegin = nullptr;

	find_boundaryBegin:
		if (findBoundaryBegin == iterFindEnd)
		{
			m_startPos = 0;
			m_bodyLen -= iterFindEnd - iterFindBegin;
			if (m_bodyLen <= 0)
				return PARSERESULT::invaild;
			return PARSERESULT::find_boundaryBegin;
		}

		if (*findBoundaryBegin != *m_boundaryBegin)
			return PARSERESULT::invaild;


	find_halfBoundaryBegin:
		if(m_bodyLen <= m_boundaryLen)
			return PARSERESULT::invaild;


		if ((len = std::distance(findBoundaryBegin, iterFindEnd)) < m_boundaryLen)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin + len;
			if (m_bodyLen <= 0)
				return PARSERESULT::invaild;
			std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
			m_startPos = len;
			return PARSERESULT::find_halfBoundaryBegin;
		}


		if (!std::equal(findBoundaryBegin, findBoundaryBegin + m_boundaryLen, m_boundaryBegin, m_boundaryEnd))
			return PARSERESULT::invaild;

		//如果相等

		findBoundaryBegin += m_boundaryLen;

		m_boundaryLen += 2;
		m_boundaryBegin -= 2;


	find_boundaryHeaderBegin:
		if ((len = std::distance(findBoundaryBegin, iterFindEnd)) < 2)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin - len;
			if (m_bodyLen <= 0)
				return PARSERESULT::invaild;
			std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
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
						return PARSERESULT::invaild;
					m_startPos = 0;
					return PARSERESULT::find_boundaryHeaderBegin2;
				}



			find_boundaryHeaderEnd:

				boundaryHeaderEnd = boundaryHeaderBegin;

				len = std::distance(boundaryHeaderEnd, iterFindEnd);
				iterFindThisEnd = len >= MAXBOUNDARYHEADERNLEN ? (boundaryHeaderEnd + MAXBOUNDARYHEADERNLEN) : iterFindEnd;

				if ((boundaryHeaderEnd = std::find_if(boundaryHeaderEnd, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ':'), std::bind(std::equal_to<>(), std::placeholders::_1, '=')))) == iterFindEnd)
				{
					if (std::distance(boundaryHeaderBegin, boundaryHeaderEnd) >= MAXBOUNDARYHEADERNLEN)
						return PARSERESULT::invaild;
					else
					{
						m_bodyLen -= iterFindEnd - iterFindBegin - len;
						if (m_bodyLen <= 0)
							return PARSERESULT::invaild;
						std::copy(boundaryHeaderBegin, boundaryHeaderEnd, m_readBuffer);
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
							return PARSERESULT::invaild;
						m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::NameLen;
					}
					break;
				case HTTPBOUNDARYHEADERLEN::FilenameLen:
					if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Filename.cbegin(), Filename.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Boundary_FilenameBegin)
							return PARSERESULT::invaild;
						m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::FilenameLen;
					}
					break;
				case HTTPBOUNDARYHEADERLEN::Content_TypeLen:
					if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Content_Type.cbegin(), Content_Type.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Boundary_ContentTypeBegin)
							return PARSERESULT::invaild;
						m_boundaryHeaderPos = HTTPBOUNDARYHEADERLEN::Content_TypeLen;
					}
					break;
				case HTTPBOUNDARYHEADERLEN::Content_DispositionLen:
					if (std::equal(boundaryHeaderBegin, boundaryHeaderEnd, Content_Disposition.cbegin(), Content_Disposition.cend(),
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
					{
						if (*m_Boundary_ContentDispositionBegin)
							return PARSERESULT::invaild;
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
						return PARSERESULT::invaild;
					m_startPos = 0;
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
						return PARSERESULT::invaild;
					m_startPos = 0;
					return PARSERESULT::find_boundaryWordBegin2;
				}

				thisBoundaryWordBegin = boundaryWordEnd = boundaryWordBegin;

			find_boundaryWordEnd:
				if (isDoubleQuotation)
				{
					if ((boundaryWordEnd = std::find(boundaryWordEnd, iterFindEnd, '\"')) == iterFindEnd)
					{
						len = std::distance(boundaryWordBegin, boundaryWordEnd);
						if(len ==m_maxReadLen)
							return PARSERESULT::invaild;
						m_bodyLen -= iterFindEnd - iterFindBegin - len;
						if (m_bodyLen <= 0)
							return PARSERESULT::invaild;
						std::copy(boundaryWordBegin, boundaryWordEnd, m_readBuffer);
						m_startPos = len;
						return PARSERESULT::find_boundaryWordEnd;
					}

					thisBoundaryWordEnd = boundaryWordEnd;
				}


			find_boundaryWordEnd2:
				if ((boundaryWordEnd = std::find_if(boundaryWordEnd, iterFindEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ';'), std::bind(std::equal_to<>(), std::placeholders::_1, '\r')))) == iterFindEnd)
				{
					if (isDoubleQuotation)
					{
						m_bodyLen -= iterFindEnd - iterFindBegin;
						if (m_bodyLen <= 0)
							return PARSERESULT::invaild;
						m_startPos = 0;
						return PARSERESULT::find_boundaryWordEnd2;
					}
					else
					{
						len = std::distance(boundaryWordBegin, boundaryWordEnd);
						if (len == m_maxReadLen)
							return PARSERESULT::invaild;
						m_bodyLen -= iterFindEnd - iterFindBegin - len;
						if (m_bodyLen <= 0)
							return PARSERESULT::invaild;
						std::copy(boundaryWordBegin, iterFindEnd, m_readBuffer);
						m_startPos = len;
						return PARSERESULT::find_boundaryWordEnd2;
					}
				}


				if (!isDoubleQuotation)
					thisBoundaryWordEnd = boundaryWordEnd;


				len = std::distance(thisBoundaryWordBegin, thisBoundaryWordEnd);
				
				if (len)
				{
					newBuffer = m_charMemoryPool.getMemory(len);

					if (!newBuffer)
					{
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
					if ((len = std::distance(boundaryWordEnd, iterFindEnd)) < 2)
					{
						m_bodyLen -= iterFindEnd - iterFindBegin - len;
						if (m_bodyLen <= 0)
							return PARSERESULT::invaild;
						std::copy(boundaryWordEnd, iterFindEnd, m_readBuffer);
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
						return PARSERESULT::invaild;
					m_startPos = 0;
					return PARSERESULT::find_nextboundaryHeaderBegin3;
				}

			} while (*boundaryHeaderBegin != '\r');

		find_fileBegin:
			if ((len = std::distance(boundaryHeaderBegin, iterFindEnd)) < 2)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin - len;
				if (m_bodyLen <= 0)
					return PARSERESULT::invaild;
				std::copy(boundaryHeaderBegin, iterFindEnd, m_readBuffer);
				m_startPos = len;
				return PARSERESULT::find_fileBegin;
			}

			boundaryHeaderBegin += 2;

			findBoundaryBegin = boundaryHeaderBegin;



		find_fileBoundaryBegin:
			boundaryHeaderBegin = findBoundaryBegin;

			


			do
			{
				if ((findBoundaryBegin = std::find(findBoundaryBegin, iterFindEnd, *m_boundaryBegin)) == iterFindEnd)
				{
					//boundaryHeaderBegin 到 findBoundaryBegin 为文件内容



					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
						return PARSERESULT::invaild;
					m_startPos = 0;
					return PARSERESULT::find_fileBoundaryBegin;
				}


				if (std::distance(findBoundaryBegin, iterFindEnd) < m_boundaryLen)
				{
					//boundaryHeaderBegin 到 findBoundaryBegin 为文件内容

					len = std::distance(findBoundaryBegin, iterFindEnd);
					m_bodyLen -= iterFindEnd - iterFindBegin - len;
					if (m_bodyLen <= 0)
						return PARSERESULT::invaild;
					std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
					m_startPos = len;
					return PARSERESULT::find_fileBoundaryBegin;
				}

				if (std::equal(findBoundaryBegin, findBoundaryBegin + m_boundaryLen, m_boundaryBegin, m_boundaryEnd))
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
					return PARSERESULT::invaild;
				m_startPos = 0;
				return PARSERESULT::check_fileBoundaryEnd;
			}

			if (*findBoundaryBegin != '-' && *findBoundaryBegin != '\r')
				return PARSERESULT::invaild;


		check_fileBoundaryEnd2:
			if ((len = std::distance(findBoundaryBegin, iterFindEnd)) < 4)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin - len;
				if (m_bodyLen <= 0)
					return PARSERESULT::invaild;
				std::copy(findBoundaryBegin, iterFindEnd, m_readBuffer);
				m_startPos = len;
				return PARSERESULT::check_fileBoundaryEnd2;
			}

			if (m_bodyLen - std::distance(iterFindBegin, findBoundaryBegin) == 4)
			{
				m_messageBegin = findBoundaryBegin;
				goto check_messageComplete;
			}

			findBoundaryBegin += 2;
		}


		// 检查是否有额外信息
	check_messageComplete:
		if (m_messageBegin == iterFindEnd)
			return PARSERESULT::complete;
		m_messageEnd = iterFindEnd;
		return PARSERESULT::check_messageComplete;
}




void HTTPSERVICE::startWrite(const char *source, const int size)
{
	startWriteLoop(source, size);

	std::fill(m_httpHeaderMap.get(), m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN, nullptr);
	m_charMemoryPool.prepare();
	m_charPointerMemoryPool.prepare();
}




void HTTPSERVICE::startWriteLoop(const char * source, const int size)
{
	boost::asio::async_write(*m_buffer->getSock(), boost::asio::buffer(source, size), [this](const boost::system::error_code &err, std::size_t size)
	{
		m_sendMemoryPool.prepare();
		if (err)
		{
			m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
			// 修复发生错误时不会触发回收的情况
			if (err != boost::asio::error::operation_aborted)
			{
				m_lock.lock();
				m_hasClean = false;
				m_lock.unlock();
			}
		}
		else
		{
			//  https://blog.csdn.net/weixin_43827934/article/details/86362985?utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromMachineLearnPai2%7Edefault-5.base&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromMachineLearnPai2%7Edefault-5.base
			switch (m_parseStatus)
			{
			case PARSERESULT::complete:
				startRead();
				break;
			case PARSERESULT::check_messageComplete:
				if (m_readBuffer != m_buffer->getBuffer())
				{
					std::copy(m_messageBegin, m_messageEnd, m_buffer->getBuffer());
					m_readBuffer = m_buffer->getBuffer();
					m_messageBegin = m_readBuffer, m_messageEnd = m_readBuffer + (m_messageEnd - m_messageBegin);
					m_maxReadLen = m_defaultReadLen;
				}
				parseReadData(m_messageBegin, m_messageEnd - m_messageBegin);
				break;
			}
		}
	});
}









































