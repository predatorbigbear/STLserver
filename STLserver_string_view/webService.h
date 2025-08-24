#pragma once


#include "publicHeader.h"
#include "readBuffer.h"
#include "ASYNCLOG.h"
#include "multiSQLREADSW.h"
#include "sqlCommand.h"
#include "mysql/mysql.h"
#include "httpinterface.h"
#include "multiRedisRead.h"
#include "multiRedisReadCopy.h"
#include "multiRedisWrite.h"
#include "STLtreeFast.h"
#include "randomParse.h"
#include "multiSQLWriteSW.h"
#include "STLTimeWheel.h"
#include "httpResult.h"
#include "commonStruct.h"
#include "CheckIP.h"
#include "randomCodeGenerator.h"
#include "verifyCode.h"
#include "userInfo.h"




#include<ctype.h>

//webService为我做实验的一个分支，开发项目请使用http 和 https 处理分支
//webService 配套类为 FixedWebServicePool  以及  webservicelistener
struct WEBSERVICE
{
	WEBSERVICE(const std::shared_ptr<io_context> &ioc, 
		const std::shared_ptr<ASYNCLOG> &log,
		const std::string& doc_root,
		const std::shared_ptr<MULTISQLREADSW> &multiSqlReadSWMaster,
		const std::shared_ptr<MULTIREDISREAD> &multiRedisReadMaster,
		const std::shared_ptr<MULTIREDISREADCOPY>& multiRedisReadCopyMaster,
		const std::shared_ptr<MULTIREDISWRITE> &multiRedisWriteMaster, 
		const std::shared_ptr<MULTISQLWRITESW> &multiSqlWriteSWMaster,
		const std::shared_ptr<STLTimeWheel> &timeWheel,
		const std::shared_ptr<std::vector<std::string>> &fileVec,
		const std::shared_ptr<std::vector<std::string>> &BGfileVec,
		const unsigned int timeOut, bool& success, const unsigned int serviceNum,
		const std::shared_ptr<std::function<void(std::shared_ptr<WEBSERVICE>&)>>& cleanFun,
		const std::shared_ptr<CHECKIP> &checkIP,
		const std::shared_ptr<RandomCodeGenerator> &randomCode,
		const std::shared_ptr<VERIFYCODE>& verifyCode,
		const unsigned int bufNum = 4096
	);

	void setReady(std::shared_ptr<WEBSERVICE>& other);

	//在实际运行中，读写在不同线程中运行，所以需要改成多线程同步
	std::shared_ptr<WEBSERVICE>* getListIter();

	//在实际运行中，读写在不同线程中运行，所以需要改成多线程同步
	void setListIter(std::shared_ptr<WEBSERVICE>* iter);

	//64位系统下，不超过8字节的传值更高效
	void checkTimeOut();

	void clean();

	std::unique_ptr<ReadBuffer> m_buffer{};

	//在成功插入超时检测时调用，解决插入超时检测队列后瞬间被回收问题
	void setRecvTrue();





private:
	const std::shared_ptr<VERIFYCODE> m_verifyCode{};

	const std::shared_ptr<MULTIREDISREADCOPY> m_multiRedisReadCopyMaster{};

	const std::shared_ptr<RandomCodeGenerator>m_randomCode{};

	const std::shared_ptr<CHECKIP>m_checkIP{};

	int m_requestTime{};                            //总请求次数

	std::vector<std::string_view> keyVec;         //存储body结果的vector

	bool hasLoginBack{ false };               //是否成功登录了后台
	bool hasVerifyRegister{ false };          //是否成功在用户注册验证了手机号
	bool hasUserLogin{ false };               //普通用户（非后台登录）是否已经登录账号


	unsigned char m_loginBackTime{};           //输入后台登录密码错误计数
	unsigned char m_userLoginTime{};           //输入用户登录密码错误计数


	std::string m_IP{};                  //本次连接IP地址
	std::string m_phone{};               //用户注册手机号
	unsigned int m_port{};               //本次连接端口


	const unsigned int m_serviceNum{};

	const std::string& m_doc_root;

	const std::shared_ptr<io_context> m_ioc{};

	int m_len{};

