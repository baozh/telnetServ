#include "muduo/base/LogFile.h"

#include "muduo/base/FileUtil.h"
#include "muduo/base/ProcessInfo.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <string.h>
#include<sys/types.h>
#include<dirent.h>
#include <string>
#include <stdlib.h>

using namespace muduo;

LogFile::LogFile(const string& basename,
                 size_t rollSize,
				 const string& logFilePath,
                 bool threadSafe,
                 int flushInterval,
                 int checkEveryN,
				 int keepLogDate)
  : basename_(basename),
    rollSize_(rollSize),
	logFilePath_(logFilePath),
    flushInterval_(flushInterval),
    checkEveryN_(checkEveryN),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0),
	reserveLogDate_(keepLogDate)
{
  assert(basename.find('/') == string::npos);
  rollFile();
}

LogFile::~LogFile()
{
}

void LogFile::append(const char* logline, int len)
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}

void LogFile::append_unlocked(const char* logline, int len)
{
  file_->append(logline, len);

  if (file_->writtenBytes() > rollSize_)
  {
    rollFile();
  }
  else
  {
    ++count_;
    if (count_ >= checkEveryN_)
    {
      count_ = 0;
      time_t now = ::time(NULL);
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_)
      {
        rollFile();
      }
      else if (now - lastFlush_ > flushInterval_)
      {
        lastFlush_ = now;
        file_->flush();
      }
    }
  }
}

//文件是否存在
bool LogFile::isExistFilePath(const char *pszFileName)
{
	bool bExisted = false;

	if(access(pszFileName, F_OK) >= 0)  /*file is existed*/
	{
		bExisted = true;
	}
	else
	{
		printf("FilePath:%s is not existed!", pszFileName);
	}
	return bExisted;	
}

bool LogFile::createDir(const char *pszDirName, uint32_t dwDirLen, uint16_t& wErrorCode)
{
#define MAX_IDS_PATH_LEN	(256)  //最大路径长度

	bool bRet = false;
	if (( MAX_IDS_PATH_LEN < dwDirLen )	||(pszDirName == NULL))
	{
		return bRet;
	}

	char achDir[MAX_IDS_PATH_LEN] = {0};
	memcpy(achDir, pszDirName, dwDirLen);

	char ch  = '/';
	char ch1 = '\\';
	char *pTmp = NULL;

	if( '/' != achDir[0] && '.' != achDir[0] )   //起始应为"/"或"."
		return bRet;

	//1. change '\' to '/'
	pTmp = strchr( achDir, ch1);
	while(pTmp)
	{
		*pTmp = '/';
		pTmp = strchr( pTmp + 1, ch1); 
	}

	//2. if the end character is not '/', then add '/' to the end
	if( achDir[dwDirLen - 1] != ch)
	{
		if ( MAX_IDS_PATH_LEN < dwDirLen + 1 )
		{
			return bRet;
		}
		achDir[dwDirLen] = ch;
	}

	pTmp = strchr( achDir, ch);   /* pszNewDir : "c:/recorder/kdc/" (win32) or "/home/aaron" (linux) */
	char szTmpDir[MAX_IDS_PATH_LEN] = {0};

	pTmp = strchr( pTmp + 1, ch); /* omit c:/ on win32 or / on linux */

	while( pTmp )
	{
		memcpy(szTmpDir, achDir, pTmp - achDir + 1);

		/* if current directory is not existed */
		if( !isExistFilePath(szTmpDir ) )
		{
			if( !mkdir( szTmpDir, 0x1FF))   /* successfully */
			{
				wErrorCode = ERR_NO;
				bRet = true;
			}
			else if (EMLINK == errno)
			{
				wErrorCode = ERR_TOO_MANY_LINKS;
			}else
			{
				wErrorCode = ERR_CREATEDIR_FAIL;
			} 
		}

		pTmp = strchr( pTmp + 1, ch);		
		memset( szTmpDir, 0, MAX_IDS_PATH_LEN);		
		if( wErrorCode != ERR_NO )
			return bRet;
	}

	printf("[LogFile::createDir] CreateDir :%s ok!\n", pszDirName);
	bRet = true;	
	return bRet;
}

//bzh add 2015.7.1 start
bool LogFile::deleteExpiredFile(const char *pszDirName)
{
	if (pszDirName == NULL) return true;
	if(isExistFilePath(pszDirName) == false)
	{
		return true;
	}

	std::vector<std::string> nameList;
	nameList.clear();
	DIR *dp = opendir(pszDirName);
	if(NULL == dp)
	{
		//LOG_ERROR << "open dir failed, dir=" << pszDirName;
		return true;
	}
	struct dirent* dirp = NULL;
	while((dirp = readdir(dp)) != NULL)
	{
		int dirLen = strlen(dirp->d_name);
		if (dirLen > 4 &&
			dirp->d_name[dirLen -4] == '.' &&
			dirp->d_name[dirLen -3] == 'l' &&
			dirp->d_name[dirLen -2] == 'o' &&
			dirp->d_name[dirLen -1] == 'g')
		{
			nameList.push_back(dirp->d_name);
			//printf("logfileName:%s\n", dirp->d_name);
		}
	}
	closedir(dp);

	time_t timeNow = time(NULL);
	timeNow -= reserveLogDate_ * 24 * 3600;
	struct tm tm;
	localtime_r(&timeNow, &tm);
	char timebuf[32] = {0};
	strftime(timebuf, sizeof timebuf, ".%Y%m%d", &tm);
	std::string cpFileName = basename_ + timebuf;
	//printf("cpFileName:%s\n", cpFileName.c_str());

	for (std::vector<std::string>::iterator iter = nameList.begin(); iter != nameList.end(); iter++)
	{
		if(strncmp(iter->c_str(), cpFileName.c_str(), cpFileName.length()) < 0)
		{
			char cmd[256] = {0};
			snprintf(cmd, sizeof(cmd), "rm -f %s%s", pszDirName, iter->c_str());
			system(cmd);
			//printf("delete log file :%s\n", cmd);
		}
	}
	return true;
}
//bzh add 2015.7.1 end

bool LogFile::rollFile()
{
  time_t now = 0;

  //bzh add 2015.6.2 start
  bool isUseFilePath = true;
  if(isExistFilePath(logFilePath_.c_str()) == false)
  {
	  //create path path
	  uint16_t errCode = 0;
	  if (createDir(logFilePath_.c_str(), logFilePath_.size(), errCode) == false)
	  {
		  printf("[LogFile::rollFile] CreateDir :%s failed! errCode:%d.\n", logFilePath_.c_str(), errCode);
		  isUseFilePath = false;
	  }
  }
  //bzh add 2015.6.2 end

  string filename = getLogFileName(basename_, &now);

  //bzh add 2015.6.2 start
  if (isUseFilePath == true)
  {
	  if( logFilePath_[logFilePath_.size() - 1] != '/')
	  {
		  logFilePath_ = logFilePath_ + "/";
	  }
	  filename = logFilePath_ + filename;
	  deleteExpiredFile(logFilePath_.c_str());
  }
  else
  {
	  deleteExpiredFile("./");
  }
  //bzh add 2015.6.2 end

  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new FileUtil::AppendFile(filename));
    return true;
  }
  return false;
}

string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64);
  filename = basename;

  char timebuf[32];
  struct tm tm;
  *now = time(NULL);
  //add by egypt
  localtime_r(now, &tm);
  //gmtime_r(now, &tm); // FIXME: localtime_r ?
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);
  filename += timebuf;

  filename += ProcessInfo::hostname();

  char pidbuf[32];
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf;

  filename += ".log";

  return filename;
}

