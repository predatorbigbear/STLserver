#pragma once


namespace REDISNAMESPACE
{
	static constexpr char* get{ "get" };
	static constexpr char* setnx{ "setnx" };
	static constexpr char* foo{ "foo" };
	static constexpr char* age{ "age" };
	static constexpr char* age1{ "age1" };
	static constexpr char* strlenStr{ "strlen" };
	static constexpr char* numberZero{ "0" };
	static constexpr char* mget{ "mget" };


	static constexpr int getLen{ strlen(get) };
	static constexpr int mgetLen{ strlen(mget) };
	static constexpr int setnxLen{ strlen(setnx) };
	static constexpr int fooLen{ strlen(foo) };
	static constexpr int ageLen{ strlen(age) };
	static constexpr int age1Len{ strlen(age1) };
	static constexpr int strlenStrLen{ strlen(strlenStr) };
	static constexpr int numberZeroLen{ strlen(numberZero) };

};