	const std::shared_ptr<std::function<void(std::shared_ptr<WEBSERVICE>&)>> m_clearFunction{};

	const std::shared_ptr<ASYNCLOG> m_log{};

	std::atomic<std::shared_ptr<WEBSERVICE>*>mySelfIter{};

	std::shared_ptr<WEBSERVICE> m_mySelf{};

	const std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //时间轮定时器

	const std::shared_ptr<std::vector<std::string>>m_fileVec{};                //普通网页html

	const std::shared_ptr<std::vector<std::string>>m_BGfileVec{};              //网页管理后台html

	std::chrono::_V2::system_clock::time_point m_time{ std::chrono::high_resolution_clock::now() };

	std::atomic<bool>m_hasRecv{ false };                  //是否接收过数据
	std::atomic<bool>m_hasClean{ false };                 //是否启动过清理


	using resultType = std::tuple<const char**, unsigned int, unsigned int, std::shared_ptr<MYSQL_RES*>, unsigned int, std::shared_ptr<unsigned int[]>, unsigned int,
		std::shared_ptr<const char* []>, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>>;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/*
	执行命令string_view集,string_view集中不需要加;   多条sql语句应分开多条string_view传入
	获取的结果个数
	装载MYSQL_RES*的vector，不用清空，每次插入命令到sql操作模块之后内部会自动执行释放内存操作
	根据查询的命令，每一条查询命令会有两项数据显示，第一项是该条命令返回结果个数，第二个是每条命令的字段个数
	返回的查询结果的string_view个数，总个数应该相当于上述两项数据的乘积   该条命令返回结果个数  *  条命令的字段个数
	回调处理函数
	用于组成每条sql语句的string_view个数（用于检查是否中间带有;  避免mysql解析错误）
	*/


	using resultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>>,
		std::reference_wrapper<std::vector<unsigned int>>, std::reference_wrapper<std::vector<std::string_view>>, std::function<void(bool, enum ERRORMESSAGE)>,
		std::reference_wrapper<std::vector<unsigned int>>>;

	/*
	执行命令string_view集
	执行命令个数
	每条命令的词语个数（方便根据redis RESP进行拼接）
	获取结果次数  （因为比如一些事务操作可能不一定有结果返回）

	返回结果string_view
	每个结果的词语个数
	是否进行拷贝数据操作,默认为false,需要拷贝数据时设置为true

	内存池指针

	回调函数
	*/
	// redis类型
	using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>, MEMORYPOOL<>&>;



	/*
	执行命令string_view集
	执行命令个数
	每条命令的词语个数（方便根据redis RESP进行拼接）
	加锁直接拼接字符串到总string中，发送前加锁取出数据进行发送

	最后回调回去删减计数标志

	利用计算标志让写入时仍然可以通过类指针的方式利用其他地方bufffer中的源数据，在真正进行拼接后才进行回调进行删减
	*/
	// redis写入 更新类型
	using redisWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>>;


	//每次使用前先进行清空操作
	std::vector<std::vector<std::string_view>>m_stringViewVec{};     //stringView分配vec

	std::vector<std::vector<MYSQL_RES*>>m_mysqlResVec;             //mysql_Res* 分配的vec

	std::vector<std::vector<unsigned int>>m_unsignedIntVec;        //unsigned int 分配的vector

	std::vector<STLtreeFast>m_STLtreeFastVec;


	//  用于获取char动态数组的内存池   发送结果并入这里获取内存块
	MEMORYPOOL<> m_MemoryPool;



	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::vector<std::shared_ptr<resultTypeSW>> m_multiSqlRequestSWVec;


	std::vector<std::shared_ptr<redisResultTypeSW>> m_multiRedisRequestSWVec;


	std::vector<std::shared_ptr<redisWriteTypeSW>> m_multiRedisWriteSWVec;






	const std::shared_ptr<MULTISQLREADSW>m_multiSqlReadSWMaster{};

	const std::shared_ptr<MULTIREDISREAD>m_multiRedisReadMaster{};

	const std::shared_ptr<MULTIREDISWRITE>m_multiRedisWriteMaster{};

	const std::shared_ptr<MULTISQLWRITESW>m_multiSqlWriteSWMaster{};



	const unsigned int m_timeOut{};

