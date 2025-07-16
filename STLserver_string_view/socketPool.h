#pragma once



#include "readBuffer.h"
#include "LOG.h"

struct SocketPool
{
	SocketPool(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> startFunction , std::shared_ptr<std::function<void()>> starthttpServicePool , 
		std::shared_ptr<LOG> log , int beginSize=200,int newSize=50);

	void getNextBuffer(std::shared_ptr<ReadBuffer> & outBuffer);

	void getBackElem(const std::shared_ptr<ReadBuffer> &buffer);

	void setNotifyAccept();








private:
	std::shared_ptr<io_context> m_ioc;
	
	std::mutex m_listMutex;
	
	std::list<std::shared_ptr<ReadBuffer>> m_bufferList;
	std::list<std::shared_ptr<ReadBuffer>>::iterator m_iterNow;
	std::list<std::shared_ptr<ReadBuffer>>::iterator m_tempIter;

	int i{};
	int m_nowSize{};
	int m_temp{};
	int m_beginSize{};
	int m_newSize{};
	bool m_startAccept{ true };
	std::shared_ptr<std::function<void()>>m_startFunction;
	std::shared_ptr<std::function<void()>>m_starthttpServicePool;
	std::unique_ptr<boost::asio::steady_timer> m_timer;
	std::shared_ptr<LOG>m_log;
	std::shared_ptr<IOcontextPool> m_ioPool;


	std::shared_ptr<ReadBuffer> m_tempReadBuffer;

private:
	void ready(int beginSize);



};