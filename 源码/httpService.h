#pragma once


#include "publicHeader.h"
#include "readBuffer.h"
#include "LOG.h"
#include "multiSQLREADSW.h"
#include "sqlCommand.h"
#include "mysql/mysql.h"
#include "httpinterface.h"
#include "multiRedisRead.h"
#include "multiRedisWrite.h"
#include "STLtreeFast.h"
#include "randomParse.h"
#include "multiSQLWriteSW.h"
#include "STLTimeWheel.h"


#include<ctype.h>


struct HTTPSERVICE
{
	HTTPSERVICE(std::shared_ptr<io_context> ioc, std::shared_ptr<LOG> log, const std::string &doc_root,
		std::shared_ptr<MULTISQLREADSW>multiSqlReadSWMaster,
		std::shared_ptr<MULTIREDISREAD>multiRedisReadMaster,
		std::shared_ptr<MULTIREDISWRITE>multiRedisWriteMaster, std::shared_ptr<MULTISQLWRITESW>multiSqlWriteSWMaster,
		std::shared_ptr<STLTimeWheel> timeWheel,
		const std::shared_ptr<std::unordered_map<std::string_view, std::string>>fileMap,
		const unsigned int timeOut, bool & success, const unsigned int bufNum = 4096
		);

	void setReady(const int index, std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>)>>clearFunction, std::shared_ptr<HTTPSERVICE> other);

	std::shared_ptr<HTTPSERVICE> *&getListIter();

	bool checkTimeOut();

	void clean();

	std::unique_ptr<ReadBuffer> m_buffer{};





