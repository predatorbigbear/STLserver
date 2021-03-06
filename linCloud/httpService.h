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
	HTTPSERVICE(std::shared_ptr<io_context> ioc, std::shared_ptr<LOG> log, const std::string &doc_root,
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

	const std::string &m_doc_root;

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
	????????string_view??
	????????????
	????????????????????????????redis RESP??????????
	????????????  ????????????????????????????????????????????

	????????string_view
	??????????????????

	????????
	*/
	// redis????
	using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>>;



	/*
	????????string_view??
	????????????
	????????????????????????????redis RESP??????????
	??????????????????????string??????????????????????????????

	????????????????????????

	????????????????????????????????????????????????????????bufffer??????????????????????????????????????????????
	*/
	// redis???? ????????
	using redisWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>>;


	//????????????????????????
	std::vector<std::vector<std::string_view>>m_stringViewVec{};     //stringView????vec

	std::vector<std::vector<MYSQL_RES*>>m_mysqlResVec;             //mysql_Res* ??????vec

	std::vector<std::vector<unsigned int>>m_unsignedIntVec;        //unsigned int ??????vector

	std::vector<STLtreeFast>m_STLtreeFastVec;


	//  ????????char????????????????   ??????????????????????????
	MEMORYPOOL<char> m_charMemoryPool;

	// ????char*????????????????
	MEMORYPOOL<char*> m_charPointerMemoryPool;


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

	//??????????????????????buffer??????????????????????body??????????????????????????????????????????
	//??????????????????????????????????post ????body????????????
	int m_maxReadLen{ 1024 };

	int m_defaultReadLen{ 1024 };

	int m_parseStatus{ PARSERESULT::complete };


	/////////////////////////////////////////

	std::unique_ptr<char[]> m_SessionID;

	int m_sessionMemory{};

	int m_sessionLen{};
	//????sessionID??????empty????????cookie????????????????


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

	bool expect_continue{ false };

	bool keep_alive{ false };

	std::unique_ptr<const char*[]>m_httpHeaderMap{};


	//????http????????????????????????????????????????????????????????????????????????????????????????
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


	std::vector<std::pair<const char*, const char*>>m_dataBufferVec;     //????????????????????????vector    


	unsigned int m_accumulateLen{};          //????????


	char *m_publicKeyBegin{};
	char *m_publicKeyEnd{};
	int m_publicKeyLen{};


	char *m_privateKeyBegin{};
	char *m_privateKeyEnd{};
	int m_privateKeyLen{};


	RSA *m_rsaPublic{};               //??????????
	RSA *m_rsaPrivate{};

	BIO *m_bio{};
	RSA *m_rsaClientPublic{};         //??????????
	int m_rsaClientSize{};            //?????? RSA_size


	MD5_CTX m_ctx;


	bool m_hasMakeRandom{ false };          // ????????????????????

	std::unique_ptr<char[]>m_clientAES{};  // ??????????AES??????????????

	//16  24  32
	int m_clientAESLen{};                   // ??????AES????????


	AES_KEY aes_encryptkey;                  //????AES
	AES_KEY aes_decryptkey;                  //????AES


	//????????
	int m_fileSize{};
	int m_readFileSize{};
	
	std::ifstream m_file{};

	int m_getStatus{};


	std::string_view m_message{};

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

		find_fileBoundaryBegin2,

		check_fileBoundaryEnd,

		check_fileBoundaryEnd2
	};



	char *m_randomString{};       // ????????????????????????????????????????

	std::chrono::system_clock::time_point m_sessionClock, m_sessionGMTClock ,m_thisClock;       //??????????????????

	time_t m_sessionTime, m_sessionGMTTime , m_thisTime;

	time_t m_firstTime{};         //??????????????

	struct tm *m_tmGMT;


	const char *m_sendBuffer{};
	unsigned int m_sendLen{};
	unsigned int m_sendTimes{};

