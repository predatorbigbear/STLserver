#include "httpService.h"





HTTPSERVICE::HTTPSERVICE(std::shared_ptr<io_context> ioc, std::shared_ptr<ASYNCLOG> log, const std::string &doc_root,
	std::shared_ptr<MULTISQLREADSW>multiSqlReadSWMaster,
	std::shared_ptr<MULTIREDISREAD>multiRedisReadMaster,
	std::shared_ptr<MULTIREDISWRITE>multiRedisWriteMaster, std::shared_ptr<MULTISQLWRITESW>multiSqlWriteSWMaster,
	std::shared_ptr<STLTimeWheel> timeWheel,
	const std::shared_ptr<std::unordered_map<std::string_view, std::string>>fileMap,
	const unsigned int timeOut, bool & success, const unsigned int serviceNum, const unsigned int bufNum
	)
	:m_ioc(ioc), m_log(log), m_doc_root(doc_root),
	m_multiSqlReadSWMaster(multiSqlReadSWMaster),m_fileMap(fileMap),
	m_multiRedisReadMaster(multiRedisReadMaster),m_multiRedisWriteMaster(multiRedisWriteMaster),m_multiSqlWriteSWMaster(multiSqlWriteSWMaster),
	m_timeOut(timeOut), m_timeWheel(timeWheel), m_maxReadLen(bufNum), m_defaultReadLen(bufNum), m_serviceNum(serviceNum)
{
	try
	{
		if (!ioc)
			throw std::runtime_error("io_context is nullptr");
		if (!log)
			throw std::runtime_error("log is nullptr");
		if(!timeOut)
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
		if(!m_timeWheel)
			throw std::runtime_error("timeWheel is nullptr");
		if(!bufNum)
			throw std::runtime_error("bufNum is 0");
		if(!serviceNum)
			throw std::runtime_error("serviceNum is 0");

		m_buffer.reset(new ReadBuffer(m_ioc, bufNum));

		m_verifyData.reset(new const char*[VerifyDataPos::maxBufferSize]);

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
	}
}




void HTTPSERVICE::setReady(const int index, std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>)>> clearFunction , std::shared_ptr<HTTPSERVICE> other)
{
	//m_buffer->getSock()->set_option(boost::asio::ip::tcp::no_delay(true), m_err);
	m_buffer->getSock()->set_option(boost::asio::socket_base::keep_alive(true), ec);     https://www.cnblogs.com/xiao-tao/p/9718017.html
	m_index = index;
	m_clearFunction = clearFunction;
	m_mySelf = other;
	m_hasClean.store(false);

	m_log->writeLog(__FUNCTION__, __LINE__, m_serviceNum);

	run();
}




std::shared_ptr<HTTPSERVICE> *HTTPSERVICE::getListIter()
{
	// TODO: 在此处插入 return 语句
	return mySelfIter.load();
}

void HTTPSERVICE::setListIter(std::shared_ptr<HTTPSERVICE>* iter)
{
	if (iter)
		mySelfIter.store(iter);
}




