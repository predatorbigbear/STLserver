#include "httpService.h"





HTTPSERVICE::HTTPSERVICE(std::shared_ptr<io_context> ioc, std::shared_ptr<LOG> log, const std::string &doc_root,
	std::shared_ptr<MULTISQLREADSW>multiSqlReadSWSlave,
	std::shared_ptr<MULTISQLREADSW>multiSqlReadSWMaster, std::shared_ptr<MULTIREDISREAD>multiRedisReadSlave,
	std::shared_ptr<MULTIREDISREAD>multiRedisReadMaster,
	std::shared_ptr<MULTIREDISWRITE>multiRedisWriteMaster, std::shared_ptr<MULTISQLWRITESW>multiSqlWriteSWMaster,
	const unsigned int timeOut, bool & success,
	char *publicKeyBegin, char *publicKeyEnd, int publicKeyLen, char *privateKeyBegin, char *privateKeyEnd, int privateKeyLen,
	RSA* rsaPublic, RSA* rsaPrivate
	)
	:m_ioc(ioc), m_log(log), m_doc_root(doc_root),
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
		if (m_doc_root.empty())
			throw std::runtime_error("m_doc_root is empty");

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
	// TODO: ?????????? return ????
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
	ReadBuffer &refBuffer{ *m_buffer };
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
		if (!refBuffer.getView().target().empty())
		{
			testGET();
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		}
		break;


	default:

		startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		break;
	}
}




void HTTPSERVICE::switchPOSTInterface()
{
	ReadBuffer &refBuffer{ *m_buffer };
	int index{ -1 }, num{ 1 };
	switch (std::accumulate(std::make_reverse_iterator(refBuffer.getView().target().cend()), std::make_reverse_iterator(refBuffer.getView().target().cbegin() + 1), 0, [&index, &num](auto &sum, auto const ch)
	{
		if (++index)num *= 10;
		return sum += (ch - '0')*num;
	}))
	{

		//????body??????????????
	case INTERFACE::testBodyParse:
		testBody();
		break;


		//????ping pong
	case INTERFACE::pingPong:
		testPingPong();
		break;


		//????????sql????
	case INTERFACE::multiSqlReadSW:
		testmultiSqlReadSW();
		break;


		//????redis ????LOT_SIZE_STRING????
	case INTERFACE::multiRedisReadLOT_SIZE_STRING:
		testMultiRedisReadLOT_SIZE_STRING();
		break;


		//????redis ????INTERGER????
	case INTERFACE::multiRedisReadINTERGER:
		testMultiRedisReadINTERGER();
		break;


		//????redis ????ARRAY????
	case INTERFACE::multiRedisReadARRAY:
		testMultiRedisReadARRAY();
		break;


		//????body????????????
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


	case INTERFACE::testPingPongJson:
		testPingPongJson();
		break;


	case INTERFACE::testMakeJson:
		testMakeJson();
		break;


	case INTERFACE::testCompareWorkFlow:
		testCompareWorkFlow();
		break;

		//????????????????????????
	default:
		startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		break;
	}
}





void HTTPSERVICE::testBody()
{
	if (!hasBody)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
	
	if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);


	m_buffer->setBodyLen(m_len);


	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::test, STATICSTRING::testLen, STATICSTRING::parameter, STATICSTRING::parameterLen, STATICSTRING::number, STATICSTRING::numberLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
	
	startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
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
			return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

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

			startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		}
	}



	else if(hasBody)
	{
		if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
			return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

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

			startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
		}
		else
		{
			startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		}
	}
	else
	{
		//   ????Query Params ????????body??????????????????????????????????????????????????????????????


		startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
	}
}






void HTTPSERVICE::testPingPong()
{
	int bodyLen{ m_buffer->getView().body().size() };
	if (!bodyLen)
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBody, HTTPRESPONSEREADY::http11OKNoBodyLen);
	}
	else
	{
		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		char *sendBuffer{};
		unsigned int sendLen{};

		if (st1.make_pingPongResppnse(sendBuffer, sendLen, nullptr, 0,
			m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
			MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
			m_finalBodyBegin, m_finalBodyEnd,
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
}



void HTTPSERVICE::testPingPongJson()
{
	int bodyLen{ m_buffer->getView().body().size() };


	if (!bodyLen)
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBodyJson, HTTPRESPONSEREADY::http11OKNoBodyJsonLen);
		return;
	}
	
	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	// ????????clear
	if (!st1.clear())
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBodyJson, HTTPRESPONSEREADY::http11OKNoBodyJsonLen);
		return;
	}

	//????json????????????????????????????????????????????????????????????<TRANSFORMTYPE>
	if (!st1.put<TRANSFORMTYPE>(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, &*m_buffer->getView().body().cbegin(), &*m_buffer->getView().body().cend()))
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBodyJson, HTTPRESPONSEREADY::http11OKNoBodyJsonLen);
		return;
	}

	makeSendJson<TRANSFORMTYPE>(st1);
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



//  ??????foo ????????  set foo hello_world
//  ??????redis????dLOT_SIZE_STRING????????

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




//????????????????????????????????
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



//????????string_view??
//????????????
//????????????????????????????redis RESP??????????
//????????????  ????????????????????????????????????????????

//????????string_view
//??????????????????

//????????  
//????????????????????
//1 ????????????????????????
//2 ??????????????????????????????
//3 ????????????????????????0 ????????????????
//
//??????????????????????????????????????????????Y??????????????????????????????????????redis??????????

//????strlen ????????????????????????????????redis??
//??????????????????????????????????1024????????????????????????????????
//??????????????????????????????????????????????????404????????????????????????????
//????append??????redis??
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
		//??????/????????????????????????????????????login.html
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		if (std::equal(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), STATICSTRING::forwardSlash, STATICSTRING::forwardSlash + STATICSTRING::forwardSlashLen))
			m_buffer->getView().setTarget(STATICSTRING::loginHtml, STATICSTRING::loginHtmlLen);
		else
		{
			auto targetBegin{ std::find_if(m_buffer->getView().target().cbegin(),m_buffer->getView().target().cend(),std::bind(std::not_equal_to<>(), std::placeholders::_1, '/')) };
			m_buffer->getView().setTarget(targetBegin, m_buffer->getView().target().size() - std::distance(m_buffer->getView().target().cbegin(), targetBegin));
		}
		int needLen{ STATICSTRING::fileLockLen + 1 + m_buffer->getView().target().size() };
		char *newBuffer{ m_charMemoryPool.getMemory(needLen) }, *newBufferIter{};

		if(!newBuffer)
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		newBufferIter = newBuffer;
		std::copy(STATICSTRING::fileLock, STATICSTRING::fileLock + STATICSTRING::fileLockLen, newBufferIter);
		newBufferIter += STATICSTRING::fileLockLen;
		*newBufferIter++ = ':';
		std::copy(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), newBufferIter);

		command.emplace_back(std::string_view(newBuffer, needLen));
		commandSize.emplace_back(2);
		
		command.emplace_back(std::string_view(STATICSTRING::strlenStr, STATICSTRING::strlenStrLen));
		command.emplace_back(m_buffer->getView().target());
		commandSize.emplace_back(2);


		std::get<1>(*redisRequest) = 2;
		std::get<3>(*redisRequest) = 2;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleTestGETFileLock, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}

//????????string_view
//??????????????????
//????????????????????????Y??????????????????????????????????????redis??????????
void HTTPSERVICE::handleTestGETFileLock(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };
	
	bool isFileLock{ false };
	int filePos{};
	int index{ -1 }, num{ 1 };

	if (result)
	{	
		if (!resultVec[filePos].empty())
			isFileLock = true;

		++filePos;

		m_fileSize = std::accumulate(std::make_reverse_iterator(resultVec[filePos].cend()), std::make_reverse_iterator(resultVec[filePos].cbegin()), 0, [&index, &num](auto &sum, char ch)
		{
			if (++index)num *= 10;
			return sum += (ch - '0')*num;
		});


		// ??????????
		// ????????????http????
		// 
		//????isFileLock??true??????????????????????????????redis????????????????????
		//????isFileLock??false??????????????
		//1 ????fileLen????0????????????redis??????????????redis??????
		//2 ????fileLen??0????????????????????????????????/?????????????? * 1??????reids??????????

		if (isFileLock)
		{
			//??????????????
			readyReadFileFromDisk(m_buffer->getView().target());
		}
		else
		{
			if (m_fileSize)
			{
				//??????redis????
				
			}
			else
			{
				//??????????????
				//??????????redis
				//????????
			}
		}
	}
	else
	{
		if (em == ERRORMESSAGE::REDIS_ERROR)
		{
			//??????????????
			readyReadFileFromDisk(m_buffer->getView().target());
		}
		else
			handleERRORMESSAGE(em);
	}
}



