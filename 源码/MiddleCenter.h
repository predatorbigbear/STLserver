#pragma once


#include "listener.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "STLTimeWheel.h"
#include<unordered_map>
#include <zlib.h>

struct MiddleCenter
{
	MiddleCenter();

	~MiddleCenter();



	// ���д���ļ��Ķ�ʱʱ��
	// ��ʱ�洢��־���ݵ�buffer�ռ��С
	// log����Ԫ�ش�С
	void setLog(const char *logFileName, std::shared_ptr<IOcontextPool> ioPool, bool &success , const int overTime = 60 ,const int bufferSize = 80960, const int bufferNum = 1);

	// Ĭ����ҳ�ļ�Ŀ¼
	// http�������Ԫ�ظ���
	// ��ʱʱ��
	void setHTTPServer(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &tcpAddress, const std::string &doc_root ,
		std::vector<std::string> &&fileVec,
		const int socketNum , const int timeOut);

	// �Ƿ�����redis��
	// ����redis��������
	// ���redis���ݵ�buffer��С
	// ��Żػ�����¿�Խ�ڴ���β����buffer�Ĵ�С
	// ���һ���Դ���������С
	void setMultiRedisRead(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &redisIP, const unsigned int redisPort, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 65535000, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	void setMultiRedisWrite(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &redisIP, const unsigned int redisPort, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 65535000, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	// 
	// SQL����
	// SQL  DB
	// �Ƿ�����sql
	// ���һ���Դ���������С
	// ����sql��������
	void setMultiSqlReadSW(std::shared_ptr<IOcontextPool> ioPool, bool& success, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int commandMaxSize = 50, const int bufferNum = 1);



	void setMultiSqlWriteSW(std::shared_ptr<IOcontextPool> ioPool, bool &success , const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int commandMaxSize = 50, const int bufferNum = 1);


	void initMysql(bool& success);
	
	void freeMysql();

	void unlock();

	//����������ϸ��STLTimeWheel.h��˵��
	void setTimeWheel(std::shared_ptr<IOcontextPool> ioPool, bool &success , const unsigned int checkSecond = 1, const unsigned int wheelNum = 180, const unsigned int everySecondFunctionNumber = 100);


private:
	std::unique_ptr<listener>m_listener;

	bool m_hasSetLog{ false };
	bool m_hasSetListener{false};
	bool m_initSql{ false };

	
	std::shared_ptr<LOGPOOL>m_logPool{};                                        //��־��
	std::shared_ptr<function<void()>>m_unlockFun{};
	std::shared_ptr<std::unordered_map<std::string_view, std::string>>m_fileMap{};       //�ļ�����map������GET����ʱ��ȡ�ļ�ʹ��
	std::vector<std::string>m_fileVec{};               //�ļ���vector��ֻ��Ҫ����doc_rootĿ¼�µ��ļ������ɣ����������

	std::ifstream file;


	std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   �����ݿ�д�����ӳ�
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   �����ݿ��ȡ���ӳ�

	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   ��redisд�����ӳ�
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   ��redis��ȡ���ӳ�

	std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //ʱ���ֶ�ʱ��


	std::mutex m_mutex;


	//����ʱ���������õ�checkSecond��
	//������socket�ļ��ʱ��Ϊ
	//if(!(��ʱ���ʱ��%checkSecond))
	//      turnNum= ��ʱ���ʱ�� / checkSecond
	//����turnNum= ��ʱ���ʱ�� / checkSecond + 1
	unsigned int m_checkSecond{};


	z_stream zs;

	const unsigned int outbufferLen{ 8096 };
	char* outbuffer{ new char[outbufferLen] };

private:
	//��Ϊ������������ֻ���Ĭ��֧��gzip������������һ��ʼ���ļ���Դȫ������gzip��ߵȼ�ѹ��
	bool gzip(const char* source, const int sourLen, std::string& outPut);

};
