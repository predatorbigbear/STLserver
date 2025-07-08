#pragma once


#include <boost/asio.hpp>
#include <boost/thread.hpp>


#include<memory>
#include<vector>
#include<mutex>




using work_guard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

struct IOcontextPool
{
	IOcontextPool();

	void setThreadNum(bool& success,const size_t threadNum = 0);

	std::shared_ptr<boost::asio::io_context> getIoNext();

	void run();

	void stop();

private:
	std::shared_ptr<std::vector<std::shared_ptr<boost::asio::io_context>>>m_ioc{};
	boost::thread_group m_tg;
	std::vector<work_guard> m_work;
	std::mutex m_ioMutex;

	std::shared_ptr<boost::asio::io_context> m_io_temp{};

	bool m_hasRun{ false };
	bool m_hasSetThreadNum{ false };

	size_t m_ioNum{};
};