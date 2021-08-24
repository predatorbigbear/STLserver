#pragma once


#include "publicHeader.h"
#include "MyReq.h"
#include "readBuffer.h"
#include "LOG.h"
#include "multiSQLREADSW.h"
#include "sqlCommand.h"
#include "mysql.h"
#include "httpinterface.h"
#include "multiRedisRead.h"
#include "multiRedisWrite.h"
#include "STLtreeFast.h"
#include "randomParse.h"
#include "multiSQLWriteSW.h"


struct HTTPSERVICE
{
	HTTPSERVICE(std::shared_ptr<io_context> ioc, std::shared_ptr<LOG> log, 
		std::shared_ptr<MULTISQLREADSW>multiSqlReadSWSlave,
		std::shared_ptr<MULTISQLREADSW>multiSqlReadSWMaster, std::shared_ptr<MULTIREDISREAD>multiRedisReadSlave,
		std::shared_ptr<MULTIREDISREAD>multiRedisReadMaster,
		std::shared_ptr<MULTIREDISWRITE>multiRedisWriteMaster, std::shared_ptr<MULTISQLWRITESW>multiSqlWriteSWMaster,
		const unsigned int timeOut, bool & success,
		char *publicKeyBegin, char *publicKeyEnd, int publicKeyLen, char *privateKeyBegin, char *privateKeyEnd, int privateKeyLen,
		RSA* rsaPublic, RSA* rsaPrivate
		);

	void setReady(const int index, std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>)>>clearFunction, std::shared_ptr<HTTPSERVICE> other);

	std::shared_ptr<HTTPSERVICE> *&getListIter();

	bool checkTimeOut();

	void clean();

	std::unique_ptr<ReadBuffer> m_buffer{};





