#pragma once

#ifndef __NUMAWARE_UTILS_H__
#define __NUMAWARE_UTILS_H__

/*-----------------------------------------
  Included Libraries
  -----------------------------------------*/
#include <math.h>						//used for rounding functions
#include <stdlib.h>						//used for string to long conversion functions
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/io.h>
#include <stdint.h>
#include <dirent.h>
#include <map>
#include <bits/stdc++.h>
#include <random>
#include <algorithm>

#ifdef WIN32
#include <windows.h>
#include <io.h>				//
#include <process.h>		// for _beginthreadex
typedef long int off_t;
#define false 0
#else
#include <unistd.h>						//used for pread and pwrite (msr functionality)
#include <fcntl.h>						//used for opening the memory device file (msr functionality)
#include <signal.h>
#include <errno.h>

#include <pthread.h>					//for thread
#include <sys/mman.h>					//used for memory mapping memory device to physical memory
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <linux/unistd.h>
#include <linux/kernel.h>       		/* for struct sysinfo */
#endif

#ifdef INTEL
#define cpuid(func,ax,bx,cx,dx)\
	__asm__ __volatile__ ("cpuid":\
	"=a" (ax), "=b" (bx), "=c" (cx), "=d" (dx) : "a" (func));

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40
#endif

#define MAP_HUGE_SHIFT  26
#define MAP_HUGE_MASK   0x3f
#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)

#endif

extern bool FencingEn;
extern bool PrefetchDis;
extern int RunIter;
extern double RunTime;
extern unsigned long long int *ClkPerSec;
extern double NSecClk;
typedef void* (*pthread_entry_func_ptr) (void *);

#define MAX_THREADS 256  //Maximum Number of Threads supported
#define THREAD_ARRAY_SIZE 1024*1024 //Size of Addresses a CPU Thread can target
#define MMCFG_BASE 0x80000000
#define CPUSET 0
#define ALL 1

/*
 * Global Variables
 */
#ifndef WIN32
pthread_t tid[MAX_THREADS];           /* Array of P-Thread Data structures for Threading */
uint8_t ThreadInUse[MAX_THREADS];    /* Array of P-Thread Data structures which are in-use */
void Execute(int Thread);
#else
void Execute(void);
DWORD WinThreadID[MAX_THREADS];
DWORD WINAPI Execute2(LPVOID temp);
extern HANDLE BWThread;
#endif

using namespace std;

char* getSystemInfo(FILE *file, char* cmd)
{
    char *output;
    if (0 == (file = (FILE *)popen(cmd, "r")))
    {
        perror("popen() failed.");
        exit(0);
    }

    output = (char *)malloc(35*sizeof(char));
    fgets(output,35,file);
    pclose(file);

    return output;
}

void TouchPages(unsigned long LogicalAddress2, unsigned long MemorySpan)
{
    detailedverbose("TouchPages called with 0x%lx\n",LogicalAddress2);
    unsigned long j;
    unsigned long tempdata = 0;

    for(j = 0; j < MemorySpan; j += 0x1000){
        tempdata = LogicalAddress2 + j;
        *((unsigned long *)(tempdata)) = tempdata;
    }
}

int CreateWorkerThread( pthread_entry_func_ptr EntryFunc, void* EntryFuncArgs, int *RetId)
{
    int Thread;
    cpu_set_t cpuset;

    for(Thread = 0; Thread < MAX_THREADS; Thread++)
    {
#ifdef WIN32
        /* Check if there is space for a free thread first */

        if(ThreadsInUse[Thread] == 0)
        {
            if((BWThread = CreateThread(NULL, 0, Execute2, (void *) EntryFuncArgs, 0 , &WinThreadID[Thread])) < 0)
            {
                cerr << "Error in creating thread " << Thread << endl;
                exit(1);
            }
            ThreadsInUse[Thread] = 1;
            *RetId = Thread;
            break;
        }
#else
        /* Check if there is space for a free thread first */

        if(ThreadInUse[Thread] == 0)
        {
            if(pthread_create(&(tid[Thread]), NULL, EntryFunc, (void *)EntryFuncArgs) > 0)
            {
                cerr << "Error in creating thread " << Thread << endl;
                exit(1);
            }
            ThreadInUse[Thread] = 1;
            *RetId = Thread;
            break;
        }
#endif
    }

    return 0;
}

