#include "cpu.h"

using namespace std;

void Thread_Cores :: InitializeThread()
{
    for(auto itr = mDevice.DeviceInfo.begin();itr != mDevice.DeviceInfo.end();itr++)
    {
        PrefetchState[itr->second] = readMSR(itr->second,0x1A4);
        writeMSR(itr->second,0x1A4,0x2F);
    }
}

void Thread_Cores :: StartThread()
{
    for(auto itr = mDevice.DeviceInfo.begin();itr != mDevice.DeviceInfo.end();itr++)
    {
        CreateWorkerThread(GenerateTrafficWrapper, (void *)this, &AllocatedTid[itr->second]);
        SetWorkerThreadAffinity(AllocatedTid[itr->second],itr->second);
    }
}

void Thread_Cores :: StopThread(bool cancel)
{
    for(auto itr = mDevice.DeviceInfo.begin();itr != mDevice.DeviceInfo.end();itr++)
    {
        TerminateWorkerThread(AllocatedTid[itr->second],cancel);
    }

    /* ****** Reset Prefetch State of Enabled Threads ****** */

    if(PrefetchDis)
    {
        for(auto itr = mDevice.DeviceInfo.begin();itr != mDevice.DeviceInfo.end(); itr++)
        {
            writeMSR(itr->second,0x1A4,PrefetchState[itr->second]);
        }
    }

    /* ****** Free Allocated Memory ****** */
    if(Tgt->MemTgt.Size < 4096)
        numa_free(Tgt->MemTgt.Virt,(Tgt->MemTgt.Size*mDevice.DeviceInfo.size()));
    else
        Tgt->UnmapMem((unsigned long *)Tgt->MemTgt.Virt,(Tgt->MemTgt.Size));
}

void Thread_Cores :: CheckThread()
{

}
void Thread_Cores :: SetupThread()
{

}

void Thread_Cores :: BuildThreadAddressTable(int Node)
{
    uint64_t AddrStart = 0x0,AddrStop = Tgt->MemTgt.Size;
    uint64_t MemoryMapSize;

    MemoryMapSize = ((mDevice.DeviceInfo.size())*(Tgt->MemTgt.Size));
    Tgt->MemTgt.Virt = (void *)Tgt->MapProcessToMemoryNode(Node,MemoryMapSize,true);
        
    for(auto itr = mDevice.DeviceInfo.begin(); itr != mDevice.DeviceInfo.end();itr++ )
    {
        Tgt->MemTgt.CoreAddrCfg[itr->second].AddrRangeStart = AddrStart;
        Tgt->MemTgt.CoreAddrCfg[itr->second].AddrRangeStop  = (AddrStart + AddrStop);
        Tgt->MemTgt.CoreAddrCfg[itr->second].AddrRangeStride = AddrStop;
        Tgt->MemTgt.CoreAddrCfg[itr->second].AddrRangeIteration  = (AddrStop/0x40);
        Tgt->MemTgt.CoreAddrCfg[itr->second].AddrType = 3;
        Tgt->MemTgt.CoreAddrCfg[itr->second].Size = (AddrStop/0x40);

        AddrStart += AddrStop;
    }
}

void* Thread_Cores :: GenerateTrafficWrapper(void* args)
{
    Thread_Cores *ThreadPtr = (Thread_Cores *)args;
    return reinterpret_cast<Thread_Cores*>(args)->GenerateTraffic(ThreadPtr);
}

void* Thread_Cores :: GenerateTraffic(Thread_Cores *Instance)
{
    printf("GenerateTraffic() is called. Running on %d\n",sched_getcpu());

    SetWorkerThreadCancelParams();

    /* ****** Thread Specific Variables ****** */

    uint8_t ThreadWriteData[64];                         /* Data to Write */
    int ThreadIndex = sched_getcpu();                    /* Thread Index */
    int Node = numa_node_of_cpu(ThreadIndex);            /* NUMA Node of Thread Index */
    bool done = false;                                   /* Thread Iteration Complete Flag */
    int RdData;                                          /* Hold Read Value */
    uint64_t StartThreadTick,StopThreadTick;             /* Latency Computing Tick Params */
	
    Instance->Tgt->MemTgt.CoreAddrCfg[ThreadIndex].AddressLoopCount = 0;

    do
    {
        MemWrCache((const void *)&ThreadWriteData[0],Instance,ThreadIndex);
        Instance->Tgt->MemTgt.CoreAddrCfg[ThreadIndex].AddressLoopCount +=1;	
    }while(!done);
}

inline void Thread_Cores :: MemWrCache(const void* src, Thread_Cores *Instance, int ThreadIndex)
{
    /* mov [rdx],rax */

    uint64_t NumLines = Instance->Tgt->MemTgt.CoreAddrCfg[ThreadIndex].Size;
    void *addr;

    for(int index=0;index<NumLines;index++)
    {
        addr = (void *)((char *)Instance->Tgt->MemTgt.Virt + Instance->Tgt->MemTgt.CoreAddrCfg[ThreadIndex].AddressArray[index]);

        asm volatile("mov %%rdx,(%%rax)"
                        : :"d"(src),"a"(addr));

        if(FencingEn)
            asm volatile("mfence");
    }
}