private:
	boost::system::error_code m_err{};

	std::shared_ptr<io_context> m_ioc{};

	std::unique_ptr<boost::asio::steady_timer> m_timer{};

	int m_index;

	int m_len{};

	std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>)>>m_clearFunction{};

	std::shared_ptr<LOG> m_log{};

	std::shared_ptr<HTTPSERVICE> *mySelfIter{};

	std::shared_ptr<HTTPSERVICE> m_mySelf{};

	std::mutex m_lock;

	std::chrono::_V2::system_clock::time_point m_time{ std::chrono::high_resolution_clock::now() };

	bool m_hasClean{ false };


	using resultType = std::tuple<const char**, unsigned int, unsigned int, std::shared_ptr<MYSQL_RES*>, unsigned int, std::shared_ptr<unsigned int[]>, unsigned int,
		std::shared_ptr<const char*[]>, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>>;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	using resultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>>,
		std::reference_wrapper<std::vector<unsigned int>>, std::reference_wrapper<std::vector<std::string_view>>, std::function<void(bool, enum ERRORMESSAGE)> >;

	/*
	执行命令string_view集
	执行命令个数
	每条命令的词语个数（方便根据redis RESP进行拼接）
	获取结果次数  （因为比如一些事务操作可能不一定有结果返回）

	返回结果string_view
	每个结果的词语个数

	回调函数
	*/
	// redis类型
	using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>>;



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


	//  用于获取char动态数组的内存池
	MEMORYPOOL<char> m_charMemoryPool;

	// 获取char*动态数组的内存池
	MEMORYPOOL<char*> m_charPointerMemoryPool;

	//  用于获取char动态数组的内存池(用于存储发送结果）
	MEMORYPOOL<char> m_sendMemoryPool;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::vector<std::shared_ptr<resultTypeSW>> m_multiSqlRequestSWVec;


	std::vector<std::shared_ptr<redisResultTypeSW>> m_multiRedisRequestSWVec;


	std::vector<std::shared_ptr<redisWriteTypeSW>> m_multiRedisWriteSWVec;



	

	std::shared_ptr<MULTISQLREADSW>m_multiSqlReadSWSlave{};

	std::shared_ptr<MULTISQLREADSW>m_multiSqlReadSWMaster{};
	
	std::shared_ptr<MULTIREDISREAD>m_multiRedisReadSlave{};

	std::shared_ptr<MULTIREDISREAD>m_multiRedisReadMaster{};
		
	std::shared_ptr<MULTIREDISWRITE>m_multiRedisWriteMaster{};
	
	std::shared_ptr<MULTISQLWRITESW>m_multiSqlWriteSWMaster{};



	unsigned int m_timeOut;

	unsigned int m_rowNum{};

	unsigned int m_fieldNum{};

	unsigned int m_tempLen{};

	unsigned int m_chunkLen{};


	int m_startPos{};

	//为提高性能，分配的读取buffer应满足能装载大部分包括body部分的长度，这样大部分情况下仅以指针指向，
	//上传文件需要另加代码处理，这里仅对post 后段body部分进行优化
	int m_maxReadLen{ 1024 };

	int m_defaultReadLen{ 1024 };

	int m_parseStatus{ PARSERESULT::complete };


	/////////////////////////////////////////

	std::unique_ptr<char[]> m_SessionID;

	int m_sessionMemory{};

	int m_sessionLen{};
	//根据sessionID是否为empty判断处理cookie时应该执行的操作


	char *m_readBuffer{};

	const char *m_funBegin{}, *m_funEnd{}, *m_finalFunBegin{}, *m_finalFunEnd{},
		*m_targetBegin{}, *m_targetEnd{}, *m_finalTargetBegin{}, *m_finalTargetEnd{},
		*m_paraBegin{}, *m_paraEnd{}, *m_finalParaBegin{}, *m_finalParaEnd{},
		*m_bodyBegin{ }, *m_bodyEnd{ }, *m_finalBodyBegin{}, *m_finalBodyEnd{},
		*m_versionBegin{}, *m_versionEnd{}, *m_finalVersionBegin{}, *m_finalVersionEnd{},
		*m_headBegin{}, *m_headEnd{}, *m_finalHeadBegin{}, *m_finalHeadEnd{},
		*m_wordBegin{}, *m_wordEnd{}, *m_finalWordBegin{}, *m_finalWordEnd{},
		*m_httpBegin{}, *m_httpEnd{}, *m_finalHttpBegin{}, *m_finalHttpEnd{},
		*m_lineBegin{}, *m_lineEnd{}, *m_finalLineBegin{}, *m_finalLineEnd{},
		*m_chunkNumBegin{}, *m_chunkNumEnd{}, *m_finalChunkNumBegin{}, *m_finalChunkNumEnd{},
		*m_chunkDataBegin{}, *m_chunkDataEnd{}, *m_finalChunkDataBegin{}, *m_finalChunkDataEnd{},
		*m_boundaryBegin{}, *m_boundaryEnd{},
		*m_findBoundaryBegin{}, *m_findBoundaryEnd{},
		*m_boundaryHeaderBegin{}, *m_boundaryHeaderEnd{},
		*m_boundaryWordBegin{}, *m_boundaryWordEnd{}, *m_thisBoundaryWordBegin{}, *m_thisBoundaryWordEnd{},
		*m_messageBegin{}, *m_messageEnd{}
		;

	bool hasBody{ false };

	bool hasPara{ false };

	bool hasChunk{ false };

	std::unique_ptr<const char*[]>m_httpHeaderMap{};


	//因为http状态位有限，所以设置以下指针指向对应位置，在初始化时设置好，以后直接获取（最强“哈希”）
	const char **m_MethodBegin{}, **m_MethodEnd{};
	const char **m_TargetBegin{}, **m_TargetEnd{};
	const char **m_VersionBegin{}, **m_VersionEnd{};
	const char **m_AcceptBegin{}, **m_AcceptEnd{};
	const char **m_Accept_CharsetBegin{}, **m_Accept_CharsetEnd{};
	const char **m_Accept_EncodingBegin{}, **m_Accept_EncodingEnd{};
	const char **m_Accept_LanguageBegin{}, **m_Accept_LanguageEnd{};
	const char **m_Accept_RangesBegin{}, **m_Accept_RangesEnd{};
	const char **m_AuthorizationBegin{}, **m_AuthorizationEnd{};
	const char **m_Cache_ControlBegin{}, **m_Cache_ControlEnd{};
	const char **m_ConnectionBegin{}, **m_ConnectionEnd{};
	const char **m_CookieBegin{}, **m_CookieEnd{};
	const char **m_Content_LengthBegin{}, **m_Content_LengthEnd{};
	const char **m_Content_TypeBegin{}, **m_Content_TypeEnd{};
	const char **m_DateBegin{}, **m_DateEnd{};
	const char **m_ExpectBegin{}, **m_ExpectEnd{};
	const char **m_FromBegin{}, **m_FromEnd{};
	const char **m_HostBegin{}, **m_HostEnd{};
	const char **m_If_MatchBegin{}, **m_If_MatchEnd{};
	const char **m_If_Modified_SinceBegin{}, **m_If_Modified_SinceEnd{};
	const char **m_If_None_MatchBegin{}, **m_If_None_MatchEnd{};
	const char **m_If_RangeBegin{}, **m_If_RangeEnd{};
	const char **m_If_Unmodified_SinceBegin{}, **m_If_Unmodified_SinceEnd{};
	const char **m_Max_ForwardsBegin{}, **m_Max_ForwardsEnd{};
	const char **m_PragmaBegin{}, **m_PragmaEnd{};
	const char **m_Proxy_AuthorizationBegin{}, **m_Proxy_AuthorizationEnd{};
	const char **m_RangeBegin{}, **m_RangeEnd{};
	const char **m_RefererBegin{}, **m_RefererEnd{};
	const char **m_TEBegin{}, **m_TEEnd{};
	const char **m_UpgradeBegin{}, **m_UpgradeEnd{};
	const char **m_User_AgentBegin{}, **m_User_AgentEnd{};
	const char **m_ViaBegin{}, **m_ViaEnd{};
	const char **m_WarningBegin{}, **m_WarningEnd{};
	const char **m_BodyBegin{}, **m_BodyEnd{};
	const char **m_Transfer_EncodingBegin{}, **m_Transfer_EncodingEnd{};
	
	const char **m_Boundary_ContentDispositionBegin{}, **m_Boundary_ContentDispositionEnd{};
	const char **m_Boundary_NameBegin{}, **m_Boundary_NameEnd{};
	const char **m_Boundary_FilenameBegin{}, **m_Boundary_FilenameEnd{};
	const char **m_Boundary_ContentTypeBegin{}, **m_Boundary_ContentTypeEnd{};




	size_t m_availableLen{};

	int m_boundaryLen{};

	int m_bodyLen{};

	int m_boundaryHeaderPos{};
	///////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::vector<std::pair<const char*, const char*>>m_dataBufferVec;     //存储分散的某段数据的临时vector    

	unsigned int m_accumulateLen{};          //累积长度


	char *m_publicKeyBegin{};
	char *m_publicKeyEnd{};
	int m_publicKeyLen{};


	char *m_privateKeyBegin{};
	char *m_privateKeyEnd{};
	int m_privateKeyLen{};


	RSA *m_rsaPublic{};               //服务器公钥
	RSA *m_rsaPrivate{};

	BIO *m_bio{};
	RSA *m_rsaClientPublic{};         //客户端公钥
	int m_rsaClientSize{};            //客户端 RSA_size


	MD5_CTX m_ctx;


	bool m_hasMakeRandom{ false };          // 是否产生过验证字符串

	std::unique_ptr<char[]>m_clientAES{};  // 保存客户端AES密钥的缓存空间

	//16  24  32
	int m_clientAESLen{};                   // 客户端AES密钥长度


	AES_KEY aes_encryptkey;                  //加密AES
	AES_KEY aes_decryptkey;                  //解密AES

	/////////////////////////////////////////