void SetWorkerThreadAffinity(uint32_t ThreadID, uint32_t CpuThreadNumber)
{
#ifdef WIN32
    auto mask = (static_cast<DWORD_PTR>(1) << CpuThreadNumber);  /* core number starts from 0 */
    SetThreadAffinityMask(WinThreadID[ThreadID],mask);
#else
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(CpuThreadNumber, &cpuset);

    pthread_setaffinity_np(tid[ThreadID],sizeof(cpu_set_t),&cpuset);

#endif
}

void TerminateWorkerThread(uint32_t ThreadId, bool cancel)
{
#ifdef WIN32
    /*TODO : Need to find WINAPI equivalent for Thread Join*/

    TerminateThread(WinThreadID[ThreadId],0);
#else
    void *status;
    if(cancel)
    {
        if(pthread_cancel(tid[ThreadId]) == -1)
        {
            perror("Could not cancel Thread.\n");
            exit(3);
        }
    }

    if(pthread_join(tid[ThreadId],&status) == -1)
    {
        perror("Could not join Thread to main.\n");
        exit(4);
    }
#endif
}

void SetWorkerThreadCancelParams()
{
#ifdef WIN32
    /* TODO : Figure out if some pre-steps are required before terminating a thread on WINAPI */
#else
    int cancel_state, cancel_type;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&cancel_state);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,&cancel_type);
#endif
}

#ifndef WIN32
/**
 * Read an MSR
 * This function will read a specified msr and return this data.
 * @param cpuNumber The cpu number of desired msr read.
 * @param msrNumber Offset to be read.
 * @return the value of the MSR requested.
 */
unsigned long long readMSR(int cpuNumber, unsigned int msrOffset){
    unsigned long long msrData;           // returned msr data
    char msr_file_name[64];
    int filedescriptor;

    // create directory structure to be read
    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpuNumber);
    // open dev file
    filedescriptor = open(msr_file_name, O_RDONLY);

    // error checks and data read
    if(filedescriptor < 0){
        fprintf(stderr,"readMSR: Device file open error.\n");
        printf("Did you run 'modprobe msr'?\n");
    }
    if (pread(filedescriptor, &msrData, sizeof(msrData), msrOffset) != sizeof(msrData))
        fprintf(stderr,"readMSR: PRead error.\n");

    close(filedescriptor);
    return (msrData);
}


/**
 * Write an MSR
 * This function will write to a specified MSR.
 * @param cpuNumber The cpu number of desired msr write.
 * @param msrNumber offset to be written,
 * @param msrData the data to be written to the offset.
 */
void writeMSR(int cpuNumber, unsigned int msrNumber, unsigned long long msrData){
    char msr_file_name[64];
    int filedescriptor;

    // create directory structure to be written
    sprintf(msr_file_name, "/dev/cpu/%d/msr", cpuNumber);
    // open dev file
    filedescriptor = open(msr_file_name, O_WRONLY);

    // error checks and write data
    if(filedescriptor < 0){
        fprintf(stderr,"writeMSR: Device file open error.\n");
        printf("Did you run 'modprobe msr'?\n");
    }
    if (pwrite(filedescriptor, &msrData, sizeof(msrData), msrNumber) != sizeof(msrData))
        fprintf(stderr,"writeMSR: PWrite error.\n");

    close(filedescriptor);
}

#endif

