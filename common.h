#pragma once

#ifndef __NUMAWARE_COMMON_H__
#define __NUMAWARE_COMMON_H__

extern "C" {
#include <pci/pci.h>
#include <numa.h>
#include <numaif.h>
}
#include "utils.h"

using namespace std;

struct sCoreAddrCfg
{
    uint64_t AddressArray[THREAD_ARRAY_SIZE];
    uint32_t Size;
    uint64_t AddrRangeStart;
    uint64_t AddrRangeStop;
    uint64_t AddrRangeStride;
    uint64_t AddrRangeIteration;
    uint64_t AddressLoopCount;
    uint8_t AddrType;
};

struct sMemTgt
{
    void *Virt;
    uint64_t Phys;
    uint64_t Size;
    uint64_t Stride;
    map<std::pair<int,int>,string> FileName;
    sCoreAddrCfg CoreAddrCfg[MAX_THREADS];
};

struct sDeviceType {
    bool core;
    vector<std::pair<int,int>> DeviceInfo;
};

class Target
{
public:
    sMemTgt MemTgt;
    struct bitmask *mask;
public:
    Target() {
        mask = NULL;
    };
    unsigned long int MapPhyMemToVirtMem(unsigned long PhysicalAddrCopy, off_t MemorySpanCopy);
    void UnmapMem (unsigned long *LogicalAddrCopy, off_t MemorySpanCopy);
    unsigned long long GetPhysAddress(void *vaddr);
    uint64_t GenerateRandomAddressFromRange(uint64_t StartAddress,uint64_t StopAddress,int BufferSize,int VmSize,int index);
    void* MapProcessToMemoryNode(int Node,uint64_t MapSize);
    void FlushCpuCache(void *Mem, uint64_t MemSize)
};

unsigned long int Target :: MapPhyMemToVirtMem(unsigned long PhysicalAddrCopy, off_t MemorySpanCopy)
{
    unsigned long LogicalAddrCopy;
#ifdef WIN32
    LogicalAddrCopy = (INTPOINTER)MapPhyMem((PVOID)PhysicalAddrCopy, MemorySpanCopy);
#else
    int pagesize,fd;
    
    /* Check for appropriate IO permissions */
    if(iopl(3)){
        cout << "Cannot get I/O permissions (try running as root)\n";
        return -1;
    }

    /* Get the number of bytes in a page */
    pagesize = getpagesize();
    printf("Pagesize: 0x%x\n",pagesize);

    /* Open system memory as a read/write file */
    fd = open("/dev/mem", O_RDWR);

    /*
     * Map from the /dev/mem file, offset by the Physical Address
     * Map for memoryspan + pagesize bytes
     * logical address points at this processes memory space mapped to the
     * shared memory
     */
    printf("Trying to map 0x%lx bytes to physical address 0x%lx\n",(MemorySpanCopy + pagesize),PhysicalAddrCopy);
    LogicalAddrCopy = (unsigned long) mmap(NULL, (MemorySpanCopy + pagesize),
            PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)PhysicalAddrCopy);

    /* Check if the memory map worked */
    if(LogicalAddrCopy == (unsigned long)MAP_FAILED)
    {
        perror("Mapping Virtual Memory to Physical Memory Failed.\n");
        return -3;
    }
    else
        printf("Virtual Address: 0x%lx\n",LogicalAddrCopy);;

    return LogicalAddrCopy;    

#endif
}

void Target :: UnmapMem(unsigned long *LogicalAddrCopy,off_t MemorySpanCopy)
{
#ifdef WIN32
    UnmapPhyMem((PVOID) LogicalAddrCopy, MemorySpanCopy);
#else
    /* remove the memory map */
    munmap(LogicalAddrCopy, MemorySpanCopy);
#endif
}

unsigned long long Target :: GetPhysAddress(void *vaddr)
{
    unsigned long long addr;
    static int pagemap_fd=-1;
    int pid = getpid();
    string filedesc = "/proc/" + to_string(pid) + "/pagemap";
    if (pagemap_fd==-1) 
    {
        pagemap_fd = open(filedesc.c_str(), O_RDONLY);
    }
    int n = pread(pagemap_fd, &addr, 8, ((unsigned long long)vaddr / 4096) * 8);
    if (n != 8)
        return 0;
    if (!(addr & (1ULL<<63)))
        return 0;
    addr &= (1ULL<<54)-1;
    addr <<= 12;
    return addr + ((unsigned long long)vaddr  & (4096-1));
}