private:

	//????????????????????????
	void recoverMemory();

	void sendOK();

	void cleanData();

	// ????????????
	void prepare();

	void startRead();

	void parseReadData(const char *source, const int size);         // ????????

	int parseHttp(const char *source, const int size);             // ????Http????

	// ????????
	// READFROMDISK ??????????????????????????
	template<typename SENDMODE = void>
	void startWrite(const char *source, const int size);


	// ????????
	// READFROMDISK ??????????????????????????
	template<typename SENDMODE = void>
	void startWriteLoop(const char *source, const int size);


	template<typename T=void, typename HTTPFLAG = void, typename ENCTYPT = void, typename HTTPFUNCTION=void*>
	void makeSendJson(STLtreeFast &st, HTTPFUNCTION httpWrite=nullptr, unsigned int httpLen=0, AES_KEY *enctyptKey=nullptr);


	//????????
	//????????
	//????buffer??????????
	//????buffer????????????
	//????buffer http????????
	//
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool makeFileFront(std::string_view fileName, const unsigned int fileLen, const unsigned int assignLen, char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, ARG&&...args);
	

	//??????????workflow????????????????????????????????????
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool make_compareWithWorkFlowResPonse(char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, unsigned int randomBodyLen, ARG&&...args);

	void run();

	void checkMethod();

	void switchPOSTInterface();



	void testBody();

	void testRandomBody();

	///////////////////////////////////////////////////

	void testPingPong();

	//??json????????ping pong????
	void testPingPongJson();


	void handleERRORMESSAGE(ERRORMESSAGE em);


	//  ????sql string_view???? ????????
	void testmultiSqlReadSW();

	void handleMultiSqlReadSW(bool result, ERRORMESSAGE em);


	//  ????redis string_view???? ????????KEY????????
	void testMultiRedisReadLOT_SIZE_STRING();

	void handleMultiRedisReadLOT_SIZE_STRING(bool result, ERRORMESSAGE em);


	//  ????redis string_view???? ????????????????????
	void testMultiRedisReadINTERGER();

	void handleMultiRedisReadINTERGER(bool result, ERRORMESSAGE em);


	//  ????redis string_view???? ????????????????????
	void testMultiRedisReadARRAY();

	void handleMultiRedisReadARRAY(bool result, ERRORMESSAGE em);



	void testGET();

	void handleTestGETFileLock(bool result, ERRORMESSAGE em);

	//????????????????
	void readyReadFileFromDisk(std::string_view fileName);

	//????????????????????????????????????  ???? ????????
	void ReadFileFromDiskLoop();


	//??????reids??????
	void readyReadFileFromRedis(std::string_view fileName);

	void handleReadFileFromRedis(bool result, ERRORMESSAGE em);

	void ReadFileFromRedisLoop();


	//????????????????
	bool makeFilePath(std::string_view fileName);

	//????????????
	bool getFileSize(std::string_view fileName);

	bool openFile(std::string_view fileName);

	//????http????????????????
	bool makeHttpFront(std::string_view fileName);

	//??????????
	void setFileLock(std::string_view fileName,const int fileSize);

	void handleSetFileLock(bool result, ERRORMESSAGE em);

	void handleTestGET(bool result, ERRORMESSAGE em);

	void readyGetFileFromDisk();

	
	void testLogin();

	void handleTestLoginRedisSlave(bool result, ERRORMESSAGE em);

	void handleTestLoginSqlSlave(bool result, ERRORMESSAGE em);


	// ????????,????session
	void testLogout();



	// https://blog.csdn.net/qq_36467463/article/details/78480136
	// ????????
	// ????????????????????????????????,
	void testGetPublicKey();


	//??????????????????????????????????????????????????????????????????????????????????????????????????????
	//????????????????????????????????
	//????????????????
	//??????????????????????????????????????????????????????????????????????????????????????????????
	//???? ?????????? publicKey   hashValue   randomString
	void testGetClientPublicKey();


	
	// 1 ????????????????????????????????????????
	// 2 ??????????????????????????AES??????????????????????????????????????????????AES??????????????????
	// 3 ????????????????????????????????????????????????????????

	// ??????????????????????????????AES??????????????????????????
	// ?????????????????????????????????????? ????????????????????AES??????????????????????????????????????????????????????????

	//??????????????
	void testFirstTime();


	// ??????????????AES????????????????????????????????????
	//AESKey0??????|   ??????????????
	void testGetEncryptKey();


	//////////////////////////////////////////////////////////////////////


	//   ????????????????????????????????  body??????AES????????????????????????????0??????

	//   ????????????????????????body????json????????????????????????????0??????????????????????????????????}????json????????????




	//  ??????????????????????HTTPS??????
	// ????????????????????????????????????AES????
	// ????????HTTP??????????????token
	// username password
	// ???????????????????????????? ????????????????md5??????????
	void testEncryptLogin();

	void handleTestLoginRedisSlaveEncrypt(bool result, ERRORMESSAGE em);

	void handleTestLoginSqlSlaveEncrypt(bool result, ERRORMESSAGE em);




	template<typename VERIFYTYPE = VERIFYINTERFACE>
	void testVerify();



	void testBusiness();


	void readyParseMultiPartFormData();

	//????????????
	void testMultiPartFormData();


	void testSuccessUpload();


	//????????chunk????????
	void readyParseChunkData();

	//json????????????
	void testMakeJson();





	//??workflow??????????????????
	//https://github.com/sogou/workflow/tree/master/benchmark
	//workflow????????????????????  body????  ????Connection  Conteng-type????
	//????????centos8  GCC10.2.0   
	//??????????????????????string??????????????????????????workflow????????????
	//????????????????????????????????????
	//workflow????????????????12  8085  11 
	//string str{ "POST /20 HTTP/1.1\r\nHost: 101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 12\r\n\r\nstringlen=11" };
	void testCompareWorkFlow();








	/// ------------------------------------------------------

	std::function<void()>m_business;
	
	// ????????????buf??????????????
	std::unique_ptr<const char*[]>m_verifyData;

	void resetVerifyData();

	bool verifyAES();

	template<typename VERIFYTYPE = VERIFYINTERFACE>
	int verifySession();

	void verifySessionBackWard(bool result, ERRORMESSAGE em);

	void verifySessionBackWardHttp(bool result, ERRORMESSAGE em);








	/////////////////////////////////////////////////////////////



	


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