void HTTPSERVICE::readyReadFileFromDisk(std::string_view fileName)
{
	if (fileName.empty())
		return startWrite(HTTPRESPONSEREADY::httpFailToMakeFilePath, HTTPRESPONSEREADY::httpFailToMakeFilePathLen);
	int needLen{ m_doc_root.size() + fileName.size() + 1 };
	//????doc_root??????target??????????/

	char *newBuffer{ m_charMemoryPool.getMemory(needLen) };
	if (!newBuffer)
		return startWrite(HTTPRESPONSEREADY::httpFailToMakeFilePath, HTTPRESPONSEREADY::httpFailToMakeFilePathLen);

	char *newBufferIter{ newBuffer };

	std::copy(m_doc_root.cbegin(), m_doc_root.cend(), newBufferIter);
	newBufferIter += m_doc_root.size();
	*newBufferIter++ = '/';

	std::copy(fileName.cbegin(), fileName.cend(), newBufferIter);
	newBufferIter += fileName.size();

	m_buffer->setFileName(newBuffer, needLen);

	//m_fileName??????????
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::error_code err{};
	m_fileSize = fs::file_size(m_buffer->fileName(), err);
	if (!m_fileSize || err)
		return startWrite(HTTPRESPONSEREADY::httpFailToGetFileSize, HTTPRESPONSEREADY::httpFailToGetFileSizeLen);

	//////////////////////////////////////////////////////////////////////????????????

	err = {};
	if (!fs::exists(m_buffer->fileName(), err))
		return startWrite(HTTPRESPONSEREADY::httpFailToOpenFile, HTTPRESPONSEREADY::httpFailToOpenFileLen);
	m_file.open(std::string(m_buffer->fileName().cbegin(), m_buffer->fileName().cend()), std::ios::binary);
	if(!m_file)
		return startWrite(HTTPRESPONSEREADY::httpFailToOpenFile, HTTPRESPONSEREADY::httpFailToOpenFileLen);

	/////////////////////////////????????   /////////////////////

	char *sendBuffer{};
	unsigned int sendLen{}, fileSeconds{ 99999 };
	m_readFileSize = 0;

	static std::function<void(char *&)>excuFun{ [fileSeconds](char *&buffer)
	{
		std::copy(MAKEJSON::CacheControl, MAKEJSON::CacheControl + MAKEJSON::CacheControlLen, buffer);
		buffer += MAKEJSON::CacheControlLen;
		*buffer++ = ':';

		std::copy(MAKEJSON::maxAge, MAKEJSON::maxAge + MAKEJSON::maxAgeLen, buffer);
		buffer += MAKEJSON::maxAgeLen;

		*buffer++ = '=';

		if (fileSeconds > 99999999)
			*buffer++ = fileSeconds / 100000000 + '0';
		if (fileSeconds > 9999999)
			*buffer++ = fileSeconds / 10000000 % 10 + '0';
		if (fileSeconds > 999999)
			*buffer++ = fileSeconds / 1000000 % 10 + '0';
		if (fileSeconds > 99999)
			*buffer++ = fileSeconds / 100000 % 10 + '0';
		if (fileSeconds > 9999)
			*buffer++ = fileSeconds / 10000 % 10 + '0';
		if (fileSeconds > 999)
			*buffer++ = fileSeconds / 1000 % 10 + '0';
		if (fileSeconds > 99)
			*buffer++ = fileSeconds / 100 % 10 + '0';
		if (fileSeconds > 9)
			*buffer++ = fileSeconds / 10 % 10 + '0';
		*buffer++ = fileSeconds % 10 + '0';

		*buffer++ = ',';

		std::copy(MAKEJSON::publicStr, MAKEJSON::publicStr + MAKEJSON::publicStrLen, buffer);
		buffer += MAKEJSON::publicStrLen;
	} };


	if (!makeFileFront<CUSTOMTAG>(m_buffer->fileName(), m_fileSize, m_defaultReadLen, sendBuffer, sendLen, excuFun,
		MAKEJSON::CacheControlLen + 1 + MAKEJSON::maxAgeLen + 1 + stringLen(fileSeconds) + 1 + MAKEJSON::publicStrLen,
		m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
		MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
		MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
		MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
		return startWrite(HTTPRESPONSEREADY::httpFailToMakeHttpFront, HTTPRESPONSEREADY::httpFailToMakeHttpFrontLen);

	//??????????sendBuffer ??m_defaultReadLen????????????
	// sendLen????body??????
	// m_sendFileSize??????????????????????????????
	m_sendBuffer = sendBuffer, m_sendLen = sendLen;

	//??????????????????????????????
	m_file.seekg(m_readFileSize, std::ios::beg);
	int maxReadfileLen{ m_defaultReadLen - m_sendLen };

	if (maxReadfileLen > m_fileSize)
		maxReadfileLen = m_fileSize;

	m_file.read(reinterpret_cast<char*>(const_cast<char*>(m_sendBuffer + m_sendLen)), maxReadfileLen);
	m_readFileSize = maxReadfileLen;

	startWrite<READFROMDISK>(m_sendBuffer, m_sendLen + maxReadfileLen);

}



void HTTPSERVICE::ReadFileFromDiskLoop()
{
	//????????????????????????????
	if (m_readFileSize == m_fileSize)
	{
		//????????????
		if (m_file.is_open())
			m_file.close();

		//????????
		recoverMemory();

		m_fileSize = 0;

		//??????????
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
		default:
			startRead();
			break;
		}
	}
	else
	{
		//????????????????????buffer??????????????
		m_file.seekg(m_readFileSize, std::ios::beg);
		int maxReadfileLen{ m_defaultReadLen  };


		if (maxReadfileLen > (m_fileSize - m_readFileSize))
			maxReadfileLen = (m_fileSize - m_readFileSize);

		m_file.read(reinterpret_cast<char*>(const_cast<char*>(m_sendBuffer)), maxReadfileLen);
		m_readFileSize += maxReadfileLen;

		startWrite<READFROMDISK>(m_sendBuffer, maxReadfileLen);
	}
}



void HTTPSERVICE::readyReadFileFromRedis(std::string_view fileName)
{
	if (fileName.empty())
		return startWrite(HTTPRESPONSEREADY::httpFailToMakeFilePath, HTTPRESPONSEREADY::httpFailToMakeFilePathLen);

	char *sendBuffer{};
	unsigned int sendLen{}, fileSeconds{ 99999 };
	m_readFileSize = 0;

	if (!makeFileFront<CUSTOMTAG>(fileName, m_fileSize, m_defaultReadLen, sendBuffer, sendLen, [fileSeconds](char *&buffer)
	{
		std::copy(MAKEJSON::CacheControl, MAKEJSON::CacheControl + MAKEJSON::CacheControlLen, buffer);
		buffer += MAKEJSON::CacheControlLen;
		*buffer++ = ':';

		std::copy(MAKEJSON::maxAge, MAKEJSON::maxAge + MAKEJSON::maxAgeLen, buffer);
		buffer += MAKEJSON::maxAgeLen;

		*buffer++ = '=';

		if (fileSeconds > 99999999)
			*buffer++ = fileSeconds / 100000000 + '0';
		if (fileSeconds > 9999999)
			*buffer++ = fileSeconds / 10000000 % 10 + '0';
		if (fileSeconds > 999999)
			*buffer++ = fileSeconds / 1000000 % 10 + '0';
		if (fileSeconds > 99999)
			*buffer++ = fileSeconds / 100000 % 10 + '0';
		if (fileSeconds > 9999)
			*buffer++ = fileSeconds / 10000 % 10 + '0';
		if (fileSeconds > 999)
			*buffer++ = fileSeconds / 1000 % 10 + '0';
		if (fileSeconds > 99)
			*buffer++ = fileSeconds / 100 % 10 + '0';
		if (fileSeconds > 9)
			*buffer++ = fileSeconds / 10 % 10 + '0';
		*buffer++ = fileSeconds % 10 + '0';

		*buffer++ = ',';

		std::copy(MAKEJSON::publicStr, MAKEJSON::publicStr + MAKEJSON::publicStrLen, buffer);
		buffer += MAKEJSON::publicStrLen;
	},
		MAKEJSON::CacheControlLen + 1 + MAKEJSON::maxAgeLen + 1 + stringLen(fileSeconds) + 1 + MAKEJSON::publicStrLen,
		m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
		MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
		MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
		MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
		return startWrite(HTTPRESPONSEREADY::httpFailToMakeHttpFront, HTTPRESPONSEREADY::httpFailToMakeHttpFrontLen);

	m_sendBuffer = sendBuffer, m_sendLen = sendLen;

	int maxReadfileLen{ m_defaultReadLen - m_sendLen };

	if (maxReadfileLen > m_fileSize)
		maxReadfileLen = m_fileSize;

	m_readFileSize = 0;
	m_sendTimes = 0;

	int needLen{ stringLen(maxReadfileLen) + 1 };

	char *numBuffer{ m_charMemoryPool.getMemory(needLen) }, *bufferIter{};

	if (!numBuffer)
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	bufferIter = numBuffer;

	*bufferIter++ = '0';

	if (maxReadfileLen > 99999999)
		*bufferIter++ = maxReadfileLen / 100000000 + '0';
	if (maxReadfileLen > 9999999)
		*bufferIter++ = maxReadfileLen / 10000000 % 10 + '0';
	if (maxReadfileLen > 999999)
		*bufferIter++ = maxReadfileLen / 1000000 % 10 + '0';
	if (maxReadfileLen > 99999)
		*bufferIter++ = maxReadfileLen / 100000 % 10 + '0';
	if (maxReadfileLen > 9999)
		*bufferIter++ = maxReadfileLen / 10000 % 10 + '0';
	if (maxReadfileLen > 999)
		*bufferIter++ = maxReadfileLen / 1000 % 10 + '0';
	if (maxReadfileLen > 99)
		*bufferIter++ = maxReadfileLen / 100 % 10 + '0';
	if (maxReadfileLen > 9)
		*bufferIter++ = maxReadfileLen / 10 % 10 + '0';
	*bufferIter++ = maxReadfileLen % 10 + '0';


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
		//??????/????????????????????????????????????login.html
		command.emplace_back(std::string_view(REDISNAMESPACE::getrange, REDISNAMESPACE::getrangeLen));
		command.emplace_back(m_buffer->getView().target());
		command.emplace_back(std::string_view(numBuffer, 1));
		command.emplace_back(std::string_view(numBuffer + 1, needLen - 1));

		commandSize.emplace_back(4);


		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleReadFileFromRedis, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception &e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
}



void HTTPSERVICE::handleReadFileFromRedis(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };

	int sendLen{};

	if (result)
	{
		//????????
		if (!m_sendTimes)
		{
			if (!resultVec.empty())
			{
				std::copy(resultVec[0].cbegin(), resultVec[0].cend(), const_cast<char*>(m_sendBuffer) + m_sendLen);
				sendLen = m_sendLen + resultVec[0].size();
				m_readFileSize= resultVec[0].size();
				startWrite<READFROMREDIS>(m_sendBuffer, sendLen);
			}
			else
			{
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		else
		{
			//??????????
			if (!resultVec.empty())
			{
				std::copy(resultVec[0].cbegin(), resultVec[0].cend(), const_cast<char*>(m_sendBuffer));
				sendLen = resultVec[0].size();
				m_readFileSize += resultVec[0].size();
				startWrite<READFROMREDIS>(m_sendBuffer, sendLen);
			}
			else
			{
				//??????redis????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????socket??????????????????????stratRead

				switch (m_parseStatus)
				{
				case PARSERESULT::complete:
					recoverMemory();
					startRead();
					break;
				case PARSERESULT::check_messageComplete:
					//??????????????????????????????????????????????????????????copy??????????????
					if (m_readBuffer != m_buffer->getBuffer())
					{
						std::copy(m_messageBegin, m_messageEnd, m_buffer->getBuffer());
						m_readBuffer = m_buffer->getBuffer();
						m_messageBegin = m_readBuffer, m_messageEnd = m_readBuffer + (m_messageEnd - m_messageBegin);
						m_maxReadLen = m_defaultReadLen;
					}
					recoverMemory();
					parseReadData(m_messageBegin, m_messageEnd - m_messageBegin);
					break;
					//default????????????????????????????????????
				default:
					recoverMemory();
					startRead();
					break;
				}
			}
		}
	}
	else
	{
		//????????????????????
		if (m_sendTimes)
		{
			handleERRORMESSAGE(em);
		}
		else
		{
			//??????redis????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????socket??????????????????????stratRead
			
			switch (m_parseStatus)
			{
			case PARSERESULT::complete:
				recoverMemory();
				startRead();
				break;
			case PARSERESULT::check_messageComplete:
				//??????????????????????????????????????????????????????????copy??????????????
				if (m_readBuffer != m_buffer->getBuffer())
				{
					std::copy(m_messageBegin, m_messageEnd, m_buffer->getBuffer());
					m_readBuffer = m_buffer->getBuffer();
					m_messageBegin = m_readBuffer, m_messageEnd = m_readBuffer + (m_messageEnd - m_messageBegin);
					m_maxReadLen = m_defaultReadLen;
				}
				recoverMemory();
				parseReadData(m_messageBegin, m_messageEnd - m_messageBegin);
				break;
				//default????????????????????????????????????
			default:
				recoverMemory();
				startRead();
				break;
			}
		}
	}
}



void HTTPSERVICE::ReadFileFromRedisLoop()
{
	//????????????????????????????
	if (m_readFileSize == m_fileSize)
	{
		//????????
		recoverMemory();

		m_fileSize = 0;

		//??????????
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
		default:
			startRead();
			break;
		}
	}
	else
	{
		//??????redis??????????????????buffer??????????????

		int maxReadfileLen{ m_defaultReadLen };

		if (maxReadfileLen > (m_fileSize - m_readFileSize))
			maxReadfileLen = (m_fileSize - m_readFileSize);

		int leftRange{ m_readFileSize },  rightRange{ m_readFileSize + maxReadfileLen };
		int leftLen{ stringLen(leftRange) }, rightLen{ stringLen(rightRange) };
		int needLen{ leftLen + rightLen };

		char *numBuffer{ m_charMemoryPool.getMemory(needLen) }, *bufferIter{};

		if (!numBuffer)
			return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

		bufferIter = numBuffer;

		
		if (leftRange > 99999999)
			*bufferIter++ = leftRange / 100000000 + '0';
		if (leftRange > 9999999)
			*bufferIter++ = leftRange / 10000000 % 10 + '0';
		if (leftRange > 999999)
			*bufferIter++ = leftRange / 1000000 % 10 + '0';
		if (leftRange > 99999)
			*bufferIter++ = leftRange / 100000 % 10 + '0';
		if (leftRange > 9999)
			*bufferIter++ = leftRange / 10000 % 10 + '0';
		if (leftRange > 999)
			*bufferIter++ = leftRange / 1000 % 10 + '0';
		if (leftRange > 99)
			*bufferIter++ = leftRange / 100 % 10 + '0';
		if (leftRange > 9)
			*bufferIter++ = leftRange / 10 % 10 + '0';
		*bufferIter++ = leftRange % 10 + '0';


		if (rightRange > 99999999)
			*bufferIter++ = rightRange / 100000000 + '0';
		if (rightRange > 9999999)
			*bufferIter++ = rightRange / 10000000 % 10 + '0';
		if (rightRange > 999999)
			*bufferIter++ = rightRange / 1000000 % 10 + '0';
		if (rightRange > 99999)
			*bufferIter++ = rightRange / 100000 % 10 + '0';
		if (rightRange > 9999)
			*bufferIter++ = rightRange / 10000 % 10 + '0';
		if (rightRange > 999)
			*bufferIter++ = rightRange / 1000 % 10 + '0';
		if (rightRange > 99)
			*bufferIter++ = rightRange / 100 % 10 + '0';
		if (rightRange > 9)
			*bufferIter++ = rightRange / 10 % 10 + '0';
		*bufferIter++ = rightRange % 10 + '0';



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
			//??????/????????????????????????????????????login.html
			command.emplace_back(std::string_view(REDISNAMESPACE::getrange, REDISNAMESPACE::getrangeLen));
			command.emplace_back(m_buffer->getView().target());
			command.emplace_back(std::string_view(numBuffer, leftLen));
			command.emplace_back(std::string_view(numBuffer + leftLen, rightLen));

			commandSize.emplace_back(4);


			std::get<1>(*redisRequest) = 1;
			std::get<3>(*redisRequest) = 1;
			std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleReadFileFromRedis, this, std::placeholders::_1, std::placeholders::_2);

			if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			{
				//????????
				recoverMemory();

				m_fileSize = 0;

				//??????????
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
				default:
					startRead();
					break;
				}

			}
		}
		catch (const std::exception &e)
		{
			//????????
			recoverMemory();

			m_fileSize = 0;

			//??????????
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
			default:
				startRead();
				break;
			}

		}
	}

}