private:
	enum PARSERESULT
	{
		invaild,

		check_messageComplete,

		complete,

		do_notthing,

		find_funBegin,

		find_funEnd,

		find_targetBegin,

		find_targetEnd,

		find_paraBegin,

		find_paraEnd,

		find_httpBegin,

		find_httpEnd,

		find_versionBegin,

		find_versionEnd,

		find_lineEnd,

		find_headBegin,

		find_headEnd,

		find_bodyLenBegin,

		find_bodyLenEnd,

		find_wordBegin,

		find_wordEnd,

		find_secondLineEnd,

		find_finalBodyEnd,

		find_fistCharacter,

		find_chunkNumBegin,

		find_chunkNumEnd,

		find_thirdLineBegin,

		find_thirdLineEnd,

	    find_chunkDataBegin,

		find_chunkDataEnd,

		find_fourthLineBegin,

		find_fourthLineEnd,

		parseMultiPartFormData,

		parseChunk,

		begin_checkChunkData,

		begin_checkBoundaryBegin,

		find_boundaryBegin,

		find_boundaryEnd,

		find_halfBoundaryBegin,

		find_boundaryHeaderBegin,

		find_boundaryHeaderBegin2,

		find_nextboundaryHeaderBegin,

		find_nextboundaryHeaderBegin2,

		find_nextboundaryHeaderBegin3,

		find_boundaryHeaderEnd,

		find_boundaryWordBegin,

		find_boundaryWordBegin2,

		find_boundaryWordEnd,

		find_boundaryWordEnd2,

		find_fileBegin,

		find_fileBoundaryBegin,

		check_fileBoundaryEnd,

		check_fileBoundaryEnd2
	};


	char *m_randomString{};       // 每个对象里面都有自己的字符串进行随机处理

	std::chrono::system_clock::time_point m_sessionClock, m_sessionGMTClock ,m_thisClock;       //用于取值当前时间点

	time_t m_sessionTime, m_sessionGMTTime , m_thisTime;

	time_t m_firstTime{};         //首次测试时间戳

	struct tm *m_tmGMT;


	const char *m_sendBuffer{};
	unsigned int m_sendLen{};

	const char *m_sendBegin{};
	const char *m_sendEnd{};