bool HTTPSERVICE::checkTimeOut()
{
	if (!m_hasRecv.load())
	{
		return true;
	}
	else
	{
		m_hasRecv.store(false);
		return false;
	}
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
		testGet();
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

	switch (std::accumulate(std::make_reverse_iterator(refBuffer.getView().target().cend()), std::make_reverse_iterator(refBuffer.getView().target().cbegin() + 1), 0, [&index, &num](auto& sum, auto const ch)
	{
		if (++index)num *= 10;
		return sum += (ch - '0') * num;
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


	case INTERFACE::testMultiRedisParseBodyReadLOT_SIZE_STRING:
		testMultiRedisParseBodyReadLOT_SIZE_STRING();
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


	case INTERFACE::testFirstTime:
		testFirstTime();
		break;



	case INTERFACE::testMultiPartFormData:
		testMultiPartFormData();
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


	case INTERFACE::testmultiSqlReadParseBosySW:
		testmultiSqlReadParseBosySW();
		break;


	case INTERFACE::testmultiSqlReadUpdateSW:
		testmultiSqlReadUpdateSW();
		break;


	case INTERFACE::testInsertHttpHeader:
		testInsertHttpHeader();
		break;



		//默认，不匹配任何接口情况
	default:
		startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);
		break;
	}
}





void HTTPSERVICE::testBody()
{
	if (m_httpresult.isBodyEmpty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::test, STATICSTRING::testLen, STATICSTRING::parameter, STATICSTRING::parameterLen, STATICSTRING::number, STATICSTRING::numberLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char** BodyBuffer{ m_buffer->bodyPara() };

	std::string_view testView{ *(BodyBuffer),*(BodyBuffer + 1) - *(BodyBuffer) };
	std::string_view parameterView{ *(BodyBuffer+2),*(BodyBuffer + 3) - *(BodyBuffer+2) };
	std::string_view numberView{ *(BodyBuffer + 4),*(BodyBuffer + 5) - *(BodyBuffer + 4) };

	static const std::string_view test{ "test" }, parameter{ "parameter" }, number{ "number" };

	STLtreeFast& st1{ m_STLtreeFastVec[0] };

	st1.reset();

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	//如果确定不需要开启json转码处理，可以不带<TRANSFORMTYPE>即可，默认不开启json转码处理

	//启用json转码处理
	if (!st1.put<TRANSFORMTYPE>(test.cbegin(), test.cend(), testView.cbegin(), testView.cend()))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put<TRANSFORMTYPE>(parameter.cbegin(), parameter.cend(), parameterView.cbegin(), parameterView.cend()))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put<TRANSFORMTYPE>(number.cbegin(), number.cend(), numberView.cbegin(), numberView.cend()))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	//启用json转码处理
	makeSendJson<TRANSFORMTYPE>(st1);
	
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
	if (!m_httpresult.isparaEmpty())
	{
		std::string_view para{ m_httpresult.getpara() };
		if (!UrlDecodeWithTransChinese(para.cbegin(), para.size(), m_len))
			return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

		m_buffer->setBodyLen(m_len);

		if (randomParseBody<HTTPINTERFACENUM::TESTRANDOMBODY, HTTPINTERFACENUM::TESTRANDOMBODY::max>(para.cbegin(), m_buffer->getBodyLen(), m_buffer->bodyPara(), m_buffer->getBodyParaLen()))
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



	else if(!m_httpresult.isBodyEmpty())
	{
		std::string_view bodyView{ m_httpresult.getBody() };
		if (!UrlDecodeWithTransChinese(bodyView.cbegin(), bodyView.size(), m_len))
			return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

		m_buffer->setBodyLen(m_len);

		if (randomParseBody<HTTPINTERFACENUM::TESTRANDOMBODY, HTTPINTERFACENUM::TESTRANDOMBODY::max>(bodyView.cbegin(), m_buffer->getBodyLen(), m_buffer->bodyPara(), m_buffer->getBodyParaLen()))
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
		//   没有Query Params ，也没有body，表明这个接口如果有参数的话，则参数全部为空的情况，自己做处理


		startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
	}
}


//读取文件接口
//读取存在文件  wrk -t1 -c100 -d60  http://127.0.0.1:8085/webfile
//读取不存在文件  wrk -t1 -c100 -d60  http://127.0.0.1:8085/webfile1
void HTTPSERVICE::testGet()
{
	if (m_buffer->getView().target().empty() || m_buffer->getView().target().size() < 2)
		return startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);

	std::unordered_map<std::string_view, std::string>::const_iterator iter;

	int len{};
	UrlDecodeWithTransChinese(m_buffer->getView().target().data() + 1, m_buffer->getView().target().size() - 1, len);

	//GET接口在读取文件时就设置了Connection 为keep-alive,为了性能考虑，这里就不修改内容直接发送出去了
	//反正检测到close的情况下，发送完http响应之后就会停止接收新消息，问题不大
	//不能直接改变里面的字符串，否则多线程操作会引起问题，如果需要改变的话，把这个字符串拷贝出来再进行修改
	iter = m_fileMap->find(std::string_view(m_buffer->getView().target().data() + 1, len));
	if (iter != m_fileMap->cend())
	{
		startWrite(iter->second.c_str(), iter->second.size());
	}
	else
		startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);
}






void HTTPSERVICE::testPingPong()
{
	std::string_view bodyView{ m_httpresult.getBody() };
	if (bodyView.empty())
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBody, HTTPRESPONSEREADY::http11OKNoBodyLen);
	}
	else
	{
		STLtreeFast &st1{ m_STLtreeFastVec[0] };

		char *sendBuffer{};
		unsigned int sendLen{};

		if (st1.make_pingPongResppnse(sendBuffer, sendLen, nullptr, 0,
			finalVersionBegin, finalVersionEnd, MAKEJSON::http200,
			MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
			bodyView.cbegin(), bodyView.cend(),
			MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
			MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen,
			MAKEJSON::Connection, MAKEJSON::Connection+ MAKEJSON::ConnectionLen,
			keep_alive? MAKEJSON::keepAlive: MAKEJSON::httpClose, keep_alive ? MAKEJSON::keepAlive+ MAKEJSON::keepAliveLen: MAKEJSON::httpClose+ MAKEJSON::httpCloseLen
			))
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
	std::string_view bodyView{ m_httpresult.getBody() };

	if (bodyView.empty())
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBodyJson, HTTPRESPONSEREADY::http11OKNoBodyJsonLen);
		return;
	}
	
	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	// 使用前先clear
	if (!st1.clear())
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBodyJson, HTTPRESPONSEREADY::http11OKNoBodyJsonLen);
		return;
	}

	//设置json转换标志，会进行转义处理，如果确定没有转义字符出现，可以不写<TRANSFORMTYPE>
	if (!st1.put<TRANSFORMTYPE>(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, bodyView.cbegin(), bodyView.cend()))
	{
		startWrite(HTTPRESPONSEREADY::http11OKNoBodyJson, HTTPRESPONSEREADY::http11OKNoBodyJsonLen);
		return;
	}

	makeSendJson<TRANSFORMTYPE>(st1);
}