bool HTTPSERVICE::makeFilePath(std::string_view fileName)
{
	if (fileName.empty())
		return false;
	int needLen{ m_doc_root.size() + fileName.size() + 1 };
	//????doc_root??????target??????????/

	char *newBuffer{ m_charMemoryPool.getMemory(needLen) };
	if (!newBuffer)
		return false;

	char *newBufferIter{ newBuffer };

	std::copy(m_doc_root.cbegin(), m_doc_root.cend(), newBufferIter);
	newBufferIter += m_doc_root.size();
	*newBufferIter++ = '/';

	std::copy(fileName.cbegin(), fileName.cend(), newBufferIter);
	newBufferIter += fileName.size();

	m_buffer->setFileName(newBuffer, needLen);
	return true;
}



bool HTTPSERVICE::getFileSize(std::string_view fileName)
{
	if (fileName.empty())
		return false;
	std::error_code err{};
	m_fileSize = fs::file_size(fileName, err);
	if(!m_fileSize || err)
		return false;
	return true;
}


bool HTTPSERVICE::openFile(std::string_view fileName)
{
	if (fileName.empty())
		return false;
	std::error_code err{};
	if (!fs::exists(fileName, err))
		return false;
	m_file.open(std::string(fileName.cbegin(), fileName.cend()), std::ios::binary);
	return m_file.is_open();
}




bool HTTPSERVICE::makeHttpFront(std::string_view fileName)
{
	char *sendBuffer{};
	unsigned int fileBodyBeginPos{};

	int fileSeconds{ 99999 };

	if (!makeFileFront<CUSTOMTAG>(m_buffer->fileName(), m_fileSize, m_defaultReadLen, sendBuffer, fileBodyBeginPos, [fileSeconds](char *&buffer)
	{
		std::copy(MAKEJSON::CacheControl, MAKEJSON::CacheControl + MAKEJSON::CacheControlLen, buffer);
		buffer += MAKEJSON::CacheControlLen;
		*buffer++ = ':';

		std::copy(MAKEJSON::maxAge, MAKEJSON::maxAge + MAKEJSON::maxAgeLen, buffer);
		buffer += MAKEJSON::maxAgeLen;

		*buffer++ = '=';

		if (fileSeconds > 99999999)
			*buffer++ = fileSeconds / 100000000 + '0';
		if (fileSeconds > 9999999)
			*buffer++ = fileSeconds / 10000000 % 10 + '0';
		if (fileSeconds > 999999)
			*buffer++ = fileSeconds / 1000000 % 10 + '0';
		if (fileSeconds > 99999)
			*buffer++ = fileSeconds / 100000 % 10 + '0';
		if (fileSeconds > 9999)
			*buffer++ = fileSeconds / 10000 % 10 + '0';
		if (fileSeconds > 999)
			*buffer++ = fileSeconds / 1000 % 10 + '0';
		if (fileSeconds > 99)
			*buffer++ = fileSeconds / 100 % 10 + '0';
		if (fileSeconds > 9)
			*buffer++ = fileSeconds / 10 % 10 + '0';
		*buffer++ = fileSeconds % 10 + '0';

		*buffer++ = ',';

		std::copy(MAKEJSON::publicStr, MAKEJSON::publicStr + MAKEJSON::publicStrLen, buffer);
		buffer += MAKEJSON::publicStrLen;
	},
		MAKEJSON::CacheControlLen + 1 + MAKEJSON::maxAgeLen + 1 + stringLen(fileSeconds) + 1 + MAKEJSON::publicStrLen,
		m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
		MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
		MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
		MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen))
	return false;
	return true;
}


// set k1 value22 EX 100 NX
void HTTPSERVICE::setFileLock(std::string_view fileName, const int fileSize)
{
	/*
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	char *newBuffer{}, *newBufferIter{};
	int needLen{}, locksec{};
	try
	{
		//??????/????????????????????????????????????login.html
		command.emplace_back(std::string_view(REDISNAMESPACE::set, REDISNAMESPACE::setLen));

		needLen = STATICSTRING::fileLockLen + 1 + m_buffer->getView().target().size();
		newBuffer = m_charMemoryPool.getMemory(needLen);

		if (!newBuffer)
		{
			m_getStatus = GETMETHODSTATUS::makeHttpFront_readFromDisk_sendClient;
			return GetStateMachine();
		}

		newBufferIter = newBuffer;
		std::copy(STATICSTRING::fileLock, STATICSTRING::fileLock + STATICSTRING::fileLockLen, newBufferIter);
		newBufferIter += STATICSTRING::fileLockLen;
		*newBufferIter++ = ':';
		std::copy(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), newBufferIter);

		command.emplace_back(std::string_view(STATICSTRING::lock, STATICSTRING::lockLen));
		command.emplace_back(std::string_view(STATICSTRING::EX, STATICSTRING::EXLen));

		locksec = m_fileSize / m_defaultReadLen;

		needLen = stringLen(locksec);

		newBuffer = m_charMemoryPool.getMemory(needLen);

		if (!newBuffer)
		{
			m_getStatus = GETMETHODSTATUS::makeHttpFront_readFromDisk_sendClient;
			return GetStateMachine();
		}

		newBufferIter = newBuffer;

		if (locksec > 99999999)
			*newBufferIter++ = locksec / 100000000 + '0';
		if (locksec > 9999999)
			*newBufferIter++ = locksec / 10000000 % 10 + '0';
		if (locksec > 999999)
			*newBufferIter++ = locksec / 1000000 % 10 + '0';
		if (locksec > 99999)
			*newBufferIter++ = locksec / 100000 % 10 + '0';
		if (locksec > 9999)
			*newBufferIter++ = locksec / 10000 % 10 + '0';
		if (locksec > 999)
			*newBufferIter++ = locksec / 1000 % 10 + '0';
		if (locksec > 99)
			*newBufferIter++ = locksec / 100 % 10 + '0';
		if (locksec > 9)
			*newBufferIter++ = locksec / 10 % 10 + '0';
		*newBufferIter++ = locksec % 10 + '0';

		command.emplace_back(std::string_view(newBuffer, needLen));

		command.emplace_back(std::string_view(STATICSTRING::NX, STATICSTRING::NXLen));

		commandSize.emplace_back(5);


		std::get<1>(*redisRequest) = 1;
		std::get<3>(*redisRequest) = 1;
		std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::handleSetFileLock, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
		{
			m_getStatus = GETMETHODSTATUS::makeHttpFront_readFromDisk_sendClient;
			return GetStateMachine();
		}
	}
	catch (const std::exception &e)
	{
		m_getStatus = GETMETHODSTATUS::makeHttpFront_readFromDisk_sendClient;
		return GetStateMachine();
	}
	*/
}