	unsigned int m_rowNum{};

	unsigned int m_fieldNum{};

	unsigned int m_tempLen{};

	unsigned int m_chunkLen{};

	int m_startPos{};

	//为提高性能，分配的读取buffer应满足能装载大部分包括body部分的长度，这样大部分情况下仅以指针指向，
	//上传文件需要另加代码处理，这里仅对post 后段body部分进行优化
	int m_maxReadLen{ };

	int m_defaultReadLen{ };

	int m_parseStatus{ PARSERESULT::complete };


	/////////////////////////////////////////


	char* m_readBuffer{};




	const char* funBegin{}, * funEnd{}, * finalFunBegin{}, * finalFunEnd{},
		* targetBegin{}, * targetEnd{}, * finalTargetBegin{}, * finalTargetEnd{},
		* paraBegin{}, * paraEnd{}, * finalParaBegin{}, * finalParaEnd{},
		* bodyBegin{ }, * bodyEnd{ }, * finalBodyBegin{}, * finalBodyEnd{},
		* versionBegin{}, * versionEnd{}, * finalVersionBegin{}, * finalVersionEnd{},
		* headBegin{}, * headEnd{}, * finalHeadBegin{}, * finalHeadEnd{},
		* wordBegin{}, * wordEnd{}, * finalWordBegin{}, * finalWordEnd{},
		* httpBegin{}, * httpEnd{}, * finalHttpBegin{}, * finalHttpEnd{},
		* lineBegin{}, * lineEnd{}, * finalLineBegin{}, * finalLineEnd{},
		* chunkNumBegin{}, * chunkNumEnd{}, * finalChunkNumBegin{}, * finalChunkNumEnd{},
		* chunkDataBegin{}, * chunkDataEnd{}, * finalChunkDataBegin{}, * finalChunkDataEnd{},
		* boundaryBegin{}, * boundaryEnd{},
		* findBoundaryBegin{}, * findBoundaryEnd{},
		* boundaryHeaderBegin{}, * boundaryHeaderEnd{},
		* boundaryWordBegin{}, * boundaryWordEnd{}, * thisBoundaryWordBegin{}, * thisBoundaryWordEnd{},
		* messageBegin{}, * messageEnd{}
	;



	/////////////////////////////////////////////////////////////////////////////////////////////////


	bool hasChunk{ false };                    //是否是Chunk编码格式

	bool hasCompress{ false };                //是否支持LZW压缩

	bool hasDeflate{ false };                 //是否支持zlib压缩

	bool hasGzip{ false };                    //是否支持LZ77压缩

	bool hasIdentity{ true };                 //是否不进行任何压缩或分块，传输原始数据

	bool expect_continue{ false };

	bool hasJson{ false };                     //请求格式是否是json

	bool hasXml{ false };                      //请求格式是否是XML

	bool hasX_www_form_urlencoded{ false };         //请求格式是否是x-www-form-urlencoded

	bool hasMultipart_form_data{ false };          //请求格式是否是multipart/form-data


	bool keep_alive{ true };                     // Connection值   默认为true，解析到close之后会设置为false



	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::unique_ptr<const char* []>m_httpHeaderMap{};

	bool isHttp10{ false };                   //是否是http1.0

	bool isHttp11{ false };                   //是否是http1.1



	/////////////////////////////////////////////////////////////////////////////////////////////


	const bool isHttp{ true };                //是否是http

	int hostPort{ 80 };                         //http host默认端口



	///////////////////////////////////////////////////////////////////////////////////////////////


	/////////////////////////////////////////////////////////////////

	HTTPRESULT m_httpresult;


	const char** m_HostNameBegin{}, ** m_HostNameEnd{};
	const char** m_HostPortBegin{}, ** m_HostPortEnd{};

	const char** m_Boundary_ContentDispositionBegin{}, ** m_Boundary_ContentDispositionEnd{};
	const char** m_Boundary_NameBegin{}, ** m_Boundary_NameEnd{};
	const char** m_Boundary_FilenameBegin{}, ** m_Boundary_FilenameEnd{};
	const char** m_Boundary_ContentTypeBegin{}, ** m_Boundary_ContentTypeEnd{};








	size_t m_availableLen{};

	int m_boundaryLen{};