private:

	void sendOK();

	void cleanData();

	// 执行预备操作
	void prepare();

	void startRead();

	void parseReadData(const char *source, const int size);         // 解析数据

	int parseHttp(const char *source, const int size);             // 解析Http数据

	void startWrite(const char *source, const int size);

	void startWriteLoop(const char *source, const int size);

	template<typename T=void, typename HTTPFLAG = void, typename ENCTYPT = void, typename HTTPFUNCTION=void*>
	void makeSendJson(STLtreeFast &st, HTTPFUNCTION httpWrite=nullptr, unsigned int httpLen=0, AES_KEY *enctyptKey=nullptr);

	void run();

	void checkMethod();

	void switchPOSTInterface();



	void testBody();

	void testRandomBody();

	///////////////////////////////////////////////////

	void testPingPong();

	void handleERRORMESSAGE(ERRORMESSAGE em);


	//  联合sql string_view版本 查询函数
	void testmultiSqlReadSW();

	void handleMultiSqlReadSW(bool result, ERRORMESSAGE em);


	//  联合redis string_view版本 查询函数KEY测试函数
	void testMultiRedisReadLOT_SIZE_STRING();

	void handleMultiRedisReadLOT_SIZE_STRING(bool result, ERRORMESSAGE em);


	//  联合redis string_view版本 查询函数数值测试函数
	void testMultiRedisReadINTERGER();

	void handleMultiRedisReadINTERGER(bool result, ERRORMESSAGE em);


	//  联合redis string_view版本 查询函数数值测试函数
	void testMultiRedisReadARRAY();

	void handleMultiRedisReadARRAY(bool result, ERRORMESSAGE em);



	void testGET();

	void handleTestGET(bool result, ERRORMESSAGE em);



	
	void testLogin();

	void handleTestLoginRedisSlave(bool result, ERRORMESSAGE em);

	void handleTestLoginSqlSlave(bool result, ERRORMESSAGE em);


	// 退出登录,清除session
	void testLogout();



	// https://blog.csdn.net/qq_36467463/article/details/78480136
	// 获取公钥
	// 将公钥以及随机字符串发送给客户端,
	void testGetPublicKey();


	//客户端收到服务器公钥和随机字符串后，按照内定规则对随机字符串做某种处理，使用服务器公钥加密后做哈希返回
	//服务器检测该哈希值是否为合法哈希
	//不合法，返回提示
	//合法，尝试初始化客户端公钥，并用客户端返回的随机字符串做某种处理使用客户端公钥加密后做哈希返回
	//参数 客户端公钥 publicKey   hashValue   randomString
	void testGetClientPublicKey();


	
	// 1 服务端与客户端必须保持长连接（重要前提）
	// 2 每次建立新连接时服务端清空AES密钥，客户端必须与服务端利用服务端公钥约定新的AES密钥加上时间戳返回
	// 3 服务器获取时间戳判断是否在可控时间范围内，超出时间则失效

	// 后续其他接口请求如果没有客户端AES密钥信息一律直接拒绝执行，
	// 攻击者后续利用相同包来发送时首次时间戳 第一次时间戳会比请求AES密钥中的时间戳要晚，从根本上杜绝了第三者重放攻击或修改参数

	//测定首次时间戳
	void testFirstTime();


	// 客户端请求获取AES密钥，使用客户端的公钥进行加密后传输
	//AESKey0时间戳|   服务器公钥加密
	void testGetEncryptKey();


	//////////////////////////////////////////////////////////////////////


	//   加密部分字符串前端发给后端的内容  body部分用AES全加密，如果需要填充则前面加0后加密

	//   后端返回前端的内容，整个body段的json串加密，如果需要填充则后面加0后加密，前端收到后解密从后面向前找}确定json串长度后读取




	//  （内部使用一种形式，和HTTPS不同）
	// 一切业务接口首先判断服务端有没有保存AES密钥
	// 尝试进行HTTP加密登陆，返回token
	// username password
	// 服务端不存明文那么客户端注册 登陆时直接先进行md5再传送即可
	void testEncryptLogin();

	void handleTestLoginRedisSlaveEncrypt(bool result, ERRORMESSAGE em);

	void handleTestLoginSqlSlaveEncrypt(bool result, ERRORMESSAGE em);




	template<typename VERIFYTYPE = VERIFYINTERFACE>
	void testVerify();



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
	std::unique_ptr<const char*[]>m_verifyData;

	void resetVerifyData();

	bool verifyAES();

	template<typename VERIFYTYPE = VERIFYINTERFACE>
	int verifySession();

	void verifySessionBackWard(bool result, ERRORMESSAGE em);

	void verifySessionBackWardHttp(bool result, ERRORMESSAGE em);








	/////////////////////////////////////////////////////////////



	template<typename T =void,typename ...ARG>
	void makeFileResPonse(const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, ARG&&...args)
	{
		int httpHeadLen{};
		if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !calLength(httpHeadLen, args...))
		{
			startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);
			return;
		}


		if constexpr (std::is_same<T, REDISNOKEY>::value)
		{
			try
			{
				if (std::equal(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), STATICSTRING::forwardSlash, STATICSTRING::forwardSlash + STATICSTRING::forwardSlashLen))
				{
					if (!std::filesystem::exists(STATICSTRING::httpDefaultFile) || std::filesystem::is_directory(STATICSTRING::httpDefaultFile)
						|| std::filesystem::is_symlink(STATICSTRING::httpDefaultFile))
					{
						startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);
						return;
					}

					std::ifstream file(STATICSTRING::httpDefaultFile, std::ios::binary);
					if (!file)
					{
						startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);
						return;
					}

					size_t fileSize{ std::filesystem::file_size(STATICSTRING::httpDefaultFile) };

					std::string_view contentTypeSw{ mine_type(STATICSTRING::httpDefaultFile) };

					int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
						+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen
						+ MAKEJSON::ContentLengthLen + MAKEJSON::colonLen + stringLen(fileSize) + MAKEJSON::halfNewLineLen
						+ MAKEJSON::ContentTypeLen + MAKEJSON::colonLen + contentTypeSw.size() + MAKEJSON::newLineLen };

					int needLen{ needLength + fileSize };

					char *httpBuffer{ m_sendMemoryPool.getMemory(needLen) };

					if (!httpBuffer)
					{
						startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
						return;
					}

					char *bufferPtr{ httpBuffer };

					std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, bufferPtr);
					bufferPtr += MAKEJSON::httpFrontLen;

					std::copy(httpVersionBegin, httpVersionEnd, bufferPtr);
					bufferPtr += std::distance(httpVersionBegin, httpVersionEnd);

					std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, bufferPtr);
					bufferPtr += MAKEJSON::spaceLen;

					std::copy(httpCodeBegin, httpCodeEnd, bufferPtr);
					bufferPtr += std::distance(httpCodeBegin, httpCodeEnd);

					std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, bufferPtr);
					bufferPtr += MAKEJSON::spaceLen;

					std::copy(httpResultBegin, httpResultEnd, bufferPtr);
					bufferPtr += std::distance(httpResultBegin, httpResultEnd);

					std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, bufferPtr);
					bufferPtr += MAKEJSON::halfNewLineLen;

					packAgeHttpHeader(bufferPtr, args...);

					std::copy(MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen, bufferPtr);
					bufferPtr += MAKEJSON::ContentTypeLen;

					std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, bufferPtr);
					bufferPtr += MAKEJSON::colonLen;

					std::copy(contentTypeSw.cbegin(), contentTypeSw.cend(), bufferPtr);
					bufferPtr += contentTypeSw.size();

					std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, bufferPtr);
					bufferPtr += MAKEJSON::halfNewLineLen;

					std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, bufferPtr);
					bufferPtr += MAKEJSON::ContentLengthLen;

					std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, bufferPtr);
					bufferPtr += MAKEJSON::colonLen;

					if (fileSize > 99999999)
						*bufferPtr++ = fileSize / 100000000 + '0';
					if (fileSize > 9999999)
						*bufferPtr++ = fileSize / 10000000 % 10 + '0';
					if (fileSize > 999999)
						*bufferPtr++ = fileSize / 1000000 % 10 + '0';
					if (fileSize > 99999)
						*bufferPtr++ = fileSize / 100000 % 10 + '0';
					if (fileSize > 9999)
						*bufferPtr++ = fileSize / 10000 % 10 + '0';
					if (fileSize > 999)
						*bufferPtr++ = fileSize / 1000 % 10 + '0';
					if (fileSize > 99)
						*bufferPtr++ = fileSize / 100 % 10 + '0';
					if (fileSize > 9)
						*bufferPtr++ = fileSize / 10 % 10 + '0';
					*bufferPtr++ = fileSize % 10 + '0';

					std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, bufferPtr);

					bufferPtr += MAKEJSON::newLineLen;

					file.read(const_cast<char*>(bufferPtr), fileSize);

					//bufferPtr  到 bufferPtr+fileSize  发送到redis 进行set操作


					std::shared_ptr<redisWriteTypeSW> redisWriteRequest{ m_multiRedisWriteSWVec[0] };

					std::vector<std::string_view> &commandVec{ std::get<0>(*redisWriteRequest).get() };

					std::get<1>(*redisWriteRequest) = 1;

					std::vector<unsigned int> &commandSizeVec{ std::get<2>(*redisWriteRequest).get() };

					commandVec.clear();
					commandSizeVec.clear();
					commandVec.emplace_back(std::string_view(REDISNAMESPACE::setnx, REDISNAMESPACE::setnxLen));
					commandVec.emplace_back(std::string_view(STATICSTRING::loginHtml, STATICSTRING::loginHtmlLen));
					commandVec.emplace_back(std::string_view(bufferPtr, fileSize));
					commandSizeVec.emplace_back(3);


					m_multiRedisWriteMaster->insertRedisRequest(redisWriteRequest);

					startWrite(httpBuffer, needLen);
				}
				else
				{

					/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

					if (!std::filesystem::exists(m_buffer->getView().target()) || std::filesystem::is_directory(m_buffer->getView().target())
						|| std::filesystem::is_symlink(m_buffer->getView().target()))
					{
						startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);
						return;
					}

					int FileNameSize{ STATICSTRING::httpDirLen + 1 + m_buffer->getView().target().size() + 1 };
					char *fileName{ m_charMemoryPool.getMemory(FileNameSize) };
					if (!fileName)
					{
						startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
						return;
					}

					char *fileNamePtr{ fileName };
					std::copy(STATICSTRING::httpDir, STATICSTRING::httpDir + STATICSTRING::httpDirLen, fileNamePtr);
					fileNamePtr += STATICSTRING::httpDirLen;
					*fileNamePtr++ = '/';
					std::copy(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), fileNamePtr);
					fileNamePtr += m_buffer->getView().target().size();
					*fileNamePtr = 0;

					std::ifstream file(fileName, std::ios::binary);
					if (!file)
					{
						startWrite(HTTPRESPONSEREADY::http404Nofile, HTTPRESPONSEREADY::http404NofileLen);
						return;
					}

					size_t fileSize{ std::filesystem::file_size(fileName) };

					std::string_view contentTypeSw{ mine_type(m_buffer->getView().target()) };

					int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
						+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen
						+ MAKEJSON::ContentLengthLen + MAKEJSON::colonLen + stringLen(fileSize) + MAKEJSON::halfNewLineLen
						+ MAKEJSON::ContentTypeLen + MAKEJSON::colonLen + contentTypeSw.size() + MAKEJSON::newLineLen };

					int needLen{ needLength + fileSize };

					
					char *httpBuffer{ m_sendMemoryPool.getMemory(needLen) };

					if (!httpBuffer)
					{
						startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
						return;
					}

					char *bufferPtr{ httpBuffer };

					std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, bufferPtr);
					bufferPtr += MAKEJSON::httpFrontLen;

					std::copy(httpVersionBegin, httpVersionEnd, bufferPtr);
					bufferPtr += std::distance(httpVersionBegin, httpVersionEnd);

					std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, bufferPtr);
					bufferPtr += MAKEJSON::spaceLen;

					std::copy(httpCodeBegin, httpCodeEnd, bufferPtr);
					bufferPtr += std::distance(httpCodeBegin, httpCodeEnd);

					std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, bufferPtr);
					bufferPtr += MAKEJSON::spaceLen;

					std::copy(httpResultBegin, httpResultEnd, bufferPtr);
					bufferPtr += std::distance(httpResultBegin, httpResultEnd);

					std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, bufferPtr);
					bufferPtr += MAKEJSON::halfNewLineLen;

					packAgeHttpHeader(bufferPtr, args...);

					std::copy(MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen, bufferPtr);
					bufferPtr += MAKEJSON::ContentTypeLen;

					std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, bufferPtr);
					bufferPtr += MAKEJSON::colonLen;

					std::copy(contentTypeSw.cbegin(), contentTypeSw.cend(), bufferPtr);
					bufferPtr += contentTypeSw.size();

					std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, bufferPtr);
					bufferPtr += MAKEJSON::halfNewLineLen;

					std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, bufferPtr);
					bufferPtr += MAKEJSON::ContentLengthLen;

					std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, bufferPtr);
					bufferPtr += MAKEJSON::colonLen;

					if (fileSize > 99999999)
						*bufferPtr++ = fileSize / 100000000 + '0';
					if (fileSize > 9999999)
						*bufferPtr++ = fileSize / 10000000 % 10 + '0';
					if (fileSize > 999999)
						*bufferPtr++ = fileSize / 1000000 % 10 + '0';
					if (fileSize > 99999)
						*bufferPtr++ = fileSize / 100000 % 10 + '0';
					if (fileSize > 9999)
						*bufferPtr++ = fileSize / 10000 % 10 + '0';
					if (fileSize > 999)
						*bufferPtr++ = fileSize / 1000 % 10 + '0';
					if (fileSize > 99)
						*bufferPtr++ = fileSize / 100 % 10 + '0';
					if (fileSize > 9)
						*bufferPtr++ = fileSize / 10 % 10 + '0';
					*bufferPtr++ = fileSize % 10 + '0';

					std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, bufferPtr);

					bufferPtr += MAKEJSON::newLineLen;

					file.read(const_cast<char*>(bufferPtr), fileSize);

					//bufferPtr  到 bufferPtr+fileSize  发送到redis 进行set操作


					std::shared_ptr<redisWriteTypeSW> redisWriteRequest{ m_multiRedisWriteSWVec[0] };

					std::vector<std::string_view> &commandVec{ std::get<0>(*redisWriteRequest).get() };

					std::get<1>(*redisWriteRequest) = 1;

					std::vector<unsigned int> &commandSizeVec{ std::get<2>(*redisWriteRequest).get() };


					commandVec.clear();
					commandVec.emplace_back(std::string_view(REDISNAMESPACE::setnx, REDISNAMESPACE::setnxLen));
					commandVec.emplace_back(m_buffer->getView().target());
					commandVec.emplace_back(std::string_view(bufferPtr, fileSize));

					m_multiRedisWriteMaster->insertRedisRequest(redisWriteRequest);

					startWrite(httpBuffer, needLen);

				}
			}
			catch (const std::exception &e)
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
		
		if constexpr (std::is_same<T, void>::value)
		{

			std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

			std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };

			std::string_view &fileKey{ resultVec[0] };

			size_t fileSize{ fileKey.size() };

			std::string_view contentTypeSw;

			if (std::equal(m_buffer->getView().target().cbegin(), m_buffer->getView().target().cend(), STATICSTRING::forwardSlash, STATICSTRING::forwardSlash + STATICSTRING::forwardSlashLen))
			{
				std::string_view contentTemp{ mine_type(std::string_view(STATICSTRING::loginHtml, STATICSTRING::loginHtmlLen)) };
				contentTypeSw.swap(contentTemp);
			}
			else
			{
				std::string_view contentTemp{ mine_type(m_buffer->getView().target()) };
				contentTypeSw.swap(contentTemp);
			}	


			int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
					+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen
					+ MAKEJSON::ContentLengthLen + MAKEJSON::colonLen + stringLen(fileSize) + MAKEJSON::halfNewLineLen
					+ MAKEJSON::ContentTypeLen + MAKEJSON::colonLen + contentTypeSw.size() + MAKEJSON::newLineLen };

			int needLen{ needLength + fileSize };

			
			char *httpBuffer{ m_sendMemoryPool.getMemory(needLen) };

			if (!httpBuffer)
			{
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
				return;
			}

			char *bufferPtr{ httpBuffer };

			std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, bufferPtr);
			bufferPtr += MAKEJSON::httpFrontLen;

			std::copy(httpVersionBegin, httpVersionEnd, bufferPtr);
			bufferPtr += std::distance(httpVersionBegin, httpVersionEnd);

			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, bufferPtr);
			bufferPtr += MAKEJSON::spaceLen;

			std::copy(httpCodeBegin, httpCodeEnd, bufferPtr);
			bufferPtr += std::distance(httpCodeBegin, httpCodeEnd);

			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, bufferPtr);
			bufferPtr += MAKEJSON::spaceLen;

			std::copy(httpResultBegin, httpResultEnd, bufferPtr);
			bufferPtr += std::distance(httpResultBegin, httpResultEnd);

			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, bufferPtr);
			bufferPtr += MAKEJSON::halfNewLineLen;

			packAgeHttpHeader(bufferPtr, args...);

			std::copy(MAKEJSON::ContentType, MAKEJSON::ContentType + MAKEJSON::ContentTypeLen, bufferPtr);
			bufferPtr += MAKEJSON::ContentTypeLen;

			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, bufferPtr);
			bufferPtr += MAKEJSON::colonLen;

			std::copy(contentTypeSw.cbegin(), contentTypeSw.cend(), bufferPtr);
			bufferPtr += contentTypeSw.size();

			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, bufferPtr);
			bufferPtr += MAKEJSON::halfNewLineLen;

			std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, bufferPtr);
			bufferPtr += MAKEJSON::ContentLengthLen;

			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, bufferPtr);
			bufferPtr += MAKEJSON::colonLen;

			if (fileSize > 99999999)
				*bufferPtr++ = fileSize / 100000000 + '0';
			if (fileSize > 9999999)
				*bufferPtr++ = fileSize / 10000000 % 10 + '0';
			if (fileSize > 999999)
				*bufferPtr++ = fileSize / 1000000 % 10 + '0';
			if (fileSize > 99999)
				*bufferPtr++ = fileSize / 100000 % 10 + '0';
			if (fileSize > 9999)
				*bufferPtr++ = fileSize / 10000 % 10 + '0';
			if (fileSize > 999)
				*bufferPtr++ = fileSize / 1000 % 10 + '0';
			if (fileSize > 99)
				*bufferPtr++ = fileSize / 100 % 10 + '0';
			if (fileSize > 9)
				*bufferPtr++ = fileSize / 10 % 10 + '0';
			*bufferPtr++ = fileSize % 10 + '0';

			std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, bufferPtr);

			bufferPtr += MAKEJSON::newLineLen;

			std::copy(fileKey.cbegin(), fileKey.cend(), bufferPtr);

			startWrite(httpBuffer, needLen);


        }
	}


	template<typename HEADERTYPE>
	bool calLength(int &httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
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
	bool calLength(int &httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
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
	void packAgeHttpHeader(char *&httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
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
	void packAgeHttpHeader(char *&httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
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



template<typename T, typename HTTPFLAG, typename ENCTYPT, typename HTTPFUNCTION>
inline void HTTPSERVICE::makeSendJson(STLtreeFast & st, HTTPFUNCTION httpWrite, unsigned int httpLen, AES_KEY * enctyptKey)
{
	if (!st.isEmpty())
	{
		char *sendBuffer{};
		unsigned int sendLen{};


		if (st.make_json<T, HTTPFLAG, ENCTYPT>(sendBuffer, sendLen, httpWrite, httpLen, enctyptKey,
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
}

template<typename VERIFYTYPE>
inline void HTTPSERVICE::testVerify()
{
	if (std::is_same<VERIFYTYPE, VERIFYINTERFACE>::value)
	{
		resetVerifyData();
	}
	

	if (!verifyAES())
	{
		if (std::is_same<VERIFYTYPE, VERIFYINTERFACE>::value)
		{
			return startWrite(HTTPRESPONSEREADY::httpFailToVerify, HTTPRESPONSEREADY::httpFailToVerifyLen);
		}
		else if (std::is_same<VERIFYTYPE, VERIFYHTTP>::value)
		{
			m_startPos = 0;
			hasChunk = false;
			m_readBuffer = m_buffer->getBuffer();
			m_maxReadLen = m_defaultReadLen;
			m_dataBufferVec.clear();
			m_availableLen = m_buffer->getSock()->available();
			m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::http10invaild), m_sendLen = HTTPRESPONSEREADY::http10invaildLen;
			cleanData();
		}
	}


	switch (verifySession<VERIFYTYPE>())
	{
	case VerifyFlag::verifyOK:
		//调用主处理函数
		m_business();
		break;

	case VerifyFlag::verifyFail:
		if (std::is_same<VERIFYTYPE, VERIFYINTERFACE>::value)
		{
			return startWrite(m_sendBuffer, m_sendLen);
		}
		else if (std::is_same<VERIFYTYPE, VERIFYHTTP>::value)
		{
			m_startPos = 0;
			hasChunk = false;
			m_readBuffer = m_buffer->getBuffer();
			m_maxReadLen = m_defaultReadLen;
			m_dataBufferVec.clear();
			m_availableLen = m_buffer->getSock()->available();
			cleanData();
		}
		break;
	case VerifyFlag::verifySession:
	default:

		break;
	}
}

template<typename VERIFYTYPE>
inline int HTTPSERVICE::verifySession()
{
	const char *cookieBegin{ *m_CookieBegin }, *cookieEnd{ *m_CookieEnd };

	if (!cookieBegin || !cookieEnd)
	{
		m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToVerify), m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
		return VerifyFlag::verifyFail;
	}

	const char *sessionBegin{ std::find(cookieBegin,cookieEnd,'=') };

	if (sessionBegin == cookieEnd)
	{
		m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToVerify), m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
		return VerifyFlag::verifyFail;
	}

	++sessionBegin;

	if (std::distance(sessionBegin, cookieEnd) <= m_sessionLen)
	{
		m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToVerify), m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
		return VerifyFlag::verifyFail;
	}

	*(m_verifyData.get() + VerifyDataPos::httpAllSessionBegin) = sessionBegin;
	*(m_verifyData.get() + VerifyDataPos::httpAllSessionEnd) = cookieEnd;


	const char *sessionEnd{}, *userNameBegin{}, *userNameEnd{};

	if (m_sessionLen)
	{
		sessionEnd = sessionBegin + m_sessionLen;

		if (!std::equal(sessionBegin, sessionEnd, m_SessionID.get(), m_SessionID.get() + m_sessionLen))
		{
			m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToVerify), m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
			return VerifyFlag::verifyFail;
		}

		*(m_verifyData.get() + VerifyDataPos::httpSessionBegin) = sessionBegin;
		*(m_verifyData.get() + VerifyDataPos::httpSessionEnd) = sessionEnd;


		userNameBegin = sessionEnd, userNameEnd = cookieEnd;

		if (userNameBegin == userNameEnd)
		{
			m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToVerify), m_sendLen = HTTPRESPONSEREADY::httpFailToVerifyLen;
			return VerifyFlag::verifyFail;
		}

		*(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) = userNameBegin;
		*(m_verifyData.get() + VerifyDataPos::httpSessionUserNameEnd) = userNameEnd;

		std::string_view userNameView{ userNameBegin ,userNameEnd - userNameBegin };

		m_thisClock = std::chrono::high_resolution_clock::now();

		m_thisTime = std::chrono::system_clock::to_time_t(m_thisClock);

		// sessionID 超时
		if (m_thisTime >= m_sessionTime)
		{
			//  HDEL KEY_NAME FIELD1.. FIELDN 
			// hdel user session::userName   sessionExpires::userName 

			std::shared_ptr<redisWriteTypeSW> &redisWrite{ m_multiRedisWriteSWVec[0] };

			std::vector<std::string_view> &writeCommand{ std::get<0>(*redisWrite).get() };
			std::vector<unsigned int> &writeCommandSize{ std::get<2>(*redisWrite).get() };

			writeCommand.clear();
			writeCommandSize.clear();

			char *sessionBuf{ m_charMemoryPool.getMemory(STATICSTRING::sessionLen + 2 + 2 * userNameView.size() + STATICSTRING::sessionExpireLen) };

			if (!sessionBuf)
			{
				m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpSTDException), m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
				return VerifyFlag::verifyFail;
			}

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
				m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpSTDException), m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
				return VerifyFlag::verifyFail;
			}


			if (!m_multiRedisWriteMaster->insertRedisRequest(redisWrite))
			{
				m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToInsertRedis), m_sendLen = HTTPRESPONSEREADY::httpFailToInsertRedisLen;
				return VerifyFlag::verifyFail;
			}

			m_thisTime = 0, m_sessionLen = 0;

			///////////////////////////////////////
			//  向客户端发出清除cookie消息，让客户端重新进行登陆操作

			STLtreeFast &st1{ m_STLtreeFastVec[0] };

			if (!st1.clear())
			{
				m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpSTDException), m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
				return VerifyFlag::verifyFail;
			}


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
				m_sendBuffer = sendBuffer, m_sendLen = sendLen;
			}
			else
			{
				m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpSTDException), m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
			}

			return VerifyFlag::verifyFail;
		}

		return VerifyFlag::verifyOK;
	}
	else
	{

		sessionEnd = sessionBegin + 24;

		*(m_verifyData.get() + VerifyDataPos::httpSessionBegin) = sessionBegin;
		*(m_verifyData.get() + VerifyDataPos::httpSessionEnd) = sessionEnd;

		userNameBegin = sessionEnd, userNameEnd = cookieEnd;

		*(m_verifyData.get() + VerifyDataPos::httpSessionUserNameBegin) = sessionBegin;
		*(m_verifyData.get() + VerifyDataPos::httpSessionUserNameEnd) = sessionEnd;

		int userNameLen{ userNameEnd - userNameBegin };

		// hmget user session:userName   sessionExpires:userName

		std::shared_ptr<redisResultTypeSW> &redisRequest{ m_multiRedisRequestSWVec[0] };

		std::vector<std::string_view> &command{ std::get<0>(*redisRequest).get() };
		std::vector<unsigned int> &commandSize{ std::get<2>(*redisRequest).get() };
		std::vector<std::string_view> &resultVec{ std::get<4>(*redisRequest).get() };
		std::vector<unsigned int> &resultNumVec{ std::get<5>(*redisRequest).get() };


		command.clear();
		commandSize.clear();
		resultVec.clear();
		resultNumVec.clear();

		int sessionNeedLen{ STATICSTRING::sessionLen + 1 + userNameLen },
			sessionExpireNeedLen{ STATICSTRING::sessionExpireLen + 1 + userNameLen };
		int needLen{ sessionNeedLen + sessionExpireNeedLen };
		char *buffer{ m_charMemoryPool.getMemory(needLen) };

		if (!buffer)
		{
			m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpSTDException), m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
			return VerifyFlag::verifyFail;
		}

		char *bufferBegin{ buffer }, *sessionBegin{ buffer }, *sessionExpireBegin{};


		std::copy(STATICSTRING::session, STATICSTRING::session + STATICSTRING::sessionLen, bufferBegin);
		bufferBegin += STATICSTRING::sessionLen;
		*bufferBegin++ = ':';
		std::copy(userNameBegin, userNameEnd, bufferBegin);
		bufferBegin += userNameLen;

		sessionExpireBegin = bufferBegin;

		std::copy(STATICSTRING::sessionExpire, STATICSTRING::sessionExpire + STATICSTRING::sessionExpireLen, bufferBegin);
		bufferBegin += STATICSTRING::sessionExpireLen;
		*bufferBegin++ = ':';
		std::copy(userNameBegin, userNameEnd, bufferBegin);
		bufferBegin += userNameLen;


		try
		{
			command.emplace_back(std::string_view(STATICSTRING::hmget, STATICSTRING::hmgetLen));
			command.emplace_back(std::string_view(STATICSTRING::user, STATICSTRING::userLen));
			command.emplace_back(std::string_view(sessionBegin, sessionNeedLen));
			command.emplace_back(std::string_view(sessionExpireBegin, sessionExpireNeedLen));
			commandSize.emplace_back(4);

			std::get<1>(*redisRequest) = 1;
			std::get<3>(*redisRequest) = 1;
			if (std::is_same<VERIFYTYPE, VERIFYINTERFACE>::value)
			{
				std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::verifySessionBackWard, this, std::placeholders::_1, std::placeholders::_2);
			}
			else if (std::is_same<VERIFYTYPE, VERIFYHTTP>::value)
			{
				std::get<6>(*redisRequest) = std::bind(&HTTPSERVICE::verifySessionBackWardHttp, this, std::placeholders::_1, std::placeholders::_2);
			}

			if (!m_multiRedisReadSlave->insertRedisRequest(redisRequest))
			{
				m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpFailToInsertRedis), m_sendLen = HTTPRESPONSEREADY::httpFailToInsertRedisLen;
				return VerifyFlag::verifyFail;
			}
		}
		catch (const std::exception &e)
		{
			m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::httpSTDException), m_sendLen = HTTPRESPONSEREADY::httpSTDExceptionLen;
			return VerifyFlag::verifyFail;
		}

		return VerifyFlag::verifySession;

	}


	return verifyOK;
}