void HTTPSERVICE::handleSetFileLock(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };

	if (result)
	{




	}
	else
	{



	}
}




void HTTPSERVICE::handleTestGET(bool result, ERRORMESSAGE em)
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };
	if (result)
	{
		//????????????????????????????????????????
		if (em == ERRORMESSAGE::NO_KEY)
		{
			//std::cout << "no key\n";

			

		}
		else
		{
			//std::cout << "has key\n";

			

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

void HTTPSERVICE::readyGetFileFromDisk()
{
}




void HTTPSERVICE::testLogin()
{
	if (!hasBody)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
	
	
	if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	
	m_buffer->setBodyLen(m_len);

	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::username, STATICSTRING::usernameLen, STATICSTRING::password, STATICSTRING::passwordLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) },
		passwordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) };

	if (usernameView.empty() || passwordView.empty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

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

		//??????????????
		std::string_view &passwordView{ resultVec[0] }, &sessionView{ resultVec[1] }, &sessionExpireView{ resultVec[2] };

		if (passwordView.empty())
		{
			// ??redis????????????????????sql??????????????????????

			std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

			std::vector<std::string_view> &sqlCommand{ std::get<0>(*sqlRequest).get() };

			std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

			std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

			const char **BodyBuffer{ m_buffer->bodyPara() };

			std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

			sqlCommand.clear();
			rowField.clear();
			result.clear();

			try
			{
				//????????????????????sql????????????????????????????????sql??????????????????????????????copy??????????string_view??????
				//???????????????? + body??????????????????????????????????????????????????,????????buffer????????????????
				sqlCommand.emplace_back(std::string_view(SQLCOMMAND::testTestLoginSQL, SQLCOMMAND::testTestLoginSQLLen));
				sqlCommand.emplace_back(usernameView);
				sqlCommand.emplace_back(std::string_view(STATICSTRING::doubleQuotation, STATICSTRING::doubleQuotationLen));

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



		//????????
		const char **BodyBuffer{ m_buffer->bodyPara() };

		if (!std::equal(*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password), *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1), passwordView.cbegin(), passwordView.cend()))
			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);


		//????????????????????????session
		//hmset  user  session:usernameView   session  sessionEX:usernameView   sessionExpire
		if (sessionView.empty() || sessionExpireView.empty())
		{
			//????????????????????
			//????session   ????24??????
			//????24??????session
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


			//????????set-cookie??http????????????

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

				//????????set-cookie??http????????????
					//????????????????????????????????
					// DAY, DD MMM YYYY HH:MM:SS GMT
				m_sessionGMTClock = m_sessionClock + std::chrono::hours(8);

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
				m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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


		//????session????????
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
			//session??????????????????
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


			//  ????redis????session  ??  session????????
			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
			m_sessionLen = 24;

		}
		else
		{
			//????????????????

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




		//????????set-cookie??http????????????

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

			//????????set-cookie??http????????????
			//????????????????????????????????
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
			m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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

		if (rowField.empty() || result.empty() || (rowField.size() % 2))   //  ????????
			return startWrite(HTTPRESPONSEREADY::httpNoRecordInSql, HTTPRESPONSEREADY::httpNoRecordInSqlLen);

		const char **BodyBuffer{ m_buffer->bodyPara() };

		std::string_view CheckpasswordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) },
			&RealpasswordView{ result[0] };

		if (!std::equal(CheckpasswordView.cbegin(), CheckpasswordView.cend(), RealpasswordView.cbegin(), RealpasswordView.cend()))
		{
			do
			{
				//????24??????session
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


				//  ????redis????session  ??  session????????
				if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
					break;

			} while (false);


			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);
		}


		//????24??????session
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


		//  ????redis????session  ??  session????????
		if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
			return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
		m_sessionLen = 24;
		

		//????????set-cookie??http????????????

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

			//????????set-cookie??http????????????
				//????????????????????????????????
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
			m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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
	// ??redis????????session ????

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


	//////////////  ????????????????cookie??????????????????????????????

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
		m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	if (!UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);


	m_buffer->setBodyLen(m_len);


	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::publicKey, STATICSTRING::publicKeyLen,
		STATICSTRING::hashValue, STATICSTRING::hashValueLen, STATICSTRING::randomString, STATICSTRING::randomStringLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);


	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view publicKeyView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::publicKey),*(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::publicKey + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::publicKey) },
		hashValueView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::hashValue),*(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::hashValue + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::hashValue) },
		randomStringView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::randomString),*(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::randomString + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETCLIENTPUBLICKEY::randomString) };


	//????hash??????????????????
	
	///////////////////////////////////////////////////////////

	if(hashValueView.size()!= STATICSTRING::serverHashLen)
		return startWrite(HTTPRESPONSEREADY::http11InvaildHash, HTTPRESPONSEREADY::http11InvaildHashLen);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//????????????????????????????RSA_size?????? 
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

	//RSA??????????????????????????
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

	//??????????md5
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

	
	//??????????????
	if (!std::equal(iter, iterEnd, hashValueView.cbegin(), hashValueView.cend()))
	{
		return startWrite(HTTPRESPONSEREADY::http11InvaildHash, HTTPRESPONSEREADY::http11InvaildHashLen);
		m_hasMakeRandom = false;
	}

	//??????????????????
	//if (!std::equal(iter, iterEnd, hashValueView.cbegin(), hashValueView.cend(),std::bind(std::equal_to<>(),std::bind(::toupper,std::placeholders::_2), std::placeholders::_1)))
	//	return startWrite(HTTPRESPONSEREADY::http11invaildHash, HTTPRESPONSEREADY::http11invaildHashLen);



	// ??????????????????
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

	if (!(m_bio = BIO_new_mem_buf(&*publicKeyView.cbegin(), publicKeyView.size())))       //????????????RSA????
		return startWrite(HTTPRESPONSEREADY::http11WrongPublicKey, HTTPRESPONSEREADY::http11WrongPublicKeyLen);


	m_rsaClientPublic = PEM_read_bio_RSA_PUBKEY(m_bio, nullptr, nullptr, nullptr);   //??bio??????????rsa????
	if (!m_rsaClientPublic)
	{
		BIO_free_all(m_bio);
		return startWrite(HTTPRESPONSEREADY::http11WrongPublicKey, HTTPRESPONSEREADY::http11WrongPublicKeyLen);
	}

	m_rsaClientSize = RSA_size(m_rsaClientPublic);

	BIO_free_all(m_bio);
	m_bio = nullptr;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ????????????????????????????????????????????????????????????????????????????????
	// ??????????????????????????????????????
	// ????????????????????????????????


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

	//RSA??????????????????????????
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


	//??????????md5
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


	// iter    iterEnd??????????????????????

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

	return startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
}



void HTTPSERVICE::testGetEncryptKey()
{
	if(!m_firstTime || !hasBody || !UrlDecodeWithTransChinese(m_buffer->getView().body().cbegin(), m_buffer->getView().body().size(), m_len))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	
	m_buffer->setBodyLen(m_len);


	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getBodyLen(), m_buffer->bodyPara(), STATICSTRING::encryptKey, STATICSTRING::encryptKeyLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);


	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view encryptKeyView{ *(BodyBuffer + HTTPINTERFACENUM::TESTGETENCRYPTKEY::encryptKey),*(BodyBuffer + HTTPINTERFACENUM::TESTGETENCRYPTKEY::encryptKey + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTGETENCRYPTKEY::encryptKey) };

	if (encryptKeyView.size() != STATICSTRING::serverRSASize)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);


	int resultLen = RSA_private_decrypt(STATICSTRING::serverRSASize, reinterpret_cast<unsigned char*>(const_cast<char*>(&*encryptKeyView.cbegin())),
		reinterpret_cast<unsigned char*>(const_cast<char*>(&*encryptKeyView.cbegin())), m_rsaPrivate, RSA_NO_PADDING);

	if (resultLen != STATICSTRING::serverRSASize)
		return startWrite(HTTPRESPONSEREADY::httpRSAdecryptFail, HTTPRESPONSEREADY::httpRSAdecryptFailLen);

	std::string_view::const_iterator AESBegin{ encryptKeyView.cbegin() }, AESEnd{}, keyTimeBegin{}, keyTimeEnd{};

	if((AESEnd=std::find(AESBegin, encryptKeyView.cend(),0))== encryptKeyView.cend())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	if((keyTimeBegin=std::find_if(AESEnd, encryptKeyView.cend(),std::bind(std::logical_and<>(),std::bind(std::greater_equal<>(),std::placeholders::_1,'0'),
		std::bind(std::less_equal<>(), std::placeholders::_1, '9'))))== encryptKeyView.cend())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	keyTimeEnd = std::find_if_not(keyTimeBegin, encryptKeyView.cend(), std::bind(std::logical_and<>(), std::bind(std::greater_equal<>(), std::placeholders::_1, '0'),
		std::bind(std::less_equal<>(), std::placeholders::_1, '9')));

	int AESLen{ AESEnd - AESBegin };

	if(AESLen!=16 || AESLen!=24 || AESLen!=32)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	int index{ -1 };
	time_t baseNum{ 1 }, ten{ 10 };
	m_thisTime = std::accumulate(std::make_reverse_iterator(keyTimeEnd), std::make_reverse_iterator(keyTimeEnd), static_cast<time_t>(0), [&index, &baseNum, &ten](time_t &sum, auto const ch)
	{
		if (++index)baseNum *= ten;
		sum += (ch - '0')*baseNum;
		return sum;
	});
	
	if(m_thisTime <= m_firstTime)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

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
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	m_buffer->setBodyLen(m_len);

	if(std::distance(m_buffer->getView().body().cbegin(), m_buffer->getView().body().cbegin()+ m_buffer->getBodyLen()) % 16)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

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
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	//////////////////////////////////////////////////////////////////////////////////
	//????????????????testLogin??????

	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) },
		passwordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) };

	if (usernameView.empty() || passwordView.empty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);


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

		//??????????????
		std::string_view &passwordView{ resultVec[0] }, &sessionView{ resultVec[1] }, &sessionExpireView{ resultVec[2] };

		if (passwordView.empty())
		{
			// ??redis????????????????????sql??????????????????????

			std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

			std::vector<std::string_view> &sqlCommand{ std::get<0>(*sqlRequest).get() };

			std::vector<unsigned int> &rowField{ std::get<3>(*sqlRequest).get() };

			std::vector<std::string_view> &result{ std::get<4>(*sqlRequest).get() };

			const char **BodyBuffer{ m_buffer->bodyPara() };

			std::string_view usernameView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::username) };

			sqlCommand.clear();
			rowField.clear();
			result.clear();


			try
			{
				//????????????????????sql????????????????????????????????sql??????????????????????????????copy??????????string_view??????
				//???????????????? + body??????????????????????????????????????????????????,????????buffer????????????????
				sqlCommand.emplace_back(std::string_view(SQLCOMMAND::testTestLoginSQL, SQLCOMMAND::testTestLoginSQLLen));
				sqlCommand.emplace_back(usernameView);
				sqlCommand.emplace_back(std::string_view(STATICSTRING::doubleQuotation, STATICSTRING::doubleQuotationLen));

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



		//????????
		const char **BodyBuffer{ m_buffer->bodyPara() };

		if (!std::equal(*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password), *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1), passwordView.cbegin(), passwordView.cend()))
			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);


		//????????????????????????session
		//hmset  user  session:usernameView   session  sessionEX:usernameView   sessionExpire
		if (sessionView.empty() || sessionExpireView.empty())
		{
			//????????????????????
			//????session   ????24??????
			//????24??????session
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


			//????????set-cookie??http????????????

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

				//????????set-cookie??http????????????
					//????????????????????????????????
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
				m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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


		//????session????????
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
			//session??????????????????
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


			//  ????redis????session  ??  session????????
			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
				return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
			m_sessionLen = 24;

		}
		else
		{
			//????????????????

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




		//????????set-cookie??http????????????

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

			//????????set-cookie??http????????????
			//????????????????????????????????
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
			m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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

		if (rowField.empty() || result.empty() || (rowField.size() % 2))   //  ????????
			return startWrite(HTTPRESPONSEREADY::httpNoRecordInSql, HTTPRESPONSEREADY::httpNoRecordInSqlLen);

		const char **BodyBuffer{ m_buffer->bodyPara() };

		std::string_view CheckpasswordView{ *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password),*(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTLOGIN::password) },
			&RealpasswordView{ result[0] };

		if (!std::equal(CheckpasswordView.cbegin(), CheckpasswordView.cend(), RealpasswordView.cbegin(), RealpasswordView.cend()))
		{
			do
			{
				//????24??????session
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


				//  ????redis????session  ??  session????????
				if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
					break;

			} while (false);


			return startWrite(HTTPRESPONSEREADY::httpPasswordIsWrong, HTTPRESPONSEREADY::httpPasswordIsWrongLen);
		}


		//????24??????session
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


		//  ????redis????session  ??  session????????
		if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
			return startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);


		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		std::copy(m_randomString, m_randomString + 24, m_SessionID.get());
		m_sessionLen = 24;


		//????????set-cookie??http????????????

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

			//????????set-cookie??http????????????
				//????????????????????????????????
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
			m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
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
	startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
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