	int m_bodyLen{};

	int m_boundaryHeaderPos{};
	///////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::vector<std::pair<const char*, const char*>>m_dataBufferVec;     //存储分散的某段数据的临时vector    


	unsigned int accumulateLen{};          //累积长度



	//文件大小
	int m_fileSize{};
	int m_readFileSize{};

	std::ifstream m_file{};

	int m_getStatus{};


	std::string_view m_message{};

	USERINFO m_userInfo;

	/////////////////////////////////////////

private:
	enum PARSERESULT
	{
		invaild = 0,

		check_messageComplete = 1,

		complete = 2,

		do_notthing = 3,

		find_funBegin = 4,

		find_funEnd = 5,

		find_targetBegin = 6,

		find_targetEnd = 7,

		find_paraBegin = 8,

		find_paraEnd = 9,

		find_httpBegin = 10,

		find_httpEnd = 11,

		find_versionBegin = 12,

		find_versionEnd = 13,

		find_lineEnd = 14,

		find_headBegin = 15,

		find_headEnd = 16,

		find_bodyLenBegin = 17,

		find_bodyLenEnd = 18,

		find_wordBegin = 19,

		find_wordEnd = 20,

		find_secondLineEnd = 21,

		find_finalBodyEnd = 22,

		find_fistCharacter = 23,

		find_chunkNumBegin = 24,

		find_chunkNumEnd = 25,

		find_thirdLineBegin = 26,

		find_thirdLineEnd = 27,

		find_chunkDataBegin = 28,

		find_chunkDataEnd = 29,

		find_fourthLineBegin = 30,

		find_fourthLineEnd = 31,

		parseMultiPartFormData = 32,

		parseChunk = 33,

		begin_checkChunkData = 34,

		begin_checkBoundaryBegin = 35,

		find_boundaryBegin = 36,

		find_boundaryEnd = 37,

		find_halfBoundaryBegin = 38,

		find_boundaryHeaderBegin = 39,

		find_boundaryHeaderBegin2 = 40,

		find_nextboundaryHeaderBegin = 41,

		find_nextboundaryHeaderBegin2 = 42,

		find_nextboundaryHeaderBegin3 = 43,

		find_boundaryHeaderEnd = 44,

		find_boundaryWordBegin = 45,

		find_boundaryWordBegin2 = 46,

		find_boundaryWordEnd = 47,

		find_boundaryWordEnd2 = 48,

		find_fileBegin = 49,

		find_fileBoundaryBegin = 50,

		find_fileBoundaryBegin2 = 51,

		check_fileBoundaryEnd = 52,

		check_fileBoundaryEnd2 = 53,

		find_ContentLengthBegin = 54,

		find_ContentLengthEnd = 55,

		before_findSecondLineEnd = 56
	};

	boost::system::error_code ec;

	std::chrono::system_clock::time_point m_sessionClock, m_sessionGMTClock, m_thisClock;       //用于取值当前时间点

	time_t m_sessionTime, m_sessionGMTTime, m_thisTime;

	time_t m_firstTime{};         //首次测试时间戳

	struct tm* m_tmGMT;


	const char* m_sendBuffer{};
	unsigned int m_sendLen{};
	unsigned int m_sendTimes{};

private:

	void sslShutdownLoop();

	void shutdownLoop();

	void cancelLoop();

	void closeLoop();

	void resetSocket();

	//在每次发送完毕后重置资源
	void recoverMemory();

	void sendOK();

	void cleanData();

	// 执行预备操作
	void prepare();

	void handShake();

	void startRead();

	void parseReadData(const char* source, const int size);         // 解析数据

	int parseHttp(const char* source, const int size);             // 解析Http数据

	bool parseHttpHeader();                                        //解析http 字段内容

	// 普通模式
	// READFROMDISK 直接从磁盘读取文件发送模式
	template<typename SENDMODE = void>
	void startWrite(const char* source, const int size);


	// 普通模式
	// READFROMDISK 直接从磁盘读取文件发送模式
	template<typename SENDMODE = void>
	void startWriteLoop(const char* source, const int size);


