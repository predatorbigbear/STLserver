#pragma once


#include "publicHeader.h"
#include "mysql.h"
#include "mysqld_error.h"



//业务层使用vector<const char**>来装载存储指针，省却自动管理烦恼

//框架层继续传参，但使用string来管理数据，关键点采用指针返回操作


//  设置标志位，记录sql执行状态


//  插入待投递消息队列时，判断sql执行状态，如果sql没有执行，则置位执行（如果本次执行的条数超过限定条数，则直接报错返回），否则插入待投递字符串中，累加条数（记得检查累加条数不能超过限定的条数）


//  如果累加后的条数超过限定条数则不进行累加操作


//  



//



struct MULTISQLREAD
{
	/*
	执行语句指针

	如果采用const char*或者string 装载实际字符串信息的类型作为参数，易用性可读性确实很好，但是会损耗一定性能，原因是在命令拼接过程中不可避免会出现扩容拷贝情况，而如果使用二级指针，虽然也会扩容拷贝，
	但是可以将拷贝过程损耗降到极低，让真正的字符串拷贝只在发送请求准备过程中进行一次


	指针个数 （实际个数=指针个数/2  因为包含首尾位置，如果是多句中间需要包含;）
	执行语句数量
	结果集指针
	结果行数 列数int*    每2个存储一个结果集的行数列数
	结果行数 列数buffer大小
	结果指针，存储每一个的结果指针
	结果指针buffer大小
	回调函数
	*/

	// 

	using resultType = std::tuple<const char**, unsigned int, unsigned int, std::shared_ptr<MYSQL_RES*>, unsigned int, std::shared_ptr<unsigned int[]>, unsigned int, 
		std::shared_ptr<const char*[]>, unsigned int,std::function<void(bool, enum ERRORMESSAGE)>>;


	using resultTypeRW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>> ,
		std::reference_wrapper<std::vector<unsigned int>> , std::reference_wrapper<std::vector<std::string_view>> , std::function<void(bool, enum ERRORMESSAGE)> >;


	MULTISQLREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT,const unsigned int commandMaxSize);

	
	std::shared_ptr<std::function<void(MYSQL_RES **, const unsigned int)>> sqlRelease();



	bool insertSqlRequest(std::shared_ptr<resultType> sqlRequest);


	// 释放数据库数据
	void FreeResult(MYSQL_RES ** resultPtr, const unsigned int resultLen);


	~MULTISQLREAD();





private:
	


	MYSQL *m_mysql;

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};
	
	const char *m_unix_socket{};


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//std::string m_message;                     //联合发送命令字符串
	
	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};
	
	unsigned int m_messageBufferNowSize{};



	unsigned int m_commandMaxSize{};        //命令个数最大大小（长度）

	unsigned int m_commandNowSize{};        //命令个数实际大小（长度）
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const char *m_sendMessage{};

	unsigned int m_sendLen{};


	bool resetConnect{ false };

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::atomic<bool> m_hasConnect{ false };         //是否已经处于连接状态
	std::atomic<bool>m_queryStatus{ false };         //是否正在进行查询工作
	




	net_async_status m_status;
	std::unique_ptr<boost::asio::steady_timer> m_timer;
	std::shared_ptr<std::function<void()>>m_unlockFun;


	//等待释放结果集的指针，结果集个数
	std::shared_ptr<std::function<void(MYSQL_RES **,const unsigned int)>>m_freeSqlResultFun;


	

	
	///////////////////////////////////////////////////////////////////
	//待投递队列，未/等待拼凑消息的队列
	SAFELIST<std::shared_ptr<resultType>>m_messageList;



	//待获取结果队列，已经发送请求的队列，处理时使用m_waitMessageList进行处理，减少使用m_messageList的锁的机会
	std::unique_ptr<std::shared_ptr<resultType>[]>m_waitMessageList;

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};



	///////////////////////////////////////////////////////////

	/*
	执行语句指针
	指针个数 （实际个数=指针个数/2  因为包含首尾位置，如果是多句中间需要包含;）
	执行语句数量
	结果集指针
	结果行数 列数int*    每2个存储一个结果集的行数列数
	结果行数 列数buffer大小
	结果指针，存储每一个的结果指针
	结果指针buffer大小
	回调函数
	*/

	std::shared_ptr<resultType> m_sqlRequest{};


	//////////////////////////////////////////////////////



	std::unique_ptr<MYSQL_RES*[]>m_resultArray{};        //  存储MYSQL RES 结果集
	unsigned int m_resultMaxSize{};                         //  buffer空间大小
	unsigned int m_resultNowSize{};





	std::unique_ptr<unsigned int[]>m_rowField{};         //  存储每一个对应结果集的row和field
	unsigned int m_rowFieldMaxSize{};
	unsigned int m_rowFieldNowSize{};                  //本次row field存储占据的数目


	std::vector<const char *>m_dataVec;       //   存储每一个对应结果集的字符串数据

	/////////////////////////////////////////////////////////

	MYSQL_FIELD *fd;    //字段列数组
	MYSQL_RES *m_result{ nullptr };    //行的一个查询结果集
	MYSQL_ROW m_row;   //数据行的列

	unsigned int m_rowNum{};
	unsigned int m_rowCount{};
	unsigned int m_fieldNum{};
	const char **m_data{};
	unsigned long *m_lengths{};


	unsigned int m_temp{};


private:
	void FreeConnect();


	void ConnectDatabase();


	void ConnectDatabaseLoop();


	void firstQuery();


	void handleAsyncError();


	void store();


	void next_result();


	void checkRowField();


	void fetch_row();


	void multiGetResult();

	//将m_dataArray清空
	void clearDataArray();


	//检查m_messageList是否有需要拼装处理的消息
	void makeMessage();


	void clearWaitMessageList();


	void clearMessageList(const ERRORMESSAGE value);

	//  每次重新组装消息时候，重置复用的变量  
	void readyMakeMessage();
};