//  keyValue   {"key":"value"}
//  

void HTTPSERVICE::testMakeJson()
{
	if(!hasBody)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char *source{ &*m_buffer->getView().body().cbegin() };
	if (!praseBody(source, m_buffer->getView().body().size(), m_buffer->bodyPara(), STATICSTRING::jsonType, STATICSTRING::jsonTypeLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view jsonTypeView{ *(BodyBuffer + HTTPINTERFACENUM::TESTMAKEJSON::jsonType),*(BodyBuffer + HTTPINTERFACENUM::TESTMAKEJSON::jsonType + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTMAKEJSON::jsonType) };

	STLtreeFast &st1{ m_STLtreeFastVec[0] }, &st2{ m_STLtreeFastVec[1] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st2.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	static const char *key1Str{ "key1" }, *value1Str{ "value1" }, *key2Str{ "key2" }, *value2Str{ "value2" }, *value3Str{ "value3" }, *key3Str{ "key3" }, *nullStr{ "null" };

	static int key1StrLen{ strlen(key1Str) }, key2StrLen{ strlen(key2Str) }, key3StrLen{ strlen(key3Str) }, value1StrLen{ strlen(value1Str) }, value2StrLen{ strlen(value2Str) },
		value3StrLen{ strlen(value3Str) }, nullStrLen{ strlen(nullStr) };

	bool success{ false }, innerSuccess{false};

	char *newbuffer{};

	int needLen{};

	do
	{
		switch (jsonTypeView.size())
		{
			// {"key1":"value1"}
		case STATICSTRING::keyValueLen:
			if (!std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::keyValue, STATICSTRING::keyValue + STATICSTRING::keyValueLen))
				break;
			if (!st1.putString<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, value1Str, value1Str + value1StrLen))
				break;
			innerSuccess = true;
			break;

			//{"key1":"value1","key2":"value2"}
			//{"key1":{}}
			//{"key1":[{"key1":"value1","key2":"","key3":true},{"key1":"value1","key2":"","key3":true},{"key1":"value1","key2":"","key3":true}]}
		case STATICSTRING::doubleValueLen:
			if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::doubleValue, STATICSTRING::doubleValue + STATICSTRING::doubleValueLen))
			{
				if (!st1.putString<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, value1Str, value1Str + value1StrLen))
					break;
				if (!st1.putString<TRANSFORMTYPE>(key2Str, key2Str + key2StrLen, value2Str, value2Str + value2StrLen))
					break;
			}
			else if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::emptyObject, STATICSTRING::emptyObject + STATICSTRING::emptyObjectLen))
			{
				if (!st1.putObject<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, st2))
					break;
			}
			else if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::objectArray, STATICSTRING::objectArray + STATICSTRING::objectArrayLen))
			{
				if (!st1.putString<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, value1Str, value1Str + value1StrLen))
					break;
				if (!st1.putString<TRANSFORMTYPE>(key2Str, key2Str + key2StrLen, nullptr, nullptr))
					break;
				if (!st1.putBoolean<TRANSFORMTYPE>(key3Str, key3Str + key3StrLen, true))
					break;
				if (!st2.putObject<TRANSFORMTYPE>(nullptr, nullptr, st1))
					break;
				if (!st2.putObject<TRANSFORMTYPE>(nullptr, nullptr, st1))
					break;
				if (!st2.putObject<TRANSFORMTYPE>(nullptr, nullptr, st1))
					break;
				if (!st1.clear())
					break;
				if (!st1.putArray<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, st2))
					break;
			}
			else
				break;
			innerSuccess = true;
			break;

			//{"key1":""}
			//{"key1":{}}
			//{"key1":["value1","value2","value3"]}
		case STATICSTRING::emptyValueLen:
			if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::emptyValue, STATICSTRING::emptyValue + STATICSTRING::emptyValueLen))
			{
				if (!st1.putString<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, nullptr, nullptr))
					break;
			}
			else if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::emptyObject, STATICSTRING::emptyObject + STATICSTRING::emptyObjectLen))
			{
				if (!st1.putArray<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, st2))
					break;
			}
			else if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::valueArray, STATICSTRING::valueArray + STATICSTRING::valueArrayLen))
			{
				if (!st2.putString<TRANSFORMTYPE>(nullptr, nullptr, value1Str, value1Str+ value1StrLen))
					break;
				if (!st2.putString<TRANSFORMTYPE>(nullptr, nullptr, value2Str, value2Str + value2StrLen))
					break;
				if (!st2.putString<TRANSFORMTYPE>(nullptr, nullptr, value3Str, value3Str + value3StrLen))
					break;
				if (!st1.putArray<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, st2))
					break;
			}
			else
				break;
			innerSuccess = true;
			break;
			// {"key1":null}
		case STATICSTRING::jsonNullLen:
			if (!std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::jsonNull, STATICSTRING::jsonNull + STATICSTRING::jsonNullLen))
				break;
			if (!st1.putNull<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen))
				break;
			innerSuccess = true;
			break;

			//{"key1":true,"key2":false}
		case STATICSTRING::jsonBooleanLen:
			if (!std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::jsonBoolean, STATICSTRING::jsonBoolean + STATICSTRING::jsonBooleanLen))
				break;
			if (!st1.putBoolean<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, true))
				break;
			if (!st1.putBoolean<TRANSFORMTYPE>(key2Str, key2Str + key2StrLen, false))
				break;
			innerSuccess = true;
			break;

			//{"key1":30}
			//{"key1":{"key1":"value1","key2":"","key3":true}}
		case STATICSTRING::jsonNumberLen:
			if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::jsonNumber, STATICSTRING::jsonNumber + STATICSTRING::jsonNumberLen))
			{
				if (!st1.putNumber<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, static_cast<int>(30)))
					break;
				if (!st1.putNumber<TRANSFORMTYPE>(key2Str, key2Str + key2StrLen, static_cast<int>(-20)))
					break;
				if (!st1.putNumber<TRANSFORMTYPE>(key3Str, key3Str + key3StrLen, static_cast<float>(40.7856f)))
					break;
			}
			else if (std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::jsonObject, STATICSTRING::jsonObject + STATICSTRING::jsonObjectLen))
			{
				if (!st2.putString<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, value1Str, value1Str + value1StrLen))
					break;
				if (!st2.putString<TRANSFORMTYPE>(key2Str, key2Str + key2StrLen, nullptr, nullptr))
					break;
				if (!st2.putBoolean<TRANSFORMTYPE>(key3Str, key3Str + key3StrLen, true))
					break;
				if (!st1.putObject<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, st2))
					break;
			}
			else
				break;
			innerSuccess = true;
			break;

			//{"key1":["key1":"value1","key2":"","key3":true]}
		case STATICSTRING::jsonArrayLen:
			if (!std::equal(jsonTypeView.cbegin(), jsonTypeView.cend(), STATICSTRING::jsonArray, STATICSTRING::jsonArray + STATICSTRING::jsonArrayLen))
				break;
			if (!st2.putString<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, value1Str, value1Str + value1StrLen))
				break;
			if (!st2.putString<TRANSFORMTYPE>(key2Str, key2Str + key2StrLen, nullptr, nullptr))
				break;
			if (!st2.putBoolean<TRANSFORMTYPE>(key3Str, key3Str + key3StrLen, true))
				break;
			if (!st1.putArray<TRANSFORMTYPE>(key1Str, key1Str + key1StrLen, st2))
				break;
			innerSuccess = true;
			break;


		default:
			break;
		}

		if (!innerSuccess)
			break;

		success = true;
	} while (false);
	if (success)
		makeSendJson<TRANSFORMTYPE>(st1);
	else
		startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
}