//测试前提条件，在database名称为serversql下创建一张表为table1,内部含有变量name,age,book即可，
//自行插入一些数据
//mysql客户端测试函数为 testmultiSqlReadSW();
//  联合sql string_view版本 解析body查询函数
//testmultiSqlReadParseBosySW();
//   联合sql string_view版本 测试update  insert  delete以及多命令执行函数
// testmultiSqlReadUpdateSW();
void HTTPSERVICE::testmultiSqlReadSW()
{

	std::shared_ptr<resultTypeSW> &sqlRequest{ m_multiSqlRequestSWVec[0] };

	resultTypeSW& thisRequest{ *sqlRequest };

	std::vector<std::string_view> &command{ std::get<0>(thisRequest).get() };

	std::vector<unsigned int> &rowField{ std::get<3>(thisRequest).get() };

	std::vector<std::string_view> &result{ std::get<4>(thisRequest).get() };

	std::vector<unsigned int>& sqlNum{ std::get<6>(thisRequest).get() };

	
	command.clear(); 
	rowField.clear();
	result.clear();
	sqlNum.clear();

	try
	{

		command.emplace_back(std::string_view(SQLCOMMAND::testSqlReadMemory, SQLCOMMAND::testSqlReadMemoryLen));

		//
		std::get<1>(thisRequest) = 1;     
		sqlNum.emplace_back(1);
		std::get<5>(thisRequest) = std::bind(&HTTPSERVICE::handleMultiSqlReadSW, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWMaster->insertSqlRequest(sqlRequest))
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

		resultTypeSW& thisRequest{ *sqlRequest };

		std::vector<unsigned int> &rowField{ std::get<3>(thisRequest).get() };

		std::vector<std::string_view> &result{ std::get<4>(thisRequest).get() };

		std::vector<unsigned int>::const_iterator rowFieldIter;
		int sum{};

		STLtreeFast& st1{ m_STLtreeFastVec[0] }, & st2{ m_STLtreeFastVec[1] }, & st3{ m_STLtreeFastVec[2] }, & st4{ m_STLtreeFastVec[3] };

		st1.reset();
		st2.reset();
		st3.reset();
		st4.reset();
		//没有任何返回结果或者返回结果错误，自己处理
		if (result.empty() || rowField.empty() || (rowField.size() % 2))
		{

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.put(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, STATICSTRING::noResult, STATICSTRING::noResult + STATICSTRING::noResultLen))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			makeSendJson(st1);
		}
		else
		{
			//比对rowField的每两项数据的总乘积是否等于result的个数
			for (rowFieldIter = rowField.cbegin(); rowFieldIter != rowField.cend(); rowFieldIter+=2)
			{
				sum += (*rowFieldIter) * (*(rowFieldIter + 1));
			}
			
			//结果数量不匹配，自己处理
			if (sum != result.size())
			{
			
				if (!st1.clear())
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				if (!st1.put(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, STATICSTRING::noResult, STATICSTRING::noResult + STATICSTRING::noResultLen))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson(st1);

			}
			else
			{
				std::vector<std::string_view>::const_iterator resultBegin{ result.cbegin() };
				std::vector<unsigned int>::const_iterator begin{ rowField.cbegin() }, end{ rowField.cend() }, iter{ begin };
				int rowNum{}, fieldNum{}, rowCount{}, fieldCount{}, index{ -1 };


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
					index = 0;
					while (++rowCount != rowNum)
					{
						//插入查询结果第一个字段
						if (!st1.put(STATICSTRING::name, STATICSTRING::name + STATICSTRING::nameLen, resultBegin->cbegin(), resultBegin->cend()))
							return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

						++resultBegin;
						++index;

						//插入查询结果第二个字段
						if (!st1.put(STATICSTRING::age, STATICSTRING::age + STATICSTRING::ageLen, resultBegin->cbegin(), resultBegin->cend()))
							return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

						++resultBegin;
						++index;

						//插入查询结果第三个字段
						if (!st1.put(STATICSTRING::book, STATICSTRING::book + STATICSTRING::bookLen, resultBegin->cbegin(), resultBegin->cend()))
							return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

						++resultBegin;
						++index;

						if (!st2.push_back(st1))
							return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

						if (!st1.clear())
							return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
					}



					if (!st3.put_child(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, st2))
						return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

					if (!st2.clear())
						return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

					if (!st4.push_back(st3))
						return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

					if (!st3.clear())
						return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

					begin += 2;
					
				} while (begin != end);

				if (!st1.put_child(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, st4))
					return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

				makeSendJson(st1);
			

			}
			
		}
		
	}
	else
	{
		handleERRORMESSAGE(em);
	}
}



