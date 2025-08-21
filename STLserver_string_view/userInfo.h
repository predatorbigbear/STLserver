#pragma once


#include<string_view>
#include<string>


//对应mysql user表中的用户信息
struct USERINFO
{
	USERINFO() = default;

	//重置数据函数
	void clear();

	//设置账号名
	void setAccount(const char* begin, const char* end);

	//设置账号名
	void setAccount(const std::string_view name);

	//获取账号名
	const std::string& getAccount();

	//获取账号名
	const std::string_view getAccountView();

private:

	//用户账号名
	std::string account;

	//会员等级   默认0  会员等级制度待定
	unsigned int level{ 0 };

};



