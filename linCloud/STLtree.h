#pragma once
#include<sstream>
#include<string>

using std::stringstream;
using std::string;


struct STLtree
{
	STLtree() = default;

	void clear()
	{
		m_sstr.str("");
		m_sstr.clear();
		index = -1;
		empty = true;
	}

	void put(const std::string &str1, const std::string &str2)
	{
		if (++index)m_sstr << ',';
		m_sstr << '"' << str1 << "\":\"" << str2 << '"';
		empty = false;
	}

	void put(const std::string &str1, const int num)
	{
		if (++index)m_sstr << ',';
		m_sstr << '"' << str1 << "\":\"" << std::to_string(num) << '"';
		empty = false;
	}

	void push_back(const std::string &str1, const STLtree &other)
	{
		if (++index)m_sstr << ',';
		m_sstr << '"' << str1 << "\":";
		if(other.isEmpty())
			m_sstr << '"' << other.print() << '"';
		else
			m_sstr << '{' << other.print() << '}';
		empty = false;
	}

	void push_back(const STLtree &other)
	{
		if (++index)m_sstr << ',';
		if (other.isEmpty())
			m_sstr << '"' << other.print() << '"';
		else
			m_sstr << '{' << other.print() << '}';
		empty = false;
	}

	void put_child(const std::string &str1, const STLtree &other)
	{
		if (++index)m_sstr << ',';
		m_sstr << '"' << str1 << "\":";
		if(other.isEmpty())
			m_sstr<<'\"' << other.print() << '\"';
		else
			m_sstr << '[' << other.print() << ']';
		empty = false;
	}

	const std::string make_json()const
	{
		return std::string("{").append(m_sstr.str()).append("}");
	}

	const std::string print()const
	{
		return m_sstr.str();
	}

	const bool isEmpty()const
	{
		return empty;
	}

private:
	std::stringstream m_sstr;
	int index{ -1 };
	bool empty{ true };
};