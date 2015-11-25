#ifndef __TELNET_SERV_H
#define __TELNET_SERV_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <muduo/base/Thread.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Condition.h>
#include <string>
#include <map>
#include <boost/noncopyable.hpp>
#include <boost/implicit_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>


// UniformFunc: definition for all functions those can invoked by user through telnet
typedef int (*UniformFunc)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);

#define INVALID_SOCKET		(-1)



namespace telnet_serv
{
	const int kSmallBuffer = 4000;
	const int kLargeBuffer = 4000*1000;

	template<int SIZE>
	class FixedBuffer : boost::noncopyable
	{
	public:
		FixedBuffer()
			: cur_(data_)
		{}

		~FixedBuffer()
		{}

		void append(const char* /*restrict*/ buf, size_t len)
		{
			// FIXME: append partially
			if (boost::implicit_cast<size_t>(avail()) > len)
			{
				memcpy(cur_, buf, len);
				cur_ += len;
			}
		}

		const char* data() const { return data_; }
		int length() const { return static_cast<int>(cur_ - data_); }

		// write to data_ directly
		char* current() { return cur_; }
		int avail() const { return static_cast<int>(end() - cur_); }
		void add(size_t len) { cur_ += len; }

		void resetData() { cur_ = data_; }
		void bzero() { ::bzero(data_, sizeof data_); }

		// for used by unit test
		std::string asString() const { return std::string(data_, length()); }

	private:
		const char* end() const { return data_ + sizeof data_; }
		char data_[SIZE];
		char* cur_;
	};

	class TelnetServer : boost::noncopyable
	{
	public:
		typedef telnet_serv::FixedBuffer<telnet_serv::kLargeBuffer> Buffer;
		typedef boost::ptr_vector<Buffer> BufferVector;
		typedef BufferVector::auto_type BufferPtr;

		struct TCmdTable 
		{
			TCmdTable()
			{
				funName_ = "";
				funUsage_ = "";
				cmdFunc_ = NULL;
			}
			std::string funName_;
			UniformFunc cmdFunc_;	/* Implementation function */
			std::string funUsage_;	/* Usage message */
		};

	public:

		TelnetServer()
			:mutex_(),
			cond_(mutex_),
			currentBuffer_(new Buffer),
			nextBuffer_(new Buffer),
			buffers_()
		{
			portListen_ = 0;

			sockClient_ = INVALID_SOCKET;
			sockServ_ = INVALID_SOCKET;

			cmdFunTable_.clear();

			isRunning_ = false;

			thdTelnetDaemonHandle_ = NULL;
			thdLogOutPutHandle_ = NULL;

			currentBuffer_->bzero();
			nextBuffer_->bzero();
			buffers_.reserve(16);
		};

		~TelnetServer()
		{
			if (isRunning_ == true)
			{
				destory();
			}
		}

		bool init(int flushInterval = 1, uint16_t telnetPort = 2500);   //创建telnet服务线程和日志输出线程、消息队列
		bool destory();

		void regCommand(const char* name, void* func, const char* usage);
		void telPrintf(char *szFormat, ...);
		void telPrintfNoTime(char *szFormat, ...);
		
	private:

		//telnet执行线程(接受telnet客户端,响应用户输入)
		void TelnetDaemonFun(uint16_t wPort);

		//日志输出线程(从消息队列中取日志，输出到telnet客户端的屏幕上)
		void LogOutPutFun();
		bool sendMsgToTerminal(const char *pchMsg, int msgLen);
		void appendToMsgBuf(const char* logline, int len);
		void pushMsgToTerminalImmidate(char *szFormat, ...);   //不是线程安全的，外部线程不可调用

		int createTcpListenSock(uint16_t wPort);

		bool isHaveTelnetClient() { return sockClient_ != INVALID_SOCKET;}
		void setTelnetClient(int sock) {sockClient_ = sock;}
		void setInvalidTelnetClient() {sockClient_ = INVALID_SOCKET;}

		void PromptShow();

		void CmdParse(const char* pchCmd, unsigned char byCmdLen);
		void RunCmd(char *szCmd);
		UniformFunc FindCommand(const char* name);
		void telhelp();
		int WordParse(const char* word);
		bool SockSend(int tSock, const char* pchBuf, uint32_t dwLen);
		bool isNullStr(const char* pchCmd);
		uint32_t formatTime(char *pszMsg);
	private:

		uint16_t portListen_;
		int sockClient_;
		int sockServ_;

		std::map<std::string, TCmdTable> cmdFunTable_;

		volatile bool isRunning_;

		muduo::Thread* thdTelnetDaemonHandle_;
		muduo::Thread* thdLogOutPutHandle_;

		int flushInterval_;

		muduo::MutexLock mutex_;
		muduo::Condition cond_;

		BufferPtr currentBuffer_;
		BufferPtr nextBuffer_;
		BufferVector buffers_;
	};
}

typedef telnet_serv::TelnetServer TelnetServer;

#endif