	template<typename T = void, typename HTTPFLAG = void, typename HTTPFUNCTION = void*>
	void makeSendJson(STLtreeFast& st, HTTPFUNCTION httpWrite = nullptr, unsigned int httpLen = 0);


	//文件名称
	//文件长度
	//发送buffer预分配空间
	//发送buffer装载返回地址
	//发送buffer http前缀大小
	//
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool makeFileFront(std::string_view fileName, const unsigned int fileLen, const unsigned int assignLen, char*& resultPtr, unsigned int& resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char* httpVersionBegin, const char* httpVersionEnd,
		const char* httpCodeBegin, const char* httpCodeEnd, const char* httpResultBegin, const char* httpResultEnd, ARG&&...args);


	//用来生成与workflow测试文件相同的返回数据的专用打包函数
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool make_compareWithWorkFlowResPonse(char*& resultPtr, unsigned int& resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char* httpVersionBegin, const char* httpVersionEnd,
		const char* httpCodeBegin, const char* httpCodeEnd, const char* httpResultBegin, const char* httpResultEnd, unsigned int randomBodyLen, ARG&&...args);

	void run();

	void checkMethod();

	void switchPOSTInterface();



	

	void testGet();     //读取文件返回

	///////////////////////////////////////////////////


	void handleERRORMESSAGE(ERRORMESSAGE em);



	//////////////////////////////////////////////////////////////////////





	void testBusiness();


	void readyParseMultiPartFormData();

	//测试上传文件
	void testMultiPartFormData();


	void testSuccessUpload();


	//测试解析chunk格式数据
	void readyParseChunkData();



	/// ------------------------------------------------------

	std::function<void()>m_business;

	// 验证函数专用buf，外界不要使用
	std::unique_ptr<const char* []>m_verifyData;

	void resetVerifyData();


	/////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////
	//                   业务逻辑区

	//登录后台管理
	void loginBackGround();

	void handleloginBackGround(bool result, ERRORMESSAGE em);


	//退出后台管理
	void exitBack();


	//用户注册接口1，填写手机号发送验证码
	void registration1();


	void handleregistration1(bool result, ERRORMESSAGE em);


	void handleregistration11(bool result, ERRORMESSAGE em);


	//检查手机号与验证码接口
	void checkVerifyCode();


	void handlecheckVerifyCode(bool result, ERRORMESSAGE em);


	//提交用户注册信息
	void registration2();


	void handleregistration21(bool result, ERRORMESSAGE em);


	void handleregistration22(bool result, ERRORMESSAGE em);


	//用户登录账号
	void userLogin();


	void handleuserLogin(bool result, ERRORMESSAGE em);


	//用户登录账号退出
	void userLoginOut();

	//用户提交信息登记
	void userInfo();


	void handleuserInfo(bool result, ERRORMESSAGE em);


	//获取用户五项信息
	void getUserInfo();

	void handlegetUserInfo(bool result, ERRORMESSAGE em);


	//后台查询待审核用户信息
	void getUserInfoExamine();


	void handlegetUserInfoExamine(bool result, ERRORMESSAGE em);











	///////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////



	template<typename HEADERTYPE>
	bool calLength(int& httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
	{
		if constexpr (std::is_same<HEADERTYPE, const char*>::value)
		{
			if (!headerBegin || !headerEnd || !wordBegin || !wordEnd)
				return false;
			httpLength += std::distance(headerBegin, headerEnd) + 3 + std::distance(wordBegin, wordEnd);
			return true;
		}
		return false;
	}



	template<typename HEADERTYPE, typename ...ARG>
	bool calLength(int& httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
	{
		if constexpr (std::is_same<HEADERTYPE, const char*>::value)
		{
			if (!headerBegin || !headerEnd || !wordBegin || !wordEnd)
				return false;
			httpLength += std::distance(headerBegin, headerEnd) + 3 + std::distance(wordBegin, wordEnd);
			return calLength(httpLength, args...);
		}
		return false;
	}



	template<typename HEADERTYPE>
	void packAgeHttpHeader(char*& httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
	{
		if constexpr (std::is_same<HEADERTYPE, const char*>::value)
		{
			std::copy(headerBegin, headerEnd, httpPointer);
			httpPointer += std::distance(headerBegin, headerEnd);

			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, httpPointer);
			httpPointer += MAKEJSON::colonLen;

			std::copy(wordBegin, wordEnd, httpPointer);
			httpPointer += std::distance(wordBegin, wordEnd);

			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, httpPointer);
			httpPointer += MAKEJSON::halfNewLineLen;
		}
	}



	template<typename HEADERTYPE, typename ...ARG>
	void packAgeHttpHeader(char*& httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
	{
		if constexpr (std::is_same<HEADERTYPE, const char*>::value)
		{
			std::copy(headerBegin, headerEnd, httpPointer);
			httpPointer += std::distance(headerBegin, headerEnd);

			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, httpPointer);
			httpPointer += MAKEJSON::colonLen;

			std::copy(wordBegin, wordEnd, httpPointer);
			httpPointer += std::distance(wordBegin, wordEnd);

			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, httpPointer);
			httpPointer += MAKEJSON::halfNewLineLen;

			packAgeHttpHeader(httpPointer, args...);
		}
	}


};



