#pragma once

#ifndef __NUMAWARE_CORES_H__
#define __NUMAWARE_CORES_H__

#include "common.h"

using namespace std;

class Thread_Cores : public Thread
{
private:
    int AllocatedTid[MAX_THREADS];

public:
    Thread_Cores() {};
    void CheckThread();
    void InitializeThread();
    void StartThread();
    void StopThread(bool cancel);
    void BuildThreadAddressTable(int Node);
    void BuildThreadAddressTable();
    void SetupThread();

private:
    static void* GenerateTrafficWrapper(void *args);
    void* GenerateTraffic(Thread_Cores *Instance);
};

#endif //__NUMAWARE_CORES_H__