uint64_t Target :: GenerateRandomAddressFromRange(uint64_t StartAddress,uint64_t StopAddress,int BufferSize,int VmSize,int index)
{
    uint64_t RandomAddr = 0x0;
    int PageSize = getpagesize();
    uint64_t AddressRangePerVm = (StopAddress - StartAddress)/VmSize;
    uint64_t StartAddr = StartAddress + (AddressRangePerVm)*(index);
    uint64_t StopAddr = StartAddress + (AddressRangePerVm)*(index+1);
    StopAddr = (StopAddr > StopAddress) ? StopAddress : StopAddr;
    std :: random_device dev;
    std :: default_random_engine Generator(dev());

    
    detailedverbose("Start Address : 0x%llx , Stop Address : 0x%llx\n",StartAddr,StopAddr);

    std :: uniform_int_distribution<uint64_t> Distribution(StartAddr,(StopAddr-BufferSize));

    RandomAddr = Distribution(Generator);
    if((RandomAddr % PageSize) != 0)
        RandomAddr = ((RandomAddr / PageSize) + 1) * PageSize;
    return RandomAddr;
}

void* Target :: MapProcessToMemoryNode(int Node,uint64_t MapSize)
{
    void *LogicalAddressCopy;
    nodemask_t mask;

    numa_set_strict(1);

    if(MapSize < 4096)
    {
        LogicalAddressCopy = numa_alloc_onnode(MapSize,Node);
        if(LogicalAddressCopy == NULL)
        {
            cerr << "Unable to allocate " << MapSize << " Bytes of memory on Node " << Node << ".\nExiting Test ...\n" << endl;
            exit(0);
        }	
    }
    else if(MapSize > 4096 && MapSize < 2097152)
    {
        nodemask_zero(&mask);
        nodemask_set_compat(&mask,Node);
        numa_set_membind_compat(&mask);
        LogicalAddressCopy = mmap(0, MapSize, PROT_READ | PROT_WRITE , MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if(LogicalAddressCopy == MAP_FAILED)
        {
            cerr << "Unable to allocate " << MapSize << " Bytes of memory.\nPlease make sure hugepages memory pool is allocated for Node " << Node << ".\nExiting Test ...\n" << endl;
            exit(0);
        }

    }
    else if(MapSize > 2097152)
    {
        nodemask_zero(&mask);
        nodemask_set_compat(&mask,Node);
        numa_set_membind_compat(&mask);
        LogicalAddressCopy = mmap(0, MapSize, PROT_READ | PROT_WRITE , MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB, -1, 0);
        if(LogicalAddressCopy == MAP_FAILED)
        {
            cerr << "Unable to allocate " << MapSize << " Bytes of memory.\nPlease make sure hugepages memory pool is allocated for Node " << Node << ".\nExiting Test ...\n" << endl;
            exit(0);
        }

    }

    /* ****** Fault the pages allocated and flush shortly after ****** */

    TouchPages((uint64_t)LogicalAddressCopy,MapSize);
    FlushCpuCache(LogicalAddressCopy,MapSize);

    return LogicalAddressCopy;
}

void Target :: FlushCpuCache(void *Mem, uint64_t MemSize)
{
    uint64_t CacheLine = 0x0, Address = 0x0;
    while(CacheLine < MemSize)
    {
        Address = (uint64_t)((char *)Mem + CacheLine);
        asm volatile("clflush (%0)"
                     :: "r"((void *)Address));
        asm volatile("mfence");
        CacheLine += 0x40;
    }
}

class Thread
{
public:
    sDeviceType mDevice;
    map<std::pair<int,int>,std::pair<int,int>> mOpcode; /* Flavours of Read/Write/Mixed */
    map <uint8_t,uint8_t> PrefetchState;                /* Prefetch State Holder */
    Target *Tgt;                                        /* Memory Buffer Handler for Threads */
public:
    Thread() { 
        Tgt = new Target();
    };

    virtual void CheckThread() {};
    virtual void SetupThread() {};
    virtual void InitializeThread() {};
    virtual void BuildThreadAddressTable(int) {};
    virtual void BuildThreadAddressTable() {};
    virtual void StartThread() {};
    virtual void StopThread(bool cancel) {};

};

#endif //__NUMAWARE_COMMON_H__