template<typename SENDMODE>
inline void WEBSERVICE::startWrite(const char* source, const int size)
{
	startWriteLoop<SENDMODE>(source, size);
}



template<typename SENDMODE>
inline void WEBSERVICE::startWriteLoop(const char* source, const int size)
{
	//每次成功发送一条消息累计请求数
	++m_requestTime;
	boost::asio::async_write(*m_buffer->getSSLSock(), boost::asio::buffer(source, size), [this](const boost::system::error_code& err, std::size_t size)
	{
		if (err)
		{
			//超时时clean函数会调用cancel,触发operation_aborted错误  修复发生错误时不会触发回收的情况
			if (err != boost::asio::error::operation_aborted)
			{
				clean();
			}

			//发生错误时等待超时回收，clean函数内会对内存池进行重置
		}
		else
		{
			if constexpr (std::is_same<SENDMODE, void>::value)
			{
				switch (m_parseStatus)
				{
				case PARSERESULT::complete:
					recoverMemory();
					startRead();
					break;
				case PARSERESULT::check_messageComplete:
					//因为这里的内存块可能不是默认读取内存块，所以回收操作应该在copy处理之后才进行
					if (m_readBuffer != m_buffer->getBuffer())
					{
						std::copy(messageBegin, messageEnd, m_buffer->getBuffer());
						m_readBuffer = m_buffer->getBuffer();
						messageEnd = m_readBuffer + (messageEnd - messageBegin);
						messageBegin = m_readBuffer;
						m_maxReadLen = m_defaultReadLen;
					}
					recoverMemory();
					parseReadData(messageBegin, messageEnd - messageBegin);
					break;
					//default处理请求非法以及因内存申请错误的情况
				case PARSERESULT::invaild:
					recoverMemory();
					clean();
					break;
				default:
					recoverMemory();
					startRead();
					break;
				}
			}

		}
	});
}



template<typename T, typename HTTPFLAG, typename HTTPFUNCTION>
inline void WEBSERVICE::makeSendJson(STLtreeFast& st, HTTPFUNCTION httpWrite, unsigned int httpLen)
{
	if (!st.isEmpty())
	{
		char* sendBuffer{};
		unsigned int sendLen{};

		if (keep_alive)
		{
			if (st.make_json<T, HTTPFLAG>(sendBuffer, sendLen, httpWrite, httpLen,
				MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen,
				MAKEJSON::Connection, MAKEJSON::Connection + MAKEJSON::ConnectionLen,
				MAKEJSON::keepAlive, MAKEJSON::keepAlive + MAKEJSON::keepAliveLen,
				MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen,
				MAKEJSON::applicationJson, MAKEJSON::applicationJson + MAKEJSON::applicationJsonLen
			))
			{
				startWrite(sendBuffer, sendLen);
			}
			else
			{
				startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
			}
		}
		else
		{
			if (st.make_json<T, HTTPFLAG>(sendBuffer, sendLen, httpWrite, httpLen,
				MAKEJSON::httpOneZero, MAKEJSON::httpOneZero + MAKEJSON::httpOneZerolen, MAKEJSON::http200,
				MAKEJSON::http200 + MAKEJSON::http200Len, MAKEJSON::httpOK, MAKEJSON::httpOK + MAKEJSON::httpOKLen,
				MAKEJSON::AccessControlAllowOrigin, MAKEJSON::AccessControlAllowOrigin + MAKEJSON::AccessControlAllowOriginLen,
				MAKEJSON::httpStar, MAKEJSON::httpStar + MAKEJSON::httpStarLen,
				MAKEJSON::Connection, MAKEJSON::Connection + MAKEJSON::ConnectionLen,
				MAKEJSON::httpClose, MAKEJSON::httpClose + MAKEJSON::httpCloseLen,
				MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen,
				MAKEJSON::applicationJson, MAKEJSON::applicationJson + MAKEJSON::applicationJsonLen
			))
			{
				startWrite(sendBuffer, sendLen);
			}
			else
			{
				startWrite(WEBSERVICEANSWER::result2memory.data(), WEBSERVICEANSWER::result2memory.size());
			}
		}
	}
	else
	{
		startWrite(WEBSERVICEANSWER::result2request.data(), WEBSERVICEANSWER::result2request.size());
	}
}




