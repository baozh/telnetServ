#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include "muduo/base/Mutex.h"
#include "muduo/base/Types.h"

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

namespace FileUtil
{
class AppendFile;
}

class LogFile : boost::noncopyable
{
public:
	//bzh add
	enum enErrorCode
	{
		ERR_NO = 0,
		ERR_TOO_MANY_LINKS = 1, 
		ERR_CREATEDIR_FAIL = 2, 	
	};

 public:
  LogFile(const string& basename,
          size_t rollSize,
		  const string& logFilePath,
          bool threadSafe = true,
          int flushInterval = 3,
          int checkEveryN = 1024,
		  int keepLogDate = 120);  //Ĭ�ϱ���4���µ���־
  ~LogFile();

  void append(const char* logline, int len);
  void flush();
  bool rollFile();
  bool createDir(const char *pszDirName, uint32_t dwDirLen, uint16_t& wErrorCode);   //bzh add 2015.6.2
  bool isExistFilePath(const char *pszFileName);
  bool deleteExpiredFile(const char *pszDirName);   //bzh add 2015.7.1
 private:
  void append_unlocked(const char* logline, int len);

  static string getLogFileName(const string& basename, time_t* now);

  string logFilePath_;   //bzh add 2015.6.2
  int reserveLogDate_;    //bzh add 2015.6.2  //Ҫ�������־����
  const string basename_;
  const size_t rollSize_;
  const int flushInterval_;
  const int checkEveryN_;

  int count_;

  boost::scoped_ptr<MutexLock> mutex_;
  time_t startOfPeriod_;
  time_t lastRoll_;
  time_t lastFlush_;
  boost::scoped_ptr<FileUtil::AppendFile> file_;

  const static int kRollPerSeconds_ = 60*60*24;
};

}
#endif  // MUDUO_BASE_LOGFILE_H