void HTTPSERVICE::testCompareWorkFlow()
{
	if (!hasBody)
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	ReadBuffer &refBuffer{ *m_buffer };

	//????????body????????stringlen  ????????????????
	if (!praseBody(&*refBuffer.getView().body().cbegin(), refBuffer.getView().body().size(), refBuffer.bodyPara(), STATICSTRING::stringlen, STATICSTRING::stringlenLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char **BodyBuffer{ refBuffer.bodyPara() };

	std::string_view numberView{ *(BodyBuffer + HTTPINTERFACENUM::TESTCOMPAREWORKFLOW::stringLen),*(BodyBuffer + HTTPINTERFACENUM::TESTCOMPAREWORKFLOW::stringLen + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTCOMPAREWORKFLOW::stringLen) };

	//??stringlen????????????????????????
	unsigned int number{ 0 };
	if (!numberView.empty())
	{
		if (!std::all_of(numberView.cbegin(), numberView.cend(), std::bind(isdigit, std::placeholders::_1)))
			return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

		int index{ -1 }, num{ 1 };
		number = std::accumulate(std::make_reverse_iterator(numberView.cend()), std::make_reverse_iterator(numberView.cbegin()), 0, [&index, &num](auto &sum, const char ch)
		{
			if (++index)num *= 10;
			return sum += (ch - '0')*num;
		});
	}

	static unsigned int dateLen{ 29 };
	char *sendBuffer{};
	unsigned int sendLen{};

	static std::function<void(char*&)>compareFun{ [this](char *&bufferBegin)
	{
		if (!bufferBegin)
			return;

		std::copy(STATICSTRING::Date, STATICSTRING::Date + STATICSTRING::DateLen, bufferBegin);
		bufferBegin += STATICSTRING::DateLen;

		*bufferBegin++ = ':';

		//????Date??   
		m_sessionClock = std::chrono::system_clock::now();

		m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

		//????m_tmGMT????CST????
		m_tmGMT = localtime(&m_sessionTime);

		// Mon, 13 Sep 2021 11:07:54 CST

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

		std::copy(STATICSTRING::CST, STATICSTRING::CST + STATICSTRING::CSTLen, bufferBegin);
		bufferBegin += STATICSTRING::CSTLen;


	} };

	//29  ??????workflow??????????????????????????
	if (make_compareWithWorkFlowResPonse<CUSTOMTAG>(sendBuffer, sendLen, compareFun, STATICSTRING::DateLen + 1 + dateLen, m_finalVersionBegin, m_finalVersionEnd, MAKEJSON::http200,
		MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
		number,
		MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen, STATICSTRING::textplainUtf8, STATICSTRING::textplainUtf8+ STATICSTRING::textplainUtf8Len,
		MAKEJSON::Connection, MAKEJSON::Connection + MAKEJSON::ConnectionLen, STATICSTRING::Keep_Alive, STATICSTRING::Keep_Alive + STATICSTRING::Keep_AliveLen))
	{
		 startWrite(sendBuffer, sendLen);
	}
	else
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}
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


		//??????????????????????session
		if (sessionView.empty() || sessionExpireView.empty())
		{
			m_thisTime = 0, m_sessionLen = 0;

			///////////////////////////////////////
			//  ????????????????cookie??????????????????????????????

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


		//  ????????????????
		//  ??????????????????????????????????????????????????????


		//????session????????
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
			// ????redis??session??????????????????????session????
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


			// ????????????????session????

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
		// ????session????????

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


			//??????????????????????session
			if (sessionView.empty() || sessionExpireView.empty())
			{
				m_thisTime = 0, m_sessionLen = 0;

				///////////////////////////////////////
				//  ????????????????cookie??????????????????????????????

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


			//  ????????????????
			//  ??????????????????????????????????????????????????????


			//????session????????
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
				// ????redis??session??????????????????????session????
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


				// ????????????????session????

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
			// ????session????????

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










//????????????????
/*

????????????????????


# id, name, age, province, city, country, phone
'1', 'dengdanjun', '9', 'GuangDong', 'GuangZhou', 'TianHe', '13528223629'  char*


????????????????????????????????????????????????????????????buffer??????????????????????????????????????????


idBegin,idEnd, nameBegin,nameEnd, ageBegin,ageEnd, provinceBegin,provinceEnd, cityBegin,cityEnd, countryBegin,countryEnd,
, phoneBegin,phoneEnd


char**

??????????????????????????????????????????????????????????????????10??????????10??????????


??????????????????????10????????????????????char**????????????????????????????????


char***      ????????????????????????????????????????age??????????????????????????????????


char**+4????char**+4+7*2??   ????????



??????????????????????????

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
					});     //????????????????????char*??????????????

				}
			});

????????????????????????????????????4??????????14??????????????????????????????



*/


void HTTPSERVICE::handleERRORMESSAGE(ERRORMESSAGE em)
{
	switch (em)
	{
	case ERRORMESSAGE::OK:
		startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
		break;

	case ERRORMESSAGE::SQL_QUERY_ERROR:
		startWrite(HTTPRESPONSEREADY::http11SqlQueryError, HTTPRESPONSEREADY::http11SqlQueryErrorLen);
		break;

	case ERRORMESSAGE::SQL_NET_ASYNC_ERROR:
		startWrite(HTTPRESPONSEREADY::http11SqlNetAsyncError, HTTPRESPONSEREADY::http11SqlNetAsyncErrorLen);
		break;

	case ERRORMESSAGE::SQL_MYSQL_RES_NULL:
		startWrite(HTTPRESPONSEREADY::http11SqlResNull, HTTPRESPONSEREADY::http11SqlResNullLen);
		break;

	case ERRORMESSAGE::SQL_QUERY_ROW_ZERO:
		startWrite(HTTPRESPONSEREADY::http11SqlQueryRowZero, HTTPRESPONSEREADY::http11SqlQueryRowZeroLen);
		break;

	case ERRORMESSAGE::SQL_QUERY_FIELD_ZERO:
		startWrite(HTTPRESPONSEREADY::http11SqlQueryFieldZero, HTTPRESPONSEREADY::http11SqlQueryFieldZeroLen);
		break;

	case ERRORMESSAGE::SQL_RESULT_TOO_LAGGE:
		startWrite(HTTPRESPONSEREADY::http11sqlSizeTooBig, HTTPRESPONSEREADY::http11sqlSizeTooBigLen);
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

	m_readBuffer = m_buffer->getBuffer();
	m_maxReadLen = m_defaultReadLen;
	std::fill(m_httpHeaderMap.get(), m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN, nullptr);
	m_charMemoryPool.reset();
	m_charPointerMemoryPool.reset();
	(*m_clearFunction)(m_mySelf);
}



void HTTPSERVICE::recoverMemory()
{
	std::fill(m_httpHeaderMap.get(), m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN, nullptr);
	m_charMemoryPool.prepare();
	m_charPointerMemoryPool.prepare();
}


void HTTPSERVICE::sendOK()
{
	startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
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
		m_STLtreeFastVec.emplace_back(STLtreeFast(&m_charPointerMemoryPool, &m_charMemoryPool));
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



//????????????????????????????????
//m_startPos ??????????????????????????????????????
void HTTPSERVICE::parseReadData(const char *source, const int size)
{
	ReadBuffer &refBuffer{ *m_buffer };

	try
	{
		switch ((m_parseStatus = parseHttp(source, size)))
		{
		case PARSERESULT::complete:
			m_startPos = 0;
			m_readBuffer = refBuffer.getBuffer();
			m_maxReadLen = m_defaultReadLen;
			//https://www.cnblogs.com/wmShareBlog/p/5924144.html
			//????Expect:100-continue ????			
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
			m_availableLen = refBuffer.getSock()->available();
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
	catch (const std::exception &e)
	{
		m_parseStatus = PARSERESULT::complete;
		m_startPos = 0;
		m_readBuffer = refBuffer.getBuffer();
		m_maxReadLen = m_defaultReadLen;
		m_availableLen = refBuffer.getSock()->available();
		m_sendBuffer = HTTPRESPONSEREADY::httpSTDException, m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
		cleanData();
	}
}




//????http????????????????????????append????????
//??????????????????????????????????????????????????????????????????????????????????????????????????????
//??????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
//????POST  GET
// ????????????
// ????????????body  chunk??????multipartFromdata????????????????
//????pipeline??????????????????????????????????????????????????  ??????startWriteLoop??????????
//??????????????????target??Query para????body????url??????????????????????????????????UrlDecodeWithTransChinese????????????????????

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
#define KEEPALIVELEN 10

	static std::string HTTPStr{ "HTTP" };
	static std::string http11{ "1.0" }, HTTP11{ "1.1" };
	static std::string lineStr{ "\r\n\r\n" };
	static std::string PUTStr{ "PUT" }, GETStr{ "GET" };                                         // 3                
	static std::string POSTStr{ "POST" }, HEADStr{ "HEAD" };                    // 4                     
	static std::string TRACEStr{ "TRACE" };                                     // 5
	static std::string DELETEStr{ "DELETE " };                                  // 6
	static std::string CONNECTStr{ "CONNECT" }, OPTIONSStr{ "OPTIONS" };        // 7
	//??????????????????????????????????????????????????????????????

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
	static std::string Expect100_continue{ "100-continue" };
	static std::string ConnectionKeep_alive{ "Keep-Alive" };
	static std::string ConnectionClose{ "close" };


	static std::string Content_Disposition{ "content-disposition" };
	static std::string Name{ "name" };
	static std::string Filename{ "filename" };
	static std::string boundaryFinal{ "--\r\n" };







	///////////////////////////////////////////////////////////////////////////////////////
	int len{}, num{}, index{};                   //????body????
	const char **headerPos{ m_httpHeaderMap.get() };
	char *newBuffer{}, *newBufferBegin{}, *newBufferEnd{}, *newBufferIter{};
	bool isDoubleQuotation{};   //????boundaryWord????????????\"

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

	//??????????????????????????????????
	LOG &reflog{ *m_log };
	ReadBuffer &refBuffer{ *m_buffer };
	MYREQVIEW &refMyReView{ refBuffer.getView() };


	switch (m_parseStatus)
	{
	case PARSERESULT::check_messageComplete:
	case PARSERESULT::invaild:
	case PARSERESULT::complete:
		refMyReView.clear();
		hasBody = hasChunk = hasPara = expect_continue = keep_alive = false;
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


	case PARSERESULT::check_fileBoundaryEnd2:
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funBegin !isupper");
			return PARSERESULT::invaild;
		}

		funBegin = iterFindBegin;

		iterFindBegin = funBegin + 1, accumulateLen = 0;

	find_funEnd:
		// httpService.cpp    
		//  accumulateLen????????method??????????????????
		// ????????????????iterFindBegin????????????????????iterFindEnd??????????method??????????????MAXMETHODLEN
		// ????????????????
		// ?????????? ???? ???????????? ????????????????????iterFindThisEnd
		//  ??????????????????????????????????????????????????????

		// ??????????????????????????iterFindEnd??????iterFindThisEnd
		// ????????????????????????????????????????????????????accumulateLen????
		// ????????????????????????????????????buffer????????????
		// ??????????????????????????iterFindEnd??????

		// ????????????????buffer??????????????
		// ????????????????????vector  dataBufferVec??????
		// ??????funBegin, iterFindEnd????dataBufferVec??
		// ??????????????buffer????????
		// ??????????????????????buffer??????????buffer????????????


		// ??????funEnd????????dataBufferVec??????????
		// ??????????????????????????????????????
		// ??????????????????????buffer  copy  ??????????????????????
		// ????????????????????????????
		// ????????????????????buffer????????????????????????????????????????????????????????????????????????????????????????????copy??
		// ????????????????????

		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXMETHODLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXMETHODLEN - accumulateLen);

			funEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_not<>(), std::bind(isupper, std::placeholders::_1)));

			if (funEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd funEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !newBuffer");
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd *funEnd != ' '");
			return PARSERESULT::invaild;
		}


		if (!dataBufferVec.empty())
		{
			//????dataBufferVec??????????????????????????????????????????????????std::distance(m_readBuffer, const_cast<char*>(funEnd)) ????????????????
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(funEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal PUTStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::PUT;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::THREE);
				break;
			case 'G':
				if (!std::equal(finalFunBegin, finalFunEnd, GETStr.cbegin(), GETStr.cend()))
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal GETStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::GET;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::THREE);
				break;
			default:
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd THREE default");
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal POSTStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::POST;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::FOUR);
				break;
			case 'H':
				if (!std::equal(finalFunBegin, finalFunEnd, HEADStr.cbegin(), HEADStr.cend()))
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal HEADStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::HEAD;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::FOUR);
				break;
			default:
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd FOUR default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		case HTTPHEADER::FIVE:
			if (!std::equal(finalFunBegin, finalFunEnd, TRACEStr.cbegin(), TRACEStr.cend()))
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal TRACEStr");
				return PARSERESULT::invaild;
			}
			refMyReView.method() = METHOD::TRACE;
			refMyReView.setMethod(finalFunBegin, HTTPHEADER::FIVE);
			break;
		case HTTPHEADER::SIX:
			if (!std::equal(finalFunBegin, finalFunEnd, DELETEStr.cbegin(), DELETEStr.cend()))
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal DELETEStr");
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal CONNECTStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::CONNECT;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::SEVEN);
				break;
			case 'O':
				if (!std::equal(finalFunBegin, finalFunEnd, OPTIONSStr.cbegin(), OPTIONSStr.cend()))
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd !equal OPTIONSStr");
					return PARSERESULT::invaild;
				}
				refMyReView.method() = METHOD::OPTIONS;
				refMyReView.setMethod(finalFunBegin, HTTPHEADER::SEVEN);
				break;
			default:
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd SEVEN default");
				return PARSERESULT::invaild;
				break;
			}
			break;
		default:
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_funEnd default");
			return PARSERESULT::invaild;
			break;
		}


		///////////////////////////////////  ????  ??????????????
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_targetBegin;
			}
		}

		if (*iterFindBegin == '\r')
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetBegin *iterFindBegin == '\r'");
			return PARSERESULT::invaild;
		}


		targetBegin = iterFindBegin;
		iterFindBegin = targetBegin + 1;
		accumulateLen = 0;


		///////////////////////////////////////////////////////////////  begin????????????begin??end?????????? ////////////////////////////////////////////////////

	find_targetEnd:
		len = std::distance(iterFindBegin, iterFindEnd);
		if (accumulateLen + len >= MAXTARGETLEN)
		{
			iterFindThisEnd = iterFindBegin + (MAXTARGETLEN - accumulateLen);

			targetEnd = std::find_if(iterFindBegin, iterFindThisEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, ' '),
				std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, '?'), std::bind(std::equal_to<>(), std::placeholders::_1, '\r'))));

			if (targetEnd == iterFindThisEnd)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd targetEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd !newBuffer");
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd *targetEnd == '\r'");
			return PARSERESULT::invaild;
		}

		//????????????????????????????????????????????
		if (!dataBufferVec.empty())
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(targetEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_targetEnd !newBuffer");
				return PARSERESULT::invaild;
			}

			newBufferIter = newBuffer;

			for (auto const &singlePair : dataBufferVec)
			{
				std::copy(singlePair.first, singlePair.second, newBufferIter);
				newBufferIter += std::distance(singlePair.first, singlePair.second);
			}
			std::copy(m_readBuffer, const_cast<char*>(targetEnd), newBufferIter);

			//??target????url????????????????????????????????charset=UTF-8??????
			if (!UrlDecodeWithTransChinese(newBuffer, accumulateLen, len))
				return PARSERESULT::invaild;

			finalTargetBegin = newBuffer, finalTargetEnd = newBuffer + len;

			dataBufferVec.clear();
		}
		else
		{
			//??target????url????????????????????????????????charset=UTF-8??????
			if (!UrlDecodeWithTransChinese(targetBegin, targetEnd - targetBegin, len))
				return PARSERESULT::invaild;

			finalTargetBegin = targetBegin, finalTargetEnd = targetBegin + len;
		}


		refMyReView.setTarget(finalTargetBegin, finalTargetEnd - finalTargetBegin);
		accumulateLen = 0;


		//////////////////////////////////////////////////////////////////   target????????????  ////////////////////////////////////////////

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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraBegin !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_targetBegin;
				}
			}

			if (*iterFindBegin == '\r')
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraBegin *iterFindBegin == '\r'");
				return PARSERESULT::invaild;
			}


			////////////////////////////////////////////////////////////   bodyBegin????????  /////////////////////////////////////////
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd paraEnd == iterFindThisEnd");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd !newBuffer");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd *paraEnd == '\r'");
				return PARSERESULT::invaild;
			}

			//????????????????????????????????????????????
			if (!dataBufferVec.empty())
			{
				accumulateLen += std::distance(m_readBuffer, const_cast<char*>(paraEnd));

				newBuffer = m_charMemoryPool.getMemory(accumulateLen);

				if (!newBuffer)
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_paraEnd !newBuffer");
					return PARSERESULT::invaild;
				}

				newBufferIter = newBuffer;

				for (auto const &singlePair : dataBufferVec)
				{
					std::copy(singlePair.first, singlePair.second, newBufferIter);
					newBufferIter += std::distance(singlePair.first, singlePair.second);
				}
				std::copy(m_readBuffer, const_cast<char*>(paraEnd), newBufferIter);

				//??query Para????url????????????????????????????????charset=UTF-8??????
				if (!UrlDecodeWithTransChinese(newBuffer, accumulateLen, len))
					return PARSERESULT::invaild;

				finalParaBegin = newBuffer, finalParaEnd = newBuffer + len;

				dataBufferVec.clear();
			}
			else
			{
				//??query Para????url????????????????????????????????charset=UTF-8??????
				if (!UrlDecodeWithTransChinese(paraBegin, paraEnd - paraBegin, len))
					return PARSERESULT::invaild;

				finalParaBegin = paraBegin, finalParaEnd = paraBegin + len;
			}


			refMyReView.setPara(finalParaBegin, finalParaEnd - finalParaBegin);
			hasPara = true;
		}

		///////////////////////////////////////////////////////////////////////////////   firstBody??????  /////////////////////////////////

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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_httpBegin;
			}
		}

		if (*iterFindBegin != 'H')
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpBegin *iterFindBegin != 'H'");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd httpEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd !newBuffer");
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd *httpEnd != '/'");
			return PARSERESULT::invaild;
		}

		//????????????????????????????????????????????
		if (!dataBufferVec.empty())
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(httpEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_httpEnd !equal HTTPStr");
			return PARSERESULT::invaild;
		}

		iterFindBegin = httpEnd + 1;
		accumulateLen = 0;

		///////////////////////////////////////////////////////////////////////////////////////////////////////??????

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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionBegin !newBuffer");
					return PARSERESULT::invaild;
				}

				m_readBuffer = newBuffer;
				m_startPos = 0;
				return PARSERESULT::find_versionBegin;
			}
		}


		if (*iterFindBegin != '1' && *iterFindBegin != '2')
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionBegin *iterFindBegin != '1' && *iterFindBegin != '2'");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd versionEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd !newBuffer");
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd *versionEnd != '\r'");
			return PARSERESULT::invaild;
		}

		//????????????????????????????????????????????
		if (!dataBufferVec.empty())
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(versionEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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


		if (!std::equal(finalVersionBegin, finalVersionEnd, http11.cbegin(), http11.cend()) &&
			!std::equal(finalVersionBegin, finalVersionEnd, HTTP11.cbegin(), HTTP11.cend()))
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_versionEnd !equal http11 HTTP11");
			return PARSERESULT::invaild;
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd lineEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd !newBuffer");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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


		len = distance(finalLineBegin, finalLineEnd);
		
		accumulateLen = 0;

		break;
	}

	while (len == 2)
	{
		if (!std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cbegin() + 2))
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_lineEnd len ==2 !equal lineStr");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd headEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd !newBuffer");
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd *headEnd == '\r'");
			return PARSERESULT::invaild;
		}

		if (!dataBufferVec.empty())
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(headEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_headEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordBegin wordBegin == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordBegin !newBuffer");
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
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordBegin *wordBegin == '\r'");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordEnd wordEnd == iterFindThisEnd");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_wordEnd;
				}
			}
		}


		//????????????????????????????????????????????
		if (!dataBufferVec.empty())
		{
			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(wordEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_wordEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_TEBegin is not nullptr");
					return PARSERESULT::invaild;
				}
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
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_ViaBegin is not nullptr");
					return PARSERESULT::invaild;
				}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_DateBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_FromBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_HostBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_RangeBegin is not nullptr");
					return PARSERESULT::invaild;
				}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_AcceptBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_CookieBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_ExpectBegin is not nullptr");
						return PARSERESULT::invaild;
					}
					//  https://www.cnblogs.com/yesok/p/12658499.html
					//??????except=100-continue????????
					if (std::equal(finalWordBegin, finalWordEnd, Expect100_continue.cbegin(), Expect100_continue.cend()))
						expect_continue = true;
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_PragmaBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_RefererBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_UpgradeBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_WarningBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_If_MatchBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_If_RangeBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_ConnectionBegin is not nullptr");
						return PARSERESULT::invaild;
					}
					if (std::equal(finalWordBegin, finalWordEnd, ConnectionKeep_alive.cbegin(), ConnectionKeep_alive.cend()))
						keep_alive = true;

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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_ConnectionBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Content_TypeBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Max_ForwardsBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Accept_RangesBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_AuthorizationBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Cache_ControlBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_If_None_MatchBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_If_None_MatchBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Content_LengthBegin is not nullptr");
						return PARSERESULT::invaild;
					}

					if (!std::distance(finalWordBegin, finalWordEnd))
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "Content_LengthBegin  !std::distance");
						return PARSERESULT::invaild;
					}


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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Accept_EncodingBegin  is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Accept_LanguageBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_If_Modified_SinceBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Transfer_EncodingBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_If_Unmodified_SinceBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Proxy_AuthorizationBegin is not nullptr");
						return PARSERESULT::invaild;
					}
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
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd lineEnd == iterFindThisEnd");
				return PARSERESULT::invaild;
			}

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
							finalBodyBegin = finalBodyEnd = m_messageBegin = lineEnd;
							refMyReView.setBody(finalBodyBegin, 0);
							goto check_messageComplete;
						}
					}
					else
					{
						if (std::distance(dataBufferVec.front().first, dataBufferVec.front().second) + std::distance(m_readBuffer, const_cast<char*>(lineEnd)) == 4)
						{
							finalBodyBegin = finalBodyEnd = m_messageBegin = lineEnd;
							refMyReView.setBody(finalBodyBegin, 0);
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd !newBuffer");
						return PARSERESULT::invaild;
					}

					m_readBuffer = newBuffer;
					m_startPos = 0;
					return PARSERESULT::find_secondLineEnd;
				}
			}
		}


		//????????????????????????????????????????????
		if (!dataBufferVec.empty())
		{

			accumulateLen += std::distance(m_readBuffer, const_cast<char*>(lineEnd));

			newBuffer = m_charMemoryPool.getMemory(accumulateLen);

			if (!newBuffer)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd !newBuffer");
				return PARSERESULT::invaild;
			}

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
	}

	if (len != 4)
	{
		reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_secondLineEnd len != 2 && len != 4");
		return PARSERESULT::invaild;
	}


	//??content-lengeh??????????????????????MultiPartFormData????????????????????????????body????
	//??????????????????chunk
	if (len == 4)
	{
		if (!std::equal(finalLineBegin, finalLineEnd, lineStr.cbegin(), lineStr.cend()))
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "len == 4 !std::equal lineStr");
			return PARSERESULT::invaild;
		}


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
				//??body????url????????????????????????????????charset=UTF-8??????  ??????postman???????? pingpong??????????   /2??pingpong????????
				if (!UrlDecodeWithTransChinese(iterFindBegin, m_bodyLen, len))
					return PARSERESULT::invaild;

				finalBodyBegin = iterFindBegin, finalBodyEnd = iterFindBegin + len;
				refMyReView.setBody(finalBodyBegin, len);
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
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_finalBodyEnd !newReadBuffer");
						return PARSERESULT::invaild;
					}

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
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "begin_checkChunkData !m_readBuffer");
				return PARSERESULT::invaild;
			}

			///////////////////////////////////////////////////////

			//  https://blog.csdn.net/u014558668/article/details/70141956
			//chunk??????????????
			//[chunk size][\r\n][chunk data][\r\n][chunk size][\r\n][chunk data][\r\n][chunk size = 0][\r\n][\r\n]
			while (true)
			{
				m_chunkLen = 0;

			find_fistCharacter:
				if (iterFindBegin == iterFindEnd)
					return PARSERESULT::find_fistCharacter;


				if (!std::isalnum(*iterFindBegin))
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_fistCharacter !std::isalnum");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_chunkNumEnd std::find_if_not");
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_chunkNumEnd *chunkNumEnd != '\r'");
					return PARSERESULT::invaild;
				}


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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_thirdLineEnd !std::equal lineStr");
					return PARSERESULT::invaild;
				}

				if (!m_chunkLen)
				{
					m_messageBegin = chunkNumEnd + len;
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
					//  chunkDataBegin, iterFindEnd  ??????????????????????
					m_chunkLen -= len;
					m_startPos = 0;
					chunkDataBegin = m_readBuffer;
					return PARSERESULT::find_chunkDataEnd;
				}

				//  chunkDataBegin, chunkDataEnd  ??????????????????????
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_fourthLineEnd !std::equal lineStr");
					return PARSERESULT::invaild;
				}


				iterFindBegin = chunkDataEnd + 2;

			}



		}
		m_messageBegin = finalLineEnd;
		goto check_messageComplete;
	}
	else
	{
	reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "len !=4");
		return PARSERESULT::invaild;
	}


	
	// ????????????  multipart/form-data
	// ????????boundary????????   --????boundary????
	// ???????? \r\n--????boundary????
	begin_checkBoundaryBegin:
		m_readBuffer = m_charMemoryPool.getMemory(m_maxReadLen);

		if (!m_readBuffer)
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "begin_checkBoundaryBegin !m_readBuffer");
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
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryBegin m_bodyLen <= 0");
				return PARSERESULT::invaild;
			}
			findBoundaryBegin = m_readBuffer;
			return PARSERESULT::find_boundaryBegin;
		}

		if (*findBoundaryBegin != *m_boundaryBegin)
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryBegin *findBoundaryBegin != *m_boundaryBegin");
			return PARSERESULT::invaild;
		}


	find_halfBoundaryBegin:
		if (m_bodyLen <= m_boundaryLen)
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_halfBoundaryBegin m_bodyLen <= m_boundaryLen");
			return PARSERESULT::invaild;
		}

		len = std::distance(findBoundaryBegin, iterFindEnd);
		if (len < m_boundaryLen)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin;
			if (m_bodyLen <= 0)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_halfBoundaryBegin m_bodyLen <= 0");
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


		if (!std::equal(findBoundaryBegin, findBoundaryBegin + m_boundaryLen, m_boundaryBegin, m_boundaryEnd))
		{
			reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_halfBoundaryBegin !std::equal m_boundaryBegin");
			return PARSERESULT::invaild;
		}

		//????????

		findBoundaryBegin += m_boundaryLen;

		m_boundaryLen += 2;
		m_boundaryBegin -= 2;


	find_boundaryHeaderBegin:
		len = std::distance(findBoundaryBegin, iterFindEnd);
		if (len < 2)
		{
			m_bodyLen -= iterFindEnd - iterFindBegin ;
			if (m_bodyLen <= 0)
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderBegin m_bodyLen <= 0");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderBegin2 m_bodyLen <= 0");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderEnd >= MAXBOUNDARYHEADERNLEN");
						return PARSERESULT::invaild;
					}
					else
					{
						m_bodyLen -= iterFindEnd - iterFindBegin;
						if (m_bodyLen <= 0)
						{
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryHeaderEnd m_bodyLen <= 0");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Boundary_NameBegin is not nullptr");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Boundary_FilenameBegin is not nullptr");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Boundary_ContentTypeBegin is not nullptr");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "m_Boundary_ContentDispositionBegin is not nullptr");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordBegin m_bodyLen <= 0");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordBegin2 m_bodyLen <= 0");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd len == m_maxReadLen");
							return PARSERESULT::invaild;
						}
						m_bodyLen -= iterFindEnd - iterFindBegin;
						if (m_bodyLen <= 0)
						{
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd m_bodyLen <= 0");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 m_bodyLen <= 0");
							return PARSERESULT::invaild;
						}
						// ??????????????????thisBoundaryWordBegin????????
						// ??????????????thisBoundaryWordBegin??????????????thisBoundaryWordEnd????????????????????????????????????????????thisBoundaryWordBegin??thisBoundaryWordEnd??????
						if (thisBoundaryWordBegin != m_readBuffer)
						{
							std::copy(thisBoundaryWordBegin, boundaryWordEnd, m_readBuffer);
							thisBoundaryWordEnd = m_readBuffer + std::distance(thisBoundaryWordBegin, thisBoundaryWordEnd);
							boundaryWordEnd= m_readBuffer + std::distance(thisBoundaryWordBegin, boundaryWordEnd);
							thisBoundaryWordBegin = m_readBuffer;
						}
						m_startPos = len;
						return PARSERESULT::find_boundaryWordEnd2;
					}
					else
					{
						if (len == m_maxReadLen)
						{
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 len == m_maxReadLen");
							return PARSERESULT::invaild;
						}
						m_bodyLen -= iterFindEnd - iterFindBegin;
						if (m_bodyLen <= 0)
						{
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 m_bodyLen <= 0 2");
							return PARSERESULT::invaild;
						}
						if (thisBoundaryWordBegin != m_readBuffer)
						{
							std::copy(thisBoundaryWordBegin, iterFindEnd, m_readBuffer);
							boundaryWordEnd = m_readBuffer + std::distance(thisBoundaryWordBegin, boundaryWordEnd);
							thisBoundaryWordBegin = m_readBuffer;
						}
						// ????????????????????thisBoundaryWordBegin????????????
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_boundaryWordEnd2 !newBuffer");
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
							reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_nextboundaryHeaderBegin m_bodyLen <= 0");
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
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_nextboundaryHeaderBegin3 m_bodyLen <= 0");
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
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_fileBegin m_bodyLen <= 0");
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
				if ((findBoundaryBegin = std::find(findBoundaryBegin, iterFindEnd, *m_boundaryBegin)) == iterFindEnd)
				{
					//boundaryHeaderBegin ?? findBoundaryBegin ??????????


					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_fileBoundaryBegin m_bodyLen <= 0 1");
						return PARSERESULT::invaild;
					}
					m_startPos = 0; 
					findBoundaryBegin = m_readBuffer;
					return PARSERESULT::find_fileBoundaryBegin;
				}

			find_fileBoundaryBegin2:
				if (std::distance(findBoundaryBegin, iterFindEnd) < m_boundaryLen)
				{
					//boundaryHeaderBegin ?? findBoundaryBegin  ??????????

					len = std::distance(findBoundaryBegin, iterFindEnd);
					m_bodyLen -= iterFindEnd - iterFindBegin;
					if (m_bodyLen <= 0)
					{
						reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "find_fileBoundaryBegin m_bodyLen <= 0 2");
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

				if (std::equal(findBoundaryBegin, findBoundaryBegin + m_boundaryLen, m_boundaryBegin, m_boundaryEnd))
				{
					//boundaryHeaderBegin ?? findBoundaryBegin ??????????

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

			//????

		check_fileBoundaryEnd:
			if (findBoundaryBegin == iterFindEnd)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "check_fileBoundaryEnd m_bodyLen <= 0");
					return PARSERESULT::invaild;
				}
				m_startPos = 0;
				findBoundaryBegin = m_readBuffer;
				return PARSERESULT::check_fileBoundaryEnd;
			}

			if (*findBoundaryBegin != '-' && *findBoundaryBegin != '\r')
			{
				reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "check_fileBoundaryEnd *findBoundaryBegin != '-' && *findBoundaryBegin != '\r'");
				return PARSERESULT::invaild;
			}


		check_fileBoundaryEnd2:
			if ((len = std::distance(findBoundaryBegin, iterFindEnd)) < 4)
			{
				m_bodyLen -= iterFindEnd - iterFindBegin;
				if (m_bodyLen <= 0)
				{
					reflog.writeLog(__FILE__, __LINE__, m_message, m_parseStatus, "check_fileBoundaryEnd2 m_bodyLen <= 0");
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
				m_messageBegin = findBoundaryBegin + 4;
				goto check_messageComplete;
			}

			findBoundaryBegin += 2;
		}


		// ??????????????????
	check_messageComplete:
		if (m_messageBegin == iterFindEnd)
			return PARSERESULT::complete;
		m_messageEnd = iterFindEnd;
		return PARSERESULT::check_messageComplete;
}




















