private:


	const std::string &m_doc_root;

	std::shared_ptr<io_context> m_ioc{};

	std::unique_ptr<boost::asio::steady_timer> m_timer{};

	int m_index;

	int m_len{};

	std::shared_ptr<std::function<void(std::shared_ptr<HTTPSERVICE>)>>m_clearFunction{};

	std::shared_ptr<LOG> m_log{};

	std::shared_ptr<HTTPSERVICE> *mySelfIter{};

	std::shared_ptr<HTTPSERVICE> m_mySelf{};

	std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //ʱ���ֶ�ʱ��

	const std::shared_ptr<std::unordered_map<std::string_view, std::string>>m_fileMap{};

	std::chrono::_V2::system_clock::time_point m_time{ std::chrono::high_resolution_clock::now() };

	std::atomic<bool>m_hasClean{ false };


	using resultType = std::tuple<const char**, unsigned int, unsigned int, std::shared_ptr<MYSQL_RES*>, unsigned int, std::shared_ptr<unsigned int[]>, unsigned int,
		std::shared_ptr<const char*[]>, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>>;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	using resultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>>,
		std::reference_wrapper<std::vector<unsigned int>>, std::reference_wrapper<std::vector<std::string_view>>, std::function<void(bool, enum ERRORMESSAGE)> >;

	/*
	ִ������string_view��
	ִ���������
	ÿ������Ĵ���������������redis RESP����ƴ�ӣ�
	��ȡ�������  ����Ϊ����һЩ����������ܲ�һ���н�����أ�

	���ؽ��string_view
	ÿ������Ĵ������

	�ص�����
	*/
	// redis����
	using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>>;



	/*
	ִ������string_view��
	ִ���������
	ÿ������Ĵ���������������redis RESP����ƴ�ӣ�
	����ֱ��ƴ���ַ�������string�У�����ǰ����ȡ�����ݽ��з���

	���ص���ȥɾ��������־

	���ü����־��д��ʱ��Ȼ����ͨ����ָ��ķ�ʽ���������ط�bufffer�е�Դ���ݣ�����������ƴ�Ӻ�Ž��лص�����ɾ��
	*/
	// redisд�� ��������
	using redisWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>>;


	//ÿ��ʹ��ǰ�Ƚ�����ղ���
	std::vector<std::vector<std::string_view>>m_stringViewVec{};     //stringView����vec

	std::vector<std::vector<MYSQL_RES*>>m_mysqlResVec;             //mysql_Res* �����vec

	std::vector<std::vector<unsigned int>>m_unsignedIntVec;        //unsigned int �����vector

	std::vector<STLtreeFast>m_STLtreeFastVec;


	//  ���ڻ�ȡchar��̬������ڴ��   ���ͽ�����������ȡ�ڴ��
	MEMORYPOOL<> m_MemoryPool;



	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::vector<std::shared_ptr<resultTypeSW>> m_multiSqlRequestSWVec;


	std::vector<std::shared_ptr<redisResultTypeSW>> m_multiRedisRequestSWVec;


	std::vector<std::shared_ptr<redisWriteTypeSW>> m_multiRedisWriteSWVec;



	


	std::shared_ptr<MULTISQLREADSW>m_multiSqlReadSWMaster{};

	std::shared_ptr<MULTIREDISREAD>m_multiRedisReadMaster{};
		
	std::shared_ptr<MULTIREDISWRITE>m_multiRedisWriteMaster{};
	
	std::shared_ptr<MULTISQLWRITESW>m_multiSqlWriteSWMaster{};



	unsigned int m_timeOut;

	unsigned int m_rowNum{};

	unsigned int m_fieldNum{};

	unsigned int m_tempLen{};

	unsigned int m_chunkLen{};

	int m_startPos{};

	//Ϊ������ܣ�����Ķ�ȡbufferӦ������װ�ش󲿷ְ���body���ֵĳ��ȣ������󲿷�����½���ָ��ָ��
	//�ϴ��ļ���Ҫ��Ӵ��봦���������post ���body���ֽ����Ż�
	int m_maxReadLen{ };

	int m_defaultReadLen{ };

	int m_parseStatus{ PARSERESULT::complete };


	/////////////////////////////////////////

	std::unique_ptr<char[]> m_SessionID;

	int m_sessionMemory{};

	int m_sessionLen{};
	//����sessionID�Ƿ�Ϊempty�жϴ���cookieʱӦ��ִ�еĲ���


	char *m_readBuffer{};

	const char *funBegin{}, *funEnd{}, *finalFunBegin{}, *finalFunEnd{},
		*targetBegin{}, *targetEnd{}, *finalTargetBegin{}, *finalTargetEnd{},
		*paraBegin{}, *paraEnd{}, *finalParaBegin{}, *finalParaEnd{},
		*bodyBegin{ }, *bodyEnd{ }, *finalBodyBegin{}, *finalBodyEnd{},
		*versionBegin{}, *versionEnd{}, *finalVersionBegin{}, *finalVersionEnd{},
		*headBegin{}, *headEnd{}, *finalHeadBegin{}, *finalHeadEnd{},
		*wordBegin{}, *wordEnd{}, *finalWordBegin{}, *finalWordEnd{},
		*httpBegin{}, *httpEnd{}, *finalHttpBegin{}, *finalHttpEnd{},
		*lineBegin{}, *lineEnd{}, *finalLineBegin{}, *finalLineEnd{},
		*chunkNumBegin{}, *chunkNumEnd{}, *finalChunkNumBegin{}, *finalChunkNumEnd{},
		*chunkDataBegin{}, *chunkDataEnd{}, *finalChunkDataBegin{}, *finalChunkDataEnd{},
		*boundaryBegin{}, *boundaryEnd{},
		*findBoundaryBegin{}, *findBoundaryEnd{},
		*boundaryHeaderBegin{}, *boundaryHeaderEnd{},
		*boundaryWordBegin{}, *boundaryWordEnd{}, *thisBoundaryWordBegin{}, *thisBoundaryWordEnd{},
		*messageBegin{}, *messageEnd{}
		;

	bool hasBody{ false };

	bool hasPara{ false };

	bool hasChunk{ false };                    //�Ƿ���Chunk�����ʽ

	bool hasCompress{ false };                //�Ƿ�֧��LZWѹ��

	bool hasDeflate{ false };                 //�Ƿ�֧��zlibѹ��

	bool hasGzip{ false };                    //�Ƿ�֧��LZ77ѹ��

	bool hasIdentity{ true };                 //�Ƿ񲻽����κ�ѹ����ֿ飬����ԭʼ����

	bool expect_continue{ false };

	bool hasJson{ false };                     //�����ʽ�Ƿ���json

	bool hasXml{ false };                      //�����ʽ�Ƿ���XML

	bool hasX_www_form_urlencoded{ false };         //�����ʽ�Ƿ���x-www-form-urlencoded

	bool hasMultipart_form_data{ false };          //�����ʽ�Ƿ���multipart/form-data


	bool keep_alive{ true };                     // Connectionֵ   Ĭ��Ϊtrue��������close֮�������Ϊfalse

	std::unique_ptr<const char*[]>m_httpHeaderMap{};

	bool isHttp10{ false };                   //�Ƿ���http1.0

	bool isHttp11{ false };                   //�Ƿ���http1.1

	const bool isHttp{ true };                //�Ƿ���http

	int hostPort{80};                         //http hostĬ�϶˿�



	///////////////////////////////////////////////////////////////////////////


	//��Ϊhttp״̬λ���ޣ�������������ָ��ָ���Ӧλ�ã��ڳ�ʼ��ʱ���úã��Ժ�ֱ�ӻ�ȡ����ǿ����ϣ����
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


	const char** m_HostNameBegin{}, ** m_HostNameEnd{};
	const char** m_HostPortBegin{}, ** m_HostPortEnd{};





	size_t m_availableLen{};

	int m_boundaryLen{};

	int m_bodyLen{};

	int m_boundaryHeaderPos{};
	///////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::vector<std::pair<const char*, const char*>>m_dataBufferVec;     //�洢��ɢ��ĳ�����ݵ���ʱvector    


	unsigned int accumulateLen{};          //�ۻ�����



	//�ļ���С
	int m_fileSize{};
	int m_readFileSize{};
	
	std::ifstream m_file{};

	int m_getStatus{};


	std::string_view m_message{};

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

	std::chrono::system_clock::time_point m_sessionClock, m_sessionGMTClock ,m_thisClock;       //����ȡֵ��ǰʱ���

	time_t m_sessionTime, m_sessionGMTTime , m_thisTime;

	time_t m_firstTime{};         //�״β���ʱ���

	struct tm *m_tmGMT;


	const char *m_sendBuffer{};
	unsigned int m_sendLen{};
	unsigned int m_sendTimes{};

