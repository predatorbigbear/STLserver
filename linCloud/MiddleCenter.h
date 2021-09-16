#pragma once


#include "publicHeader.h"
#include "listener.h"
#include "LOG.h"
#include "logPool.h"
#include "multiSqlReadSWPool.h"
#include "multiRedisReadPool.h"
#include "multiRedisWritePool.h"
#include "multiSqlWriteSWPool.h"
#include "STLTimeWheel.h"


struct MiddleCenter
{
	MiddleCenter();


	void setKeyPlace(const char *publicKey, const char *privateKey);


	// ���д���ļ��Ķ�ʱʱ��
	// ��ʱ�洢��־���ݵ�buffer�ռ��С
	// log����Ԫ�ش�С
	void setLog(const char *logFileName, std::shared_ptr<IOcontextPool> ioPool, const int overTime = 60 ,const int bufferSize = 20480, const int bufferNum = 16);

	// Ĭ����ҳ�ļ�Ŀ¼
	// http�������Ԫ�ظ���
	// ��ʱʱ��
	void setHTTPServer(std::shared_ptr<IOcontextPool> ioPool, const std::string &tcpAddress, const std::string &doc_root , const int socketNum , const int timeOut);

	// �Ƿ�����redis��
	// ����redis��������
	// ���redis���ݵ�buffer��С
	// ��Żػ�����¿�Խ�ڴ���β����buffer�Ĵ�С
	// ���һ���Դ���������С
	void setMultiRedisRead(std::shared_ptr<IOcontextPool> ioPool, const std::string &redisIP, const unsigned int redisPort, const bool isMaster = true, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 65535000, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	void setMultiRedisWrite(std::shared_ptr<IOcontextPool> ioPool, const std::string &redisIP, const unsigned int redisPort, const bool isMaster = true, const unsigned int bufferNum = 1,
		const unsigned int memorySize = 65535000, const unsigned int outRangeMaxSize = 65535, const unsigned int commandSize = 50
	);


	// 
	// 
	// SQL����
	// SQL  DB
	// �Ƿ�����sql
	// ���һ���Դ���������С
	// ����sql��������
	void setMultiSqlReadSW(std::shared_ptr<IOcontextPool> ioPool, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const bool isMaster = true, const unsigned int commandMaxSize = 50, const int bufferNum = 4);



	void setMultiSqlWriteSW(std::shared_ptr<IOcontextPool> ioPool, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const bool isMaster = true, const unsigned int commandMaxSize = 50, const int bufferNum = 4);


	void unlock();

	//����������ϸ��STLTimeWheel.h��˵��
	void setTimeWheel(std::shared_ptr<IOcontextPool> ioPool, const unsigned int checkSecond = 1, const unsigned int wheelNum = 60, const unsigned int everySecondFunctionNumber = 1000);


private:
	std::unique_ptr<listener>m_listener;

	bool m_hasSetLog{ false };
	bool m_hasSetListener{false};

	std::shared_ptr<LOGPOOL>m_logPool{};
	std::shared_ptr<function<void()>>m_unlockFun{};

	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolSlave{};            //   �����ݿ��ȡ���ӳ�
	std::shared_ptr<MULTISQLWRITESWPOOL>m_multiSqlWriteSWPoolMaster{};         //   �����ݿ�д�����ӳ�
	std::shared_ptr<MULTISQLREADSWPOOL>m_multiSqlReadSWPoolMaster{};           //   �����ݿ��ȡ���ӳ�

	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolSlave{};            //   ��redis��ȡ���ӳ�
	std::shared_ptr<MULTIREDISWRITEPOOL>m_multiRedisWritePoolMaster{};         //   ��redisд�����ӳ�
	std::shared_ptr<MULTIREDISREADPOOL>m_multiRedisReadPoolMaster{};           //   ��redis��ȡ���ӳ�

	std::shared_ptr<STLTimeWheel>m_timeWheel{};                                 //ʱ���ֶ�ʱ��


	std::mutex m_mutex;

	char *m_publicKeyBuffer{};
	char *m_privateKeyBuffer{};

	char *m_publicKeyBegin{};
	char *m_publicKeyEnd{};
	int m_publicKeyLen{};


	char *m_privateKeyBegin{};
	char *m_privateKeyEnd{};
	int m_privateKeyLen{};

	RSA* m_rsaPublic{};
	RSA* m_rsaPrivate{};

	//����ʱ���������õ�checkSecond��
	//������socket�ļ��ʱ��Ϊ
	//if(!(��ʱ���ʱ��%checkSecond))
	//      turnNum= ��ʱ���ʱ�� / checkSecond
	//����turnNum= ��ʱ���ʱ�� / checkSecond + 1
	unsigned int m_checkSecond{};

};