template<typename HTTPFLAG, typename HTTPFUNCTION, typename ...ARG>
inline bool WEBSERVICE::makeFileFront(std::string_view fileName, const unsigned int fileLen, const unsigned int assignLen, char*& resultPtr, unsigned int& resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char* httpVersionBegin, const char* httpVersionEnd, const char* httpCodeBegin, const char* httpCodeEnd, const char* httpResultBegin, const char* httpResultEnd, ARG && ...args)
{
	int parSize{ sizeof...(args) }, httpHeadLen{};
	if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !calLength(httpHeadLen, args...))
		return false;

	int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
		+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

	if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
	{
		needLength += httpLen + MAKEJSON::halfNewLineLen;
	}

	string_view mineTypeStr{ mine_type(fileName) };

	unsigned int needFrontLen{ needLength + stringLen(fileLen) + MAKEJSON::newLineLen + MAKEJSON::ContentTypeLen + 1 + mineTypeStr.size() + MAKEJSON::halfNewLineLen };

	//如果预申请空间连前缀空间都不能装载，则返回失败
	if (assignLen < needFrontLen)
		return false;

	char* newResultPtr{ m_MemoryPool.getMemory<char*>(assignLen) };

	if (!newResultPtr)
		return false;

	resultPtr = newResultPtr;

	std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
	newResultPtr += MAKEJSON::httpFrontLen;


	std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
	newResultPtr += std::distance(httpVersionBegin, httpVersionEnd);


	std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
	newResultPtr += MAKEJSON::spaceLen;


	std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
	newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);


	std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
	newResultPtr += MAKEJSON::spaceLen;


	std::copy(httpResultBegin, httpResultEnd, newResultPtr);
	newResultPtr += std::distance(httpResultBegin, httpResultEnd);


	std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
	newResultPtr += MAKEJSON::halfNewLineLen;

	packAgeHttpHeader(newResultPtr, args...);

	std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
	newResultPtr += MAKEJSON::ContentLengthLen;


	std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
	newResultPtr += MAKEJSON::colonLen;


	if (fileLen > 99999999)
		*newResultPtr++ = fileLen / 100000000 + '0';
	if (fileLen > 9999999)
		*newResultPtr++ = fileLen / 10000000 % 10 + '0';
	if (fileLen > 999999)
		*newResultPtr++ = fileLen / 1000000 % 10 + '0';
	if (fileLen > 99999)
		*newResultPtr++ = fileLen / 100000 % 10 + '0';
	if (fileLen > 9999)
		*newResultPtr++ = fileLen / 10000 % 10 + '0';
	if (fileLen > 999)
		*newResultPtr++ = fileLen / 1000 % 10 + '0';
	if (fileLen > 99)
		*newResultPtr++ = fileLen / 100 % 10 + '0';
	if (fileLen > 9)
		*newResultPtr++ = fileLen / 10 % 10 + '0';
	*newResultPtr++ = fileLen % 10 + '0';

	std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
	newResultPtr += MAKEJSON::halfNewLineLen;

	std::copy(MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen, newResultPtr);
	newResultPtr += MAKEJSON::ContentTypeLen;

	*newResultPtr++ = ':';

	std::copy(mineTypeStr.cbegin(), mineTypeStr.cend(), newResultPtr);
	newResultPtr += mineTypeStr.size();

	if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
	{
		std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
		newResultPtr += MAKEJSON::halfNewLineLen;
		httpWrite(newResultPtr);
	}

	std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);
	newResultPtr += MAKEJSON::newLineLen;

	resultLen = std::distance(resultPtr, newResultPtr);

	return true;
}