private:

	void shutdownLoop();

	void cancelLoop();

	void closeLoop();

	void resetSocket();

	//��ÿ�η�����Ϻ�������Դ
	void recoverMemory();

	void sendOK();

	void cleanData();

	// ִ��Ԥ������
	void prepare();

	void startRead();

	void parseReadData(const char *source, const int size);         // ��������

	int parseHttp(const char *source, const int size);             // ����Http����

	bool parseHttpHeader();                                        //����http �ֶ�����

	// ��ͨģʽ
	// READFROMDISK ֱ�ӴӴ��̶�ȡ�ļ�����ģʽ
	template<typename SENDMODE = void>
	void startWrite(const char *source, const int size);


	// ��ͨģʽ
	// READFROMDISK ֱ�ӴӴ��̶�ȡ�ļ�����ģʽ
	template<typename SENDMODE = void>
	void startWriteLoop(const char *source, const int size);


	template<typename T=void, typename HTTPFLAG = void, typename HTTPFUNCTION=void*>
	void makeSendJson(STLtreeFast &st, HTTPFUNCTION httpWrite=nullptr, unsigned int httpLen=0);


	//�ļ�����
	//�ļ�����
	//����bufferԤ����ռ�
	//����bufferװ�ط��ص�ַ
	//����buffer httpǰ׺��С
	//
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool makeFileFront(std::string_view fileName, const unsigned int fileLen, const unsigned int assignLen, char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, ARG&&...args);
	

	//����������workflow�����ļ���ͬ�ķ������ݵ�ר�ô������
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool make_compareWithWorkFlowResPonse(char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, unsigned int randomBodyLen, ARG&&...args);

	void run();

	void checkMethod();

	void switchPOSTInterface();



	void testBody();

	void testRandomBody();

	void testGet();     //��ȡ�ļ�����

	///////////////////////////////////////////////////

	void testPingPong();

	//��json��ʽ����ping pong��Ϣ
	void testPingPongJson();


	void handleERRORMESSAGE(ERRORMESSAGE em);


	//  ����sql string_view�汾 ��ѯ����
	void testmultiSqlReadSW();

	void handleMultiSqlReadSW(bool result, ERRORMESSAGE em);


	//  ����redis string_view�汾 ��ѯ����KEY���Ժ���
	void testMultiRedisReadLOT_SIZE_STRING();

	void handleMultiRedisReadLOT_SIZE_STRING(bool result, ERRORMESSAGE em);


	//  ����redis string_view�汾 ��body�н����������в�ѯKEY���Ժ���
	void testMultiRedisParseBodyReadLOT_SIZE_STRING();


	//  ����redis string_view�汾 ��ѯ������ֵ���Ժ���
	void testMultiRedisReadINTERGER();

	void handleMultiRedisReadINTERGER(bool result, ERRORMESSAGE em);


	//  ����redis string_view�汾 ��ѯ������ֵ���Ժ���
	void testMultiRedisReadARRAY();

	void handleMultiRedisReadARRAY(bool result, ERRORMESSAGE em);






	//�ⶨ�״�ʱ���
	void testFirstTime();



	//////////////////////////////////////////////////////////////////////





	void testBusiness();


	void readyParseMultiPartFormData();

	//�����ϴ��ļ�
	void testMultiPartFormData();


	void testSuccessUpload();


	//���Խ���chunk��ʽ����
	void readyParseChunkData();

	//json��ʽ������ʾ
	void testMakeJson();





	//��workflow���жԱȲ��Եĺ���
	//https://github.com/sogou/workflow/tree/master/benchmark
	//workflow�����󷵻�ÿ�ε�ʱ��  body����  �Լ�Connection  Conteng-type����
	//string str{ "POST /20 HTTP/1.1\r\nHost: 101.32.203.226:8085\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 12\r\n\r\nstringlen=11" };
	void testCompareWorkFlow();








	/// ------------------------------------------------------

	std::function<void()>m_business;
	
	// ��֤����ר��buf����粻Ҫʹ��
	std::unique_ptr<const char*[]>m_verifyData;

	void resetVerifyData();


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
}