//从body中解析参数执行sql查询的测试函数
void HTTPSERVICE::testmultiSqlReadParseBosySW()
{
	if (m_httpresult.isBodyEmpty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::name, STATICSTRING::nameLen, STATICSTRING::age, STATICSTRING::ageLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char** BodyBuffer{ m_buffer->bodyPara() };

	//提取name  和  age出来
	std::string_view nameView{ *(BodyBuffer),*(BodyBuffer + 1) - *(BodyBuffer) }, ageView{ *(BodyBuffer + 2),*(BodyBuffer + 3) - *(BodyBuffer + 2) };

	//可以在这里对数据做一些校验，校验通过后插入sql查询语句
	
	//可以直接在这里构建静态string_view语句，不用再翻文件找了
	static const std::string_view query1{ "select name,age,book from table1 where name='" };
	static const std::string_view query2{ "' or age=" };
	static const std::string_view query3{ " limit 2" };


	std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

	resultTypeSW& thisRequest{ *sqlRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };

	std::vector<unsigned int>& rowField{ std::get<3>(thisRequest).get() };

	std::vector<std::string_view>& result{ std::get<4>(thisRequest).get() };

	std::vector<unsigned int>& sqlNum{ std::get<6>(thisRequest).get() };

	//command是每次进行的命令必须进行清除的，多条命令中间需要加;分隔开  
	// rowField和result在多次查询时可以复用不清空
	//另外需要多次查询时可以将insertSqlRequest第二个参数置为false，以保存之前的MYSQL_RES不被释放，这样就可以继续进行零拷贝处理
	command.clear();
	rowField.clear();
	result.clear();
	sqlNum.clear();

	//使用string_view进行拼接
	try
	{

		command.emplace_back(query1);
		command.emplace_back(nameView);
		command.emplace_back(query2);
		command.emplace_back(ageView);
		command.emplace_back(query3);


		//执行命令数为1
		std::get<1>(thisRequest) = 1;
		sqlNum.emplace_back(5);
		std::get<5>(thisRequest) = std::bind(&HTTPSERVICE::handleMultiSqlReadSW, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWMaster->insertSqlRequest(sqlRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertSql, HTTPRESPONSEREADY::httpFailToInsertSqlLen);
	}
	catch (const std::exception& e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}

}


//处理联合sql string_view版本 测试update   insert执行，delete自己可以写一条测试，需要多条操作可以参考这里
void HTTPSERVICE::testmultiSqlReadUpdateSW()
{
	std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

	resultTypeSW& thisRequest{ *sqlRequest };

	std::vector<std::string_view>& command{ std::get<0>(thisRequest).get() };

	std::vector<unsigned int>& rowField{ std::get<3>(thisRequest).get() };

	std::vector<std::string_view>& result{ std::get<4>(thisRequest).get() };

	std::vector<unsigned int>& sqlNum { std::get<6>(thisRequest).get() };

	command.clear();
	rowField.clear();
	result.clear();
	sqlNum.clear();

	//
	// 插入出现错误时会陷入程序停顿问题已经修复，内部实现了机制可以在出现错误时继续获取sql结果
	//如需进行insert操作，当插入值中有unique值时可以加入if not exists逻辑先绕过这个问题
	static const std::string_view updateSql1{ "update table1 set name='test' where name='iist'" };
	static const std::string_view updateSql2{ "insert into table1(name,age,book) value('testName',20,'python')" };
	static const std::string_view updateSql3{ "update table1 set name='test1' where name='iist1'" };
	try
	{
		//插入多语句的时候不需要加入;   在内部会自动加入
		command.emplace_back(updateSql1);
		command.emplace_back(updateSql2);

		//插入了两条执行语句，所以这里设置2
		std::get<1>(thisRequest) = 2;
		sqlNum.emplace_back(1);
		sqlNum.emplace_back(1);
		std::get<5>(thisRequest) = std::bind(&HTTPSERVICE::handlemultiSqlReadUpdateSW, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiSqlReadSWMaster->insertSqlRequest(sqlRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertSql, HTTPRESPONSEREADY::httpFailToInsertSqlLen);
	}
	catch (const std::exception& e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}

}


//处理联合sql string_view版本 测试update函数的回调函数
void HTTPSERVICE::handlemultiSqlReadUpdateSW(bool result, ERRORMESSAGE em)
{
	if (result)
	{

		std::shared_ptr<resultTypeSW>& sqlRequest{ m_multiSqlRequestSWVec[0] };

		resultTypeSW& thisRequest{ *sqlRequest };

		std::vector<unsigned int>& rowField{ std::get<3>(thisRequest).get() };

		std::vector<std::string_view>& result{ std::get<4>(thisRequest).get() };

		std::vector<unsigned int>::const_iterator rowFieldIter;
		int sum{};

		STLtreeFast& st1{ m_STLtreeFastVec[0] }, & st2{ m_STLtreeFastVec[1] }, & st3{ m_STLtreeFastVec[2] }, & st4{ m_STLtreeFastVec[3] };

		st1.reset();
		st2.reset();
		st3.reset();
		st4.reset();
		//insert 与 delete可以自行修改上个接口进行测试
		//对于update或者insert或者delete语句   result是不会有查询结果字符串返回的，上述每条命令执行后rowField里每一条执行命令会出现1 0，可以查看result中的string_view判断是否执行成功
		//执行成功返回success to execute,  失败返回  fail to execute ，可以在sql读取模块中设置相关字符串内容
		//一般在插入有unique指的sql语句时  可能出现失败情况（该值在sql中已存在的情况下）
		if (rowField.empty() || (rowField.size() % 2) || result.empty())
		{
			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.put(STATICSTRING::result, STATICSTRING::result + STATICSTRING::resultLen, STATICSTRING::noResult, STATICSTRING::noResult + STATICSTRING::noResultLen))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			makeSendJson(st1);
		}
		else if (!(rowField.size() % 2))
		{
			//执行结束，自己根据result中的字符串提示进行相关后续处理
			
			startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
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


//POST /4 HTTP/1.0\r\nHost:192.168.80.128:8085\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length:0\r\n\r\n
//  这里将foo 设置值为  set foo hello_world
//  测试从redis获取dLOT_SIZE_STRING类型数据

//测试从body中解析参数进行redis查询可以查看testMultiRedisParseBodyReadLOT_SIZE_STRING接口
//测试从redis中读取int值可以查看testMultiRedisReadINTERGER接口
//测试从redis中读取ARRAY结果集的可以查看testMultiRedisReadARRAY接口
void HTTPSERVICE::testMultiRedisReadLOT_SIZE_STRING()
{

	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view> &command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(thisRequest).get() };


	command.clear();            //插入命令string_view                   
	commandSize.clear();        //插入每条命令的string_view个数，redis客户端会在生成发送给redis-server的命令时才进行数据的copy，减少性能开销
	resultVec.clear();          //返回string_view结果的vector集，如果处理结果在一次调用之后就能完成，则可以直接使用该string_view
	                            //否则应该调用内存池申请内存块保存string_view的结果
	resultNumVec.clear();       //返回每条命令的string_view个数的vector集
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::foo, REDISNAMESPACE::fooLen));
		commandSize.emplace_back(2);
		//执行多条命令可以反复插入，例如再插入一遍此操作
		//command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		//command.emplace_back(std::string_view(REDISNAMESPACE::foo, REDISNAMESPACE::fooLen));
		//commandSize.emplace_back(2);
		//也可以执行写入命令
		//command.emplace_back(std::string_view(REDISNAMESPACE::set, REDISNAMESPACE::setLen));
		//command.emplace_back(std::string_view(REDISNAMESPACE::foo, REDISNAMESPACE::fooLen));
		//command.emplace_back(std::string_view(REDISNAMESPACE::helloWorld, REDISNAMESPACE::helloWorldLen));
		//commandSize.emplace_back(3);



		std::get<1>(thisRequest) = 1;       //执行命令个数
		std::get<3>(thisRequest) = 1;       //获取结果个数
		//如果将上述额外的2条命令都插入，则应该设置为
		//std::get<1>(thisRequest) = 3;
		//std::get<3>(thisRequest) = 3; 
		//此时resultVec中的结果会有3个string_view，resultNumVec会有3个1，表明每条命令的string_view个数各为1
		std::get<6>(thisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadLOT_SIZE_STRING, this, std::placeholders::_1, std::placeholders::_2);
		//获取到结果后执行的回调函数


		//不想创建另一个函数将逻辑分开处理的话，也可以传入一个lambda表达式，具体这么写
		//这样就可以在一个函数内完成多次异步交互，不停套娃就行了，至于性能的话，可以自己测试一下，再决定采用哪种方式
		//std::get<6>(thisRequest) = [this](bool result, ERRORMESSAGE em)
		//{
		//	  将handleMultiRedisReadLOT_SIZE_STRING  内部实现拷贝进来即可		
		//};



		//如果有主从的话，可以分别设置不同的MULTIREDISREAD对象，没有的话只传递一个进来即可
		//目前实现好MULTIREDISREAD对象类，可以正常返回多种redis结果值
		//MULTIREDISWRITE的设置目的是为了高效执行一些写入操作，不返回执行结果的字符串，只返回1和0代表成功与否，目前还需要完善
		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
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

	redisResultTypeSW& thisRequest{ *redisRequest };
	std::vector<std::string_view> &resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(thisRequest).get() };
	
	
	if (result)
	{
		if (!resultVec.empty())
		{
			STLtreeFast& st1{ m_STLtreeFastVec[0] };

			st1.reset();

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//如果确定不需要开启json转码处理，可以不带<TRANSFORMTYPE>即可，默认不开启json转码处理

			//启用json转码处理
			if (!st1.put<TRANSFORMTYPE>(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//启用json转码处理
			makeSendJson<TRANSFORMTYPE>(st1);
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
				st1.reset();

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



//POST /8 HTTP/1.0\r\nHost:192.168.80.128:8085\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length:7\r\n\r\nkey=foo
// POST /8 HTTP/1.0\r\nHost:192.168.80.128:8085\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length:6\r\n\r\nkey=fo
//  联合redis string_view版本 从body中解析参数进行查询KEY测试函数
void HTTPSERVICE::testMultiRedisParseBodyReadLOT_SIZE_STRING()
{
	if (m_httpresult.isBodyEmpty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::key, STATICSTRING::keyLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char** BodyBuffer{ m_buffer->bodyPara() };

	std::string_view keyView{ *(BodyBuffer),*(BodyBuffer + 1) - *(BodyBuffer) };

	if(keyView.empty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

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
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		command.emplace_back(keyView);
		commandSize.emplace_back(2);

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadLOT_SIZE_STRING, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
			startWrite(HTTPRESPONSEREADY::httpFailToInsertRedis, HTTPRESPONSEREADY::httpFailToInsertRedisLen);
	}
	catch (const std::exception& e)
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
	}

}


//POST /5 HTTP/1.0\r\nHost:192.168.80.128:8085\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length:0\r\n\r\n

void HTTPSERVICE::testMultiRedisReadINTERGER()
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view> &command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(thisRequest).get() };


	command.clear();
	commandSize.clear();
	resultVec.clear();
	resultNumVec.clear();
	try
	{
		command.emplace_back(std::string_view(REDISNAMESPACE::get, REDISNAMESPACE::getLen));
		command.emplace_back(std::string_view(REDISNAMESPACE::age, REDISNAMESPACE::ageLen));
		commandSize.emplace_back(2);

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadINTERGER, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
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
		if (!resultVec.empty())
		{
			STLtreeFast& st1{ m_STLtreeFastVec[0] };

			st1.reset();

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			int index{ -1 }, num{ 1 };

			if (!st1.putNumber(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, std::accumulate(std::make_reverse_iterator(resultVec[0].cend()), std::make_reverse_iterator(resultVec[0].cbegin()), 0, [&index, &num](auto& sum, auto const ch)
			{
				if (++index)num *= 10;
				return sum += (ch - '0') * num;
			})))
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



//测试redis返回ARRAY结果集的接口
void HTTPSERVICE::testMultiRedisReadARRAY()
{
	std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };
	redisResultTypeSW& thisRequest{ *redisRequest };

	std::vector<std::string_view> &command{ std::get<0>(thisRequest).get() };
	std::vector<unsigned int> &commandSize{ std::get<2>(thisRequest).get() };
	std::vector<std::string_view> &resultVec{ std::get<4>(thisRequest).get() };
	std::vector<unsigned int> &resultNumVec{ std::get<5>(thisRequest).get() };


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

		std::get<1>(thisRequest) = 1;
		std::get<3>(thisRequest) = 1;
		std::get<6>(thisRequest) = std::bind(&HTTPSERVICE::handleMultiRedisReadARRAY, this, std::placeholders::_1, std::placeholders::_2);

		if (!m_multiRedisReadMaster->insertRedisRequest(redisRequest))
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

		if (resultVec.size() == 4)
		{
			STLtreeFast& st1{ m_STLtreeFastVec[0] }, & st2{ m_STLtreeFastVec[1] };

			st1.reset();
			st2.reset();

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(REDISNAMESPACE::foo, REDISNAMESPACE::foo + REDISNAMESPACE::fooLen, &*(resultVec[0].cbegin()), &*(resultVec[0].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			
			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(REDISNAMESPACE::age, REDISNAMESPACE::age + REDISNAMESPACE::ageLen, &*(resultVec[1].cbegin()), &*(resultVec[1].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(REDISNAMESPACE::foo, REDISNAMESPACE::foo + REDISNAMESPACE::fooLen, &*(resultVec[2].cbegin()), &*(resultVec[2].cend())))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st2.push_back(st1))
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			if (!st1.clear())
				return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

			///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			if (!st1.put(REDISNAMESPACE::age1, REDISNAMESPACE::age1 + REDISNAMESPACE::age1Len, &*(resultVec[3].cbegin()), &*(resultVec[3].cend())))
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



//接口/23 将动态http头部以及内容直接写入发送缓冲区测试函数，省去一次拷贝开销
//可以搜索if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)进行了解实现细节
//当需要插入多条http头部值时，前面的http值结束时需要加入\r\n，最后一个不需要
//当不确定需要多大的空间时，可以将需要的空间值设置大一点，
//在插入数据前保存当前指针位置，在插入完成后比对指针位置长度，然后将剩余长度内容用空格填充即可
//提供给需要极限qps的场景下使用
void HTTPSERVICE::testInsertHttpHeader()
{
	STLtreeFast& st1{ m_STLtreeFastVec[0] };

	st1.reset();

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::test, STATICSTRING::test + STATICSTRING::testLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);


	static unsigned int dateLen{ 34 };
	char* sendBuffer{};
	unsigned int sendLen{};

	static std::function<void(char*&)>timeStampFun{ [this](char*& bufferBegin)
	{
		if (!bufferBegin)
			return;

		std::copy(STATICSTRING::Date, STATICSTRING::Date + STATICSTRING::DateLen, bufferBegin);
		bufferBegin += STATICSTRING::DateLen;

		*bufferBegin++ = ':';

		//设置Date值   
		m_time = std::chrono::high_resolution_clock::now();

		m_sessionTime = std::chrono::system_clock::to_time_t(m_time);

		//复用m_tmGMT生成CST时间
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


	makeSendJson<TRANSFORMTYPE, CUSTOMTAG>(st1, timeStampFun, dateLen);

}






void HTTPSERVICE::testFirstTime()
{
	m_firstTime = std::chrono::system_clock::to_time_t(std::chrono::high_resolution_clock::now());

	return startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
}






void HTTPSERVICE::testBusiness()
{
	startWrite(HTTPRESPONSEREADY::http11OK, HTTPRESPONSEREADY::http11OKLen);
}



void HTTPSERVICE::readyParseMultiPartFormData()
{
	m_boundaryLen = boundaryEnd - boundaryBegin + 4;
	char *buffer{ m_MemoryPool.getMemory<char*>(m_boundaryLen) }, *iter{};

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




void HTTPSERVICE::testMultiPartFormData()
{
	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success_upload, STATICSTRING::success_upload + STATICSTRING::success_uploadLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1, nullptr, 0);
}



void HTTPSERVICE::testSuccessUpload()
{
	STLtreeFast &st1{ m_STLtreeFastVec[0] };

	if (!st1.clear())
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	if (!st1.put(MAKEJSON::result, MAKEJSON::result + MAKEJSON::resultLen, STATICSTRING::success_upload, STATICSTRING::success_upload + STATICSTRING::success_uploadLen))
		return startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);

	makeSendJson(st1, nullptr, 0);
}



void HTTPSERVICE::readyParseChunkData()
{
	m_parseStatus = PARSERESULT::begin_checkChunkData;

	parseReadData(m_readBuffer, bodyEnd - bodyBegin);
}




//  keyValue   {"key":"value"}
//  

//测试json生成函数
void HTTPSERVICE::testMakeJson()
{
	if(m_httpresult.isBodyEmpty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	std::string_view bodyView{ m_httpresult.getBody() };
	if (!praseBody(bodyView.cbegin(), bodyView.size(), m_buffer->bodyPara(), STATICSTRING::jsonType, STATICSTRING::jsonTypeLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char **BodyBuffer{ m_buffer->bodyPara() };

	std::string_view jsonTypeView{ *(BodyBuffer + HTTPINTERFACENUM::TESTMAKEJSON::jsonType),*(BodyBuffer + HTTPINTERFACENUM::TESTMAKEJSON::jsonType + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTMAKEJSON::jsonType) };

	STLtreeFast &st1{ m_STLtreeFastVec[0] }, &st2{ m_STLtreeFastVec[1] };

	st1.reset();
	st2.reset();

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
				if (!st1.putNumber(key1Str, key1Str + key1StrLen, 1.001f))
					break;
				if (!st1.putNumber(key2Str, key2Str + key2StrLen, 10.983f))
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
			if (!st2.putString<TRANSFORMTYPE>(nullptr, nullptr, value1Str, value1Str + value1StrLen))
				break;
			if (!st2.putString<TRANSFORMTYPE>(nullptr, nullptr, nullptr, nullptr))
				break;
			if (!st2.putBoolean<TRANSFORMTYPE>(nullptr, nullptr, true))
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
	if (m_httpresult.isBodyEmpty())
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	ReadBuffer &refBuffer{ *m_buffer };

	std::string_view bodyView{ m_httpresult.getBody() };
	//解释获取body中的参数stringlen  必须存在此项参数
	if (!praseBody(bodyView.cbegin(), bodyView.size(), refBuffer.bodyPara(), STATICSTRING::stringlen, STATICSTRING::stringlenLen))
		return startWrite(HTTPRESPONSEREADY::http11invaild, HTTPRESPONSEREADY::http11invaildLen);

	const char **BodyBuffer{ refBuffer.bodyPara() };

	std::string_view numberView{ *(BodyBuffer + HTTPINTERFACENUM::TESTCOMPAREWORKFLOW::stringLen),*(BodyBuffer + HTTPINTERFACENUM::TESTCOMPAREWORKFLOW::stringLen + 1) - *(BodyBuffer + HTTPINTERFACENUM::TESTCOMPAREWORKFLOW::stringLen) };

	//从stringlen获取本次返回的字符串长度
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

		//设置Date值   
		m_sessionClock = std::chrono::system_clock::now();

		m_sessionTime = std::chrono::system_clock::to_time_t(m_sessionClock);

		//复用m_tmGMT生成CST时间
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

	//29  生成与workflow测试中的一样类型的返回消息
	if (make_compareWithWorkFlowResPonse<CUSTOMTAG>(sendBuffer, sendLen, compareFun, STATICSTRING::DateLen + 1 + dateLen, finalVersionBegin, finalVersionEnd, MAKEJSON::http200,
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
	if (!m_hasClean.load())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, m_serviceNum);
		m_hasClean.store(true);

		m_sessionLen = 0;
		m_startPos = 0;
		m_firstTime = 0;

		for (auto& st : m_STLtreeFastVec)
		{
			st.reset();
		}

		m_maxReadLen = m_defaultReadLen;
		recoverMemory();

		//将所有存储MYSQL_RES*的vector进行一次清理
		for (auto ptr : m_multiSqlRequestSWVec)
		{
			m_multiSqlReadSWMaster->FreeResult(std::get<2>(*ptr));
		}


		ec = {};
		m_buffer->getSock()->shutdown(boost::asio::socket_base::shutdown_send, ec);

		//等待异步shutdown完成
		m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
	}
}



void HTTPSERVICE::shutdownLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_buffer->getSock()->shutdown(boost::asio::socket_base::shutdown_send, ec);
		m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
	}
	else
	{
		m_buffer->getSock()->cancel(ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
}


void HTTPSERVICE::cancelLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_buffer->getSock()->cancel(ec);
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
	else
	{
		m_buffer->getSock()->close(ec);
		//等待异步cancel完成
		m_timeWheel->insert([this]() {closeLoop(); }, 5);
	}

}

void HTTPSERVICE::closeLoop()
{
	if (ec.value() != 107 && ec.value())
	{
		m_log->writeLog(__FUNCTION__, __LINE__, ec.value(), ec.message());
		m_buffer->getSock()->close(ec);
		m_timeWheel->insert([this]() {cancelLoop(); }, 5);
	}
	else
	{
		m_timeWheel->insert([this]() {resetSocket(); }, 5);
	}
}

void HTTPSERVICE::resetSocket()
{
	boost::system::error_code ec;
	m_buffer->getSock().reset(new boost::asio::ip::tcp::socket(*m_ioc));
	m_buffer->getSock()->set_option(boost::asio::socket_base::keep_alive(true), ec);

	(*m_clearFunction)(m_mySelf);
}


void HTTPSERVICE::recoverMemory()
{
	m_httpresult.resetheader();
	m_MemoryPool.prepare();
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
				//超时时clean函数会调用cancel,触发operation_aborted错误  修复发生错误时不会触发回收的情况
				if (err != boost::asio::error::operation_aborted)
				{
					
				}
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
		m_multiRedisRequestSWVec.emplace_back(std::make_shared<redisResultTypeSW>(m_stringViewVec[++j], 0, m_unsignedIntVec[++z], 0, m_stringViewVec[++j], m_unsignedIntVec[++z], nullptr,false, &m_MemoryPool));
		m_multiRedisWriteSWVec.emplace_back(std::make_shared<redisWriteTypeSW>(m_stringViewVec[++j], 0, m_unsignedIntVec[++z]));
	}

}




void HTTPSERVICE::startRead()
{

	//如果Connection则发送http 响应后继续接收新消息，否则停止接收，等待回收到对象池
	if (keep_alive)
	{
		m_buffer->getSock()->async_read_some(boost::asio::buffer(m_readBuffer + m_startPos, m_maxReadLen - m_startPos), [this](const boost::system::error_code& err, std::size_t size)
		{
			if (err)
			{
				if (err != boost::asio::error::operation_aborted)
				{
					//超时时clean函数会调用cancel,触发operation_aborted错误  修复发生错误时不会触发回收的情况
					
				}
			}
			else
			{
				if (!m_hasRecv.load())
				{
					m_hasRecv.store(true);
				}
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
}



//发生异常时还原置位，开启新的监听
//m_startPos 集中在外层设置，表示下次的起始读取位置
void HTTPSERVICE::parseReadData(const char *source, const int size)
{
	ReadBuffer &refBuffer{ *m_buffer };

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




//常规http解析对于分包处理是将数据append然后处理
//这里为了更快，每次解析不全时记录下当前状态位，下一次收到数据包时可以根据状态位恢复到最近的地方继续解析,参考状态机思想实现
//有需要才进行拷贝，没有的话只是记录下位置以备需要，同时兼顾性能与扩展，仅当关键数据段跨越两块内存区时才进行拷贝合成，否则直接用指针指向
//支持POST  GET
// 支持分包解析,可以一次发送多个http请求进来，每次进行接口处理后会接着上次解析完的位置继续解析
// 除了支持常规body  chunk解析和multipartFromdata两种上传格式解析
//支持pipeline，在每次发送完毕后判断有没有后续请求包需要解析处理  可查看startWriteLoop的相关处理
//在解析处理过程中对target、Query para以及body进行url转码处理，后续无需在转码处理，搜素UrlDecodeWithTransChinese函数即可定位处理代码
//UrlDecodeWithTransChinese可以在一次循环内同时转换url转义字符和中文字符转换

int HTTPSERVICE::parseHttp(const char* source, const int size)
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
		if(isHttp)
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
		


		if(*finalVersionBegin!='1' || *(finalVersionBegin+1)!='.')
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
	if(!parseHttpHeader())
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
bool HTTPSERVICE::parseHttpHeader()
{
	//HTTP/1.1：‌必须存在Host字段‌，缺失则返回400 Bad Request错误‌1；
	//HTTP / 1.0：允许缺失（旧协议无强制要求）‌
	const char* iter1Begin{}, * iter1End{}, * iter2Begin{}, * iter2End{};
	const char* strBegin{}, * strEnd{};
	int index{}, num{}, sum{};

	static const std::string gzip{ "gzip" }, chunked{ "chunked" }, compress{ "compress" }, deflate{ "deflate" }, identity{ "identity" };
	static const std::string keepalive{ "keep-alive" }, closeStr{ "close" };
	static const std::string Expect100_continue{ "100-continue" };
	static const std::string json{ "json" }, xml{ "xml" }, x_www_form_urlencoded{ "x-www-form-urlencoded" }, multipart_form_data{"form-data"};
	
	//字段解析基本完成，比workflow还多解析了字段
	//可以去看workflow的http_parser.c 中的__check_message_header函数比对，后面再加上Content-Type解析吧，
	


	//解析Host字段内容
	if (m_httpresult.isHostEmpty())
	{
		if(isHttp11)
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
				std::bind(std::logical_or<bool>(),std::bind(std::equal_to<>(),std::placeholders::_1, ':'), std::bind(std::equal_to<>(), std::placeholders::_1, '/')));
			//判断请求的目标服务标识是否为空  比如:80  没有前面的网址信息  HTTP / 1.0：允许缺失
			if (iter1Begin == iter1End)
			{
				if (isHttp11)
					return false;
			}
			if (iter1End != strEnd)
			{
				//斜杠（/）属于 URI 路径部分，‌禁止出现在 Host 值中
				if(*iter1End =='/')
					return false;
				iter2Begin = iter1End + 1;
				iter2End = strEnd;
				index = -1, num = 1, sum = 0;
				//使用自定义实现函数，在判断是否全部是数字字符的情况下，同时完成数值累加计算，避免两次循环调用
				if (!std::all_of(std::make_reverse_iterator(iter2End), std::make_reverse_iterator(iter2Begin), [&index, &num, &sum](const char ch)
				{
					if (!std::isdigit(ch))
						return false;
					if (++index)
						num *= 10;
					sum += (ch - '0') * num;
					return true;
				}))
					return false;
				//检测端口是否异常
				if (sum < 0 || sum>65535)
					return false;
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
			if (isHttp11)
				return false;
			m_HostNameBegin = m_HostNameEnd = nullptr;
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
				std::bind(std::logical_or<bool>(),std::bind(std::equal_to<>(),std::placeholders::_1,' '),
					std::bind(std::equal_to<>(), std::placeholders::_1, ',')));

			switch (std::distance(iter1Begin, iter1End))
			{
			case 4:
				if(!std::equal(iter1Begin, iter1End, gzip.cbegin(), gzip.cend()))
					return false;
				hasGzip = true;
				break;
			case 7:
				switch (*iter1Begin)
				{
				case 'c':
					if(!std::equal(iter1Begin +1, iter1End, chunked.cbegin()+1, chunked.cend()))
						return false;
					hasChunk = true;
					break;
				case 'd':
					if(!std::equal(iter1Begin + 1, iter1End, deflate.cbegin() + 1, deflate.cend()))
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
				if(*iter1End !=' ')
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

after_parse_Connection:
	;

	//解析Expect头部
	if (!m_httpresult.isExpectEmpty())
	{
		std::string_view ExpectView{ m_httpresult.getExpect() };
		if(std::equal(ExpectView.cbegin(), ExpectView.cend(), Expect100_continue.cbegin(), Expect100_continue.cend()))
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






































