#pragma once

#include "publicHeader.h"
#include "MyReq.h"
#include "STLtree.h"


//原本云端工程最麻烦就是tcp与http之间使用共享缓存，而缓存在中间过程中学校离线的时候有些数据会清空，现暂时不清空

struct cloudHttp
{
	cloudHttp(
		shared_ptr<io_context>ioc,
		shared_ptr<boost::asio::ip::tcp::socket> socket,
		std::shared_ptr<std::string const> const doc_root,
		shared_ptr<string>fileBuf,
		shared_ptr<mutex>m1
	);











private:
/////////////////////////////////////////////////////////私有变量部分
	shared_ptr<io_context>m_ioc;
	shared_ptr<boost::asio::ip::tcp::socket>m_sock;
	shared_ptr<steady_timer>m_timer;

	unique_ptr<char[]> m_readBuf;
	static const int m_maxReadLen{ 65535 };
	int m_readLen{};
	//int m_leftReadLen{};


	std::shared_ptr<std::string const> m_doc_root;


	MYREQ m_req;
	list<MYREQ>m_MYREQVec;

	atomic<bool>m_startRead;
	atomic<bool>m_startWrite;
	atomic<bool>m_requestInvaild;
	atomic<bool>m_startCheck;
	string m_readStr;

	stringstream m_sstr;
	string response;
	string errorMessage;

	vector<string>::iterator vecIter;
	static const bool openUrl{ true };
	bool m_success{};
	bool m_innerSuccess{};

	shared_ptr<string>m_fileBuf;
	shared_ptr<mutex> m_m1;
	shared_ptr<boost::asio::io_context::strand>m_strand;

	list<string>m_sendMessage;
	shared_ptr<boost::asio::io_context::strand>m_sendStrand;


    //////////////////////////////////////////////////////////私有函数部分
	void run();

	void checkTimeOut();

	void do_read();

	void on_read(const boost::system::error_code &ec);

	void parseHttp();      //解析http数据

	void send_bad_response(const std::string& error);      //发送错误应答消息

	void getReq();     //批量处理请求

	void handle_request(const MYREQ &req);               //处理请求

	void checkOnDelete();    //清理资源

	void waitForSocket();

	void checkuserOffLine();

	void deletesocketSignal();

	void userOffLine();

	void waitForClose();

	void deleteSocket();

	void send_file(const std::string& target);         //发送文件

	void process_interface();   //处理接口函数

	

	void setErrorMessage(const char* ch);    //设置错误信息

	void sendResponse(const string &sendMessage, int sendMode = 0);    //发送应答消息

	void sendLoop();

	void innerSendLoop();

	void test();
};