template<typename SENDMODE>
inline void HTTPSERVICE::startWriteLoop(const char * source, const int size)
{
	boost::asio::async_write(*m_buffer->getSock(), boost::asio::buffer(source, size), [this](const boost::system::error_code &err, std::size_t size)
	{
		if (err)
		{
			//��ʱʱclean���������cancel,����operation_aborted����  �޸���������ʱ���ᴥ�����յ����
			if (err != boost::asio::error::operation_aborted)
			{
				m_hasClean.store(false);
			}
			
			//��������ʱ�ȴ���ʱ���գ�clean�����ڻ���ڴ�ؽ�������
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
					//��Ϊ������ڴ����ܲ���Ĭ�϶�ȡ�ڴ�飬���Ի��ղ���Ӧ����copy����֮��Ž���
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
					//default��������Ƿ��Լ����ڴ������������
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
inline void HTTPSERVICE::makeSendJson(STLtreeFast & st, HTTPFUNCTION httpWrite, unsigned int httpLen)
{
	if (!st.isEmpty())
	{
		char *sendBuffer{};
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
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
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
				startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
			}
		}
	}
	else
	{
		startWrite(HTTPRESPONSEREADY::httpSTDException, HTTPRESPONSEREADY::httpSTDExceptionLen);
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

	//���Ԥ����ռ���ǰ׺�ռ䶼����װ�أ��򷵻�ʧ��
	if (assignLen < needFrontLen)
		return false;

	char *newResultPtr{ m_MemoryPool.getMemory<char*>(assignLen) };

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


	char *newResultPtr{ m_MemoryPool.getMemory<char*>(needLen) };

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