template<typename HTTPFLAG, typename HTTPFUNCTION, typename ...ARG>
inline bool WEBSERVICE::make_compareWithWorkFlowResPonse(char*& resultPtr, unsigned int& resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen,
	const char* httpVersionBegin, const char* httpVersionEnd, const char* httpCodeBegin, const char* httpCodeEnd, const char* httpResultBegin, const char* httpResultEnd,
	unsigned int randomBodyLen, ARG && ...args)
{
	int parSize{ sizeof...(args) }, httpHeadLen{};
	if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !randomBodyLen || !calLength(httpHeadLen, args...))
		return false;

	int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
		+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

	if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
	{
		needLength += httpLen + MAKEJSON::halfNewLineLen;
	}

	resultPtr = nullptr, resultLen = 0;

	unsigned int thisbodyLen{ randomBodyLen };

	unsigned int needFrontLen{ needLength + stringLen(thisbodyLen) + MAKEJSON::newLineLen };

	unsigned int needLen{ needFrontLen + thisbodyLen };     //93

	/////////////////////////////

	//////////////////////////////////////////////////


	char* newResultPtr{ m_MemoryPool.getMemory<char*>(needLen) };

	if (!newResultPtr)
		return false;

	resultPtr = newResultPtr, resultLen = needLen;

	std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
	newResultPtr += MAKEJSON::httpFrontLen;


	std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
	newResultPtr += std::distance(httpVersionBegin, httpVersionEnd);


	std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
	newResultPtr += MAKEJSON::spaceLen;


	std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
	newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);


	std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
	newResultPtr += MAKEJSON::spaceLen;


	std::copy(httpResultBegin, httpResultEnd, newResultPtr);
	newResultPtr += std::distance(httpResultBegin, httpResultEnd);


	std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
	newResultPtr += MAKEJSON::halfNewLineLen;

	packAgeHttpHeader(newResultPtr, args...);

	std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
	newResultPtr += MAKEJSON::ContentLengthLen;


	std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
	newResultPtr += MAKEJSON::colonLen;


	if (thisbodyLen > 99999999)
		*newResultPtr++ = thisbodyLen / 100000000 + '0';
	if (thisbodyLen > 9999999)
		*newResultPtr++ = thisbodyLen / 10000000 % 10 + '0';
	if (thisbodyLen > 999999)
		*newResultPtr++ = thisbodyLen / 1000000 % 10 + '0';
	if (thisbodyLen > 99999)
		*newResultPtr++ = thisbodyLen / 100000 % 10 + '0';
	if (thisbodyLen > 9999)
		*newResultPtr++ = thisbodyLen / 10000 % 10 + '0';
	if (thisbodyLen > 999)
		*newResultPtr++ = thisbodyLen / 1000 % 10 + '0';
	if (thisbodyLen > 99)
		*newResultPtr++ = thisbodyLen / 100 % 10 + '0';
	if (thisbodyLen > 9)
		*newResultPtr++ = thisbodyLen / 10 % 10 + '0';
	*newResultPtr++ = thisbodyLen % 10 + '0';


	if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
	{
		std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
		newResultPtr += MAKEJSON::halfNewLineLen;
		httpWrite(newResultPtr);
	}

	std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);
	newResultPtr += MAKEJSON::newLineLen;

	if (thisbodyLen <= randomStringLen)
	{
		std::copy(randomString, randomString + thisbodyLen, newResultPtr);
	}
	else
	{
		do
		{
			std::copy(randomString, randomString + randomStringLen, newResultPtr);
			newResultPtr += randomStringLen;
			thisbodyLen -= randomStringLen;
		} while (thisbodyLen >= randomStringLen);

		std::copy(randomString, randomString + thisbodyLen, newResultPtr);
	}

	return true;
}












