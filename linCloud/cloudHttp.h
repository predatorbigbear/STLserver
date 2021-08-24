#pragma once

#include "publicHeader.h"
#include "MyReq.h"
#include "STLtree.h"


//ԭ���ƶ˹������鷳����tcp��http֮��ʹ�ù����棬���������м������ѧУ���ߵ�ʱ����Щ���ݻ���գ�����ʱ�����

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
/////////////////////////////////////////////////////////˽�б�������
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


    //////////////////////////////////////////////////////////˽�к�������
	void run();

	void checkTimeOut();

	void do_read();

	void on_read(const boost::system::error_code &ec);

	void parseHttp();      //����http����

	void send_bad_response(const std::string& error);      //���ʹ���Ӧ����Ϣ

	void getReq();     //������������

	void handle_request(const MYREQ &req);               //��������

	void checkOnDelete();    //������Դ

	void waitForSocket();

	void checkuserOffLine();

	void deletesocketSignal();

	void userOffLine();

	void waitForClose();

	void deleteSocket();

	void send_file(const std::string& target);         //�����ļ�

	void process_interface();   //����ӿں���

	

	void setErrorMessage(const char* ch);    //���ô�����Ϣ

	void sendResponse(const string &sendMessage, int sendMode = 0);    //����Ӧ����Ϣ

	void sendLoop();

	void innerSendLoop();

	void test();
};