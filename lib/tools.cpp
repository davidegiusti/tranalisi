#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#include <iostream>
#include <cstdarg>
#include <git_hash.hpp>
#include <signal.h>
#include <sys/stat.h>
#include <execinfo.h>

#include <tools.hpp>

using namespace std;

//! write the list of called routines
void print_backtrace_list()
{
  void *callstack[128];
  int frames=backtrace(callstack,128);
  char **strs=backtrace_symbols(callstack,frames);
  
  //only master rank, but not master thread
  cerr<<"Backtracing..."<<endl;
  for(int i=0;i<frames;i++) cerr<<strs[i]<<endl;
  
  free(strs);
}

void internal_crash(int line,const char *file,const char *temp,...)
{
  char buffer[1024];
  va_list args;
  
  va_start(args,temp);
  vsprintf(buffer,temp,args);
  va_end(args);
  
  cerr<<"ERROR at line "<<line<<" of file "<<file<<": "<<buffer<<endl;
  print_backtrace_list();
  exit(1);
}

string combine(const char *format,...)
{
  char buffer[1024];
  va_list args;
  
  va_start(args,format);
  vsprintf(buffer,format,args);
  va_end(args);
  
  return buffer;
}

int file_exists(string path)
{
  int status=1;
  
  FILE *f=fopen(path.c_str(),"r");
  if(f!=NULL)
    {
      status=1;
      fclose(f);
    }
  else status=0;
  
  return status;
}

int dir_exists(string path)
{
  struct stat info;
  int rc=stat(path.c_str(),&info);
  int is=(info.st_mode&S_IFDIR);
  return (rc==0)&&is;
}

void signal_handler(int sig)
{
  char name[100];
  switch(sig)
    {
    case SIGSEGV: sprintf(name,"segmentation violation");break;
    case SIGFPE: sprintf(name,"floating-point exception");break;
    case SIGXCPU: sprintf(name,"cpu time limit exceeded");break;
    case SIGABRT: sprintf(name,"abort signal");break;
    default: sprintf(name,"unassociated");break;
    }
  print_backtrace_list();
  
  CRASH("signal %d (%s) detected, exiting",sig,name);
}


//! class to force call to initialization
class initializer_t
{
public:
  initializer_t()
  {
    //associate signals
    signal(SIGSEGV,signal_handler);
    signal(SIGFPE,signal_handler);
    signal(SIGXCPU,signal_handler);
    signal(SIGABRT,signal_handler);
    
    //print info
    cout<<"Git hash: "<<GIT_HASH<<endl;
    cout<<"Last commit on: "<<GIT_TIME<<endl;
    cout<<"Commit message: \""<<GIT_LOG<<"\""<<endl;
    cout<<"Configured on "<<CONFIG_TIME<<endl;
    cout<<"Configured with flags: "<<CONFIG_FLAGS<<endl;
    cout<<"Compiled at "<<__TIME__<<" of "<<__DATE__<<endl;
  }
};

initializer_t initializer;