template<typename SENDMODE>
inline void HTTPSERVICE::startWrite(const char * source, const int size)
{
	startWriteLoop<SENDMODE>(source, size);

	//????????????????????????????????????
	if constexpr (std::is_same<SENDMODE, void>::value)
	{
		if (m_parseStatus != PARSERESULT::check_messageComplete)
		{
			std::fill(m_httpHeaderMap.get(), m_httpHeaderMap.get() + HTTPHEADERSPACE::HTTPHEADERLIST::HTTPHEADERLEN, nullptr);
			m_charPointerMemoryPool.prepare();
		}
	}
}



template<typename SENDMODE>
inline void HTTPSERVICE::startWriteLoop(const char * source, const int size)
{
	boost::asio::async_write(*m_buffer->getSock(), boost::asio::buffer(source, size), [this](const boost::system::error_code &err, std::size_t size)
	{
		if (err)
		{
			m_log->writeLog(__FILE__, __LINE__, err.value(), err.message());
			//??????clean??????????cancel,????operation_aborted????  ????????????????????????????????
			if (err != boost::asio::error::operation_aborted)
			{
				m_lock.lock();
				m_hasClean = false;
				m_lock.unlock();
			}

			//????????????????????????????????????????
			if constexpr (std::is_same<SENDMODE, READFROMDISK>::value)
			{
				if (m_file.is_open())
					m_file.close();
			}
			
			//????????????????????????clean????????????????????????
		}
		else
		{
			if constexpr (std::is_same<SENDMODE, void>::value)
			{
				switch (m_parseStatus)
				{
				case PARSERESULT::complete:
					m_charMemoryPool.prepare();
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
					m_charMemoryPool.prepare();
					startRead();
					break;
				}
			}

			//????????????????
			if constexpr (std::is_same<SENDMODE, READFROMDISK>::value)
			{
				ReadFileFromDiskLoop();
			}

			if constexpr (std::is_same<SENDMODE, READFROMREDIS>::value)
			{
				//????????????
				++m_sendTimes;
				ReadFileFromRedisLoop();
			}
		}
	});
}



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




template<typename HTTPFLAG, typename HTTPFUNCTION, typename ...ARG>
inline bool HTTPSERVICE::makeFileFront(std::string_view fileName,const unsigned int fileLen, const unsigned int assignLen, char *& resultPtr, unsigned int & resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char * httpVersionBegin, const char * httpVersionEnd, const char * httpCodeBegin, const char * httpCodeEnd, const char * httpResultBegin, const char * httpResultEnd, ARG && ...args)
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

	//??????????????????????????????????????????????
	if (assignLen < needFrontLen)
		return false;

	char *newResultPtr{ m_charMemoryPool.getMemory(assignLen) };

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
inline bool HTTPSERVICE::make_compareWithWorkFlowResPonse(char *& resultPtr, unsigned int & resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, 
	const char * httpVersionBegin, const char * httpVersionEnd, const char * httpCodeBegin, const char * httpCodeEnd, const char * httpResultBegin, const char * httpResultEnd,
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


	char *newResultPtr{ m_charMemoryPool.getMemory(needLen) };

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
			m_sendBuffer = const_cast<char*>(HTTPRESPONSEREADY::http11invaild), m_sendLen = HTTPRESPONSEREADY::http11invaildLen;
			cleanData();
		}
	}


	switch (verifySession<VERIFYTYPE>())
	{
	case VerifyFlag::verifyOK:
		//??????????????
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

		// sessionID ????
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
			//  ????????????????cookie??????????????????????????????

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






