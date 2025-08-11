#pragma once



#include <string>
#include <random>
#include <chrono>

//验证码生成模块
struct RandomCodeGenerator
{
	RandomCodeGenerator();

	//生成验证码函数  传入指针需确保有足够大的空间  
	const bool generate(char*& verifyCodeBuf, const unsigned int length = 6);


private:
	std::string m_charset{ "01234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" };
	std::mt19937 m_rng;
	std::uniform_int_distribution<size_t> m_dist;
};