#ifdef INTEL
unsigned long long rdtsc(void)
{
    unsigned long hi, lo;
#ifdef WIN32
    #ifdef _AMD64_
        unsigned long long ret;
        QueryPerformanceCounter((LARGE_INTEGER *)&ret);
        return ret;
    #else
    __asm { rdtsc
        mov hi,edx
        mov lo,eax
    }
    #endif
#else
    __asm__ __volatile__ ("xorl %%eax, %%eax \n  cpuid" ::: "%eax", "%ebx", "%ecx", "%edx");
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
#endif
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
#else
//  POWER PC RETURNING HUGE NUMBERS ?????
unsigned long long rdtsc(void)
{
    unsigned long long int result=0;
    unsigned long int upper, lower,tmp;
    __asm__ volatile(
            "0:                  \n"
            "\tmftbu   %0           \n"
            "\tmftb    %1           \n"
            "\tmftbu   %2           \n"
            "\tcmpw    %2,%0        \n"
            "\tbne     0b         \n"
            : "=r"(upper),"=r"(lower),"=r"(tmp)
            );
    result = upper;
    result = result<<32;
    result = result|lower;

    return(result);
}
#endif

static inline unsigned long long int GetTickCount()
{
#ifdef WIN32
    /* TODO find similar function on Windows */   
#else
    struct timeval tp;
    gettimeofday(&tp,NULL);
    return tp.tv_sec*1000+tp.tv_usec/1000;
}
#endif

void Calibrate(unsigned long long int *ClkPerSec,double NSecClk)
{
    unsigned long long int start,stop,diff;
    unsigned long long int starttick,stoptick,difftick;
    
    stoptick = GetTickCount();
    while(stoptick == (starttick=GetTickCount()));
    
    start = rdtsc();
    while((stoptick=GetTickCount())<(starttick+500));
    stop  = rdtsc();

    diff = stop-start;
    difftick = stoptick-starttick;

    *ClkPerSec = ( diff * (unsigned long long int)1000 )/ (unsigned long long int)(difftick);
    NSecClk = (double)1000000000 / (double)(__int64_t)*ClkPerSec;
}

std::pair <uint64_t,uint64_t> GetAddressRangeFromDmi(int Node)
{
    uint64_t StartAddr,StopAddr;
    int NodeIndex = -1;
    uint64_t NextAddr = 0x00000000FFFFFFFF;
    vector<std::pair<uint64_t,uint64_t>> Address;
    string tempstart,tempend;
#ifdef WIN32

#else
    const string cmd = "dmidecode --type 19 | grep \"Address:\"";
    FILE* fpipe = popen(cmd.c_str(),"r");
    if(fpipe == NULL)
        throw runtime_error(string("Cannot run : ") + cmd);

    char *lineptr;
    size_t n;
    ssize_t s;

    do
    {
        lineptr  = NULL;
        tempstart = "";
        tempend   = "";
        s = getline(&lineptr, &n, fpipe);
        if(s>0 && lineptr != NULL)
        {
            if(lineptr[s-1] == '\n')
                lineptr[--s] = 0;
            if(lineptr[s-1] == '\r')
                lineptr[--s] = 0;

            if (string(lineptr).find("Starting Address") != string::npos)
            {
                for(int i = string(lineptr).find("0x"); i< string(lineptr).length();i++)
                    tempstart += (lineptr)[i];
                StartAddr = strtoull(tempstart.c_str(),NULL,16);
            }

            if (string(lineptr).find("Ending Address") != string::npos)
            {
                for(int i = string(lineptr).find("0x"); i< string(lineptr).length();i++)
                    tempend += (lineptr)[i];
                StopAddr = strtoull(tempend.c_str(),NULL,16);

                Address.push_back(make_pair(StartAddr,StopAddr));
            }
        }

        if(lineptr != NULL)
            free(lineptr);
    }while(s>0);

    int i = 0;
    while(NodeIndex != Node)
    {
        if(Address[i].first == (NextAddr + 1))
        {
            NodeIndex +=1;
            NextAddr = Address[i].second;
        }
        i++;
    }
#endif

    return Address[i-1];
}

/**
 * The function returns 0 when the hardware multi-threaded bit is
 * not set.
 */
unsigned int is_HWMT_Supported(void)
{
    unsigned int a,b,c,d;

    // read cpuid function 1
    cpuid(1,a,b,c,d);

    // return register d with correct mask correct bit
    return (d & HWD_MT_BIT);
}

#endif //__NUMAWARE_UTILS_H__