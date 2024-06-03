#include <windows.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <rtworkq.h>

using namespace std;

static inline int64_t GetTicks()
{
    LARGE_INTEGER ticks;
    if (!QueryPerformanceCounter(&ticks))
    {
        cout << "Error: QueryPerformanceCounter." << endl;
    }
    return ticks.QuadPart;
}

__interface IDeadlineTask
{
    virtual void Run() = 0;
};

class MatrixTask
{
public:
    MatrixTask();
    ~MatrixTask();
    int Allocate(int M, int K, int N);
    int Initialize();
    int Multiply();
    int Delete();

private:
    uint32_t m_M = 0;
    uint32_t m_K = 0;
    uint32_t m_N = 0;
    int *m_pA = NULL;
    int *m_pB = NULL;
    int *m_pC = NULL;
};

MatrixTask::MatrixTask()
{
}

MatrixTask::~MatrixTask()
{
}

int MatrixTask::Initialize()
{

    int *pA;
    int *pB;

    pA = m_pA;
    pB = m_pB;

    if (pA == NULL || pB == NULL)
    {
        return -1;
    }

    // Input elements for matrix A
    for (uint32_t m = 0; m < m_M; m++)
    {
        for (uint32_t k = 0; k < m_K; k++)
        {
            *pA++ = rand() % 256; // 1;
        }
    }

    // Input elements for matrix B
    for (uint32_t k = 0; k < m_K; k++)
    {
        for (uint32_t n = 0; n < m_N; n++)
        {
            *pB++ = rand() % 256; // 1;
        }
    }
    return 0;
}

int MatrixTask::Allocate(int M, int K, int N)
{
    // allocate Matrix
    m_M = M;
    m_K = K;
    m_N = N;

    m_pA = new int[M * K]; // [M] [K] ;
    if (m_pA == NULL)
    {
        return -1;
    }
    m_pB = new int[K * N]; // [K] [N] ;
    if (m_pB == NULL)
    {
        delete[] m_pA;
        return -2;
    }
    m_pC = new int[M * N]; // [M] [N] ; // stores result
    if (m_pC == NULL)
    {
        delete[] m_pA;
        delete[] m_pB;
        return -3;
    }
    return 0;
}

int MatrixTask::Multiply()
{
    int *pA;
    int *pB;
    int *pC;

    pA = m_pA;
    pB = m_pB;
    pC = m_pC;

    for (uint32_t m = 0; m < m_M; m++)
    {
        for (uint32_t n = 0; n < m_N; n++)
        {
            int sum = 0;
            for (uint32_t k = 0; k < m_K; k++)
            {
                sum += pA[(m * m_K) + k] * pB[(k * m_N) + n];
            }
            pC[(m * m_N) + n] = sum;
        }
    }
    return 0;
}

int MatrixTask::Delete()
{
    if (m_pA)
    {
        delete (m_pA);
        m_pC = NULL;
    }
    if (m_pB)
    {
        delete (m_pB);
        m_pC = NULL;
    }
    if (m_pC)
    {
        delete (m_pC);
        m_pC = NULL;
    }
    return 0;
}

// global hack until move to separate files
MatrixTask g_matrixTask;

class DBSTask : public IDeadlineTask
{
public:
    DBSTask();
    ~DBSTask();
    void Initialize();
    void Run();

private:
    //    MatrixTask m_matrixTask;
};

DBSTask::DBSTask()
{
}

DBSTask::~DBSTask()
{
}

void DBSTask::Initialize()
{
}

void DBSTask::Run()
{
    cout << "Test";
    g_matrixTask.Multiply();
    return;
}

class DECLSPEC_UUID("DEE26635-F6D6-4ABC-B222-1CA3076A39CF") RtwqAsyncCallbackImpl : public IRtwqAsyncCallback
{
private:
    LONG volatile *m_cRef;
    DWORD m_workQueueId;
    HANDLE m_taskComplete;
    LARGE_INTEGER m_submitTime;
    std::shared_ptr<IDeadlineTask> m_task;
    ~RtwqAsyncCallbackImpl();

public:
    RtwqAsyncCallbackImpl(DWORD workQueueId, HANDLE finishEvent, LARGE_INTEGER submitTime, std::shared_ptr<IDeadlineTask> work);
    HRESULT Invoke(IRtwqAsyncResult *pAsyncResult);
    HRESULT GetParameters(DWORD *pdwFlags, DWORD *pdwQueue);
    HRESULT QueryInterface(REFIID riid, void **ppvObj);
    ULONG AddRef();
    ULONG Release();
};

HRESULT RtwqAsyncCallbackImpl::Invoke(IRtwqAsyncResult *pAsyncResult)
{
    int ret;
    double startTime, stopTime, timeInMs;

    startTime = (double ) GetTicks();

    m_task->Run();
    stopTime = (double ) GetTicks();

    timeInMs = (stopTime - startTime) / 1000;
    cout << "time: " << timeInMs << " ms" << endl;

    SetEvent(m_taskComplete);
    return S_OK;
}

RtwqAsyncCallbackImpl::~RtwqAsyncCallbackImpl()
{
    delete m_cRef;
}

RtwqAsyncCallbackImpl::RtwqAsyncCallbackImpl(DWORD workQueueId, HANDLE taskComplete, LARGE_INTEGER submitTime, std::shared_ptr<IDeadlineTask> work)
    : m_workQueueId(workQueueId), m_cRef(new LONG(0)), m_taskComplete(taskComplete), m_submitTime(submitTime), m_task(work)
{
    AddRef();
}

HRESULT RtwqAsyncCallbackImpl::GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)
{
    *pdwFlags = 0;
    *pdwQueue = m_workQueueId;
    return S_OK;
}

HRESULT RtwqAsyncCallbackImpl::QueryInterface(REFIID riid, void **ppvObj)
{
    if (!ppvObj)
        return E_INVALIDARG;
    *ppvObj = NULL;
    if (riid == IID_IUnknown || riid == __uuidof(IRtwqAsyncCallback) ||
        riid == __uuidof(RtwqAsyncCallbackImpl))
    {
        *ppvObj = (LPVOID)this;
        AddRef();
        return NOERROR;
    }
    return E_NOINTERFACE;
}

ULONG RtwqAsyncCallbackImpl::AddRef()
{
    return InterlockedIncrement(m_cRef);
}

ULONG RtwqAsyncCallbackImpl::Release()
{
    ULONG ulRefCount = InterlockedDecrement(m_cRef);
    if (0 == ulRefCount)
    {
        delete this;
    }
    return ulRefCount;
}

class WorkQueue
{
public:
    WorkQueue();
    ~WorkQueue();
    int createAsyncResult();
    int initializeRtworkQ();
    int lockWorkQ();
    int putWorkItem();
    int setDeadline2(uint32_t deadLineHNS, double preDeadline);
    int setLongRunning(BOOL bSet);
    int waitForWorkToComplete();

private:
    IRtwqAsyncResult *m_result = nullptr;
    RtwqAsyncCallbackImpl *cb = nullptr;
    HANDLE m_hEvent = NULL;
    DWORD m_workQueueId = 0;
    LARGE_INTEGER m_submitQPC;
};

WorkQueue::WorkQueue()
{
    m_hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

WorkQueue::~WorkQueue()
{
    if (m_hEvent)
        CloseHandle(m_hEvent);
}
int WorkQueue::initializeRtworkQ()
{
    int ret;
    HRESULT hr;
    hr = RtwqStartup();
    if (hr != S_OK)
        ret = -1;
    else
        ret = 0;
    return ret;
}

int WorkQueue::lockWorkQ()
{
    HRESULT hr;
    hr = RtwqLockSharedWorkQueue(L"Playback", 0, 0, &m_workQueueId);
    return 0;
}

int WorkQueue::setLongRunning(BOOL bSet)
{
    HRESULT hr;
    hr = RtwqSetLongRunning(m_workQueueId, bSet);
    return 0;
}

int WorkQueue::createAsyncResult()
{
    HRESULT hr;
    auto workload = std::make_shared<DBSTask>();

    cb = new RtwqAsyncCallbackImpl(m_workQueueId, m_hEvent, m_submitQPC, workload);
    hr = RtwqCreateAsyncResult(NULL, cb, NULL, &m_result);
    return 0;
}

int WorkQueue::setDeadline2(uint32_t deadLineHNS, double preDeadline)
{
    HRESULT hr;
    HANDLE deadlineRequest = NULL;
    LONGLONG deadlineHNS = (LONGLONG)deadLineHNS;
    hr = RtwqSetDeadline2(m_workQueueId, deadlineHNS, (LONGLONG)((double)deadLineHNS * preDeadline), &deadlineRequest);
    return 0;
}

int WorkQueue::putWorkItem()
{
    HRESULT hr;
    hr = RtwqPutWorkItem(m_workQueueId, 0, m_result);
    return 0;
}

int WorkQueue::waitForWorkToComplete()
{
    DWORD result = WaitForSingleObject(m_hEvent, INFINITE);
    return 0;
}

int getParams(int argc, char *argv[], uint32_t &M, uint32_t &K, uint32_t &N, uint32_t &Scale, uint32_t &Iterations,
              uint32_t &ShortWorkload, uint32_t &LongWorkload)
{

    if (argc != 8)
    {
        cerr << "Usage: " << argv[0] << " M K N scale iterations shortWorkload longWorkload" << std::endl;
        return -1;
    }

    M = stoi(argv[1]);
    K = stoi(argv[2]);
    N = stoi(argv[3]);
    Scale = stoi(argv[4]);
    Iterations = stoi(argv[5]);
    ShortWorkload = stoi(argv[6]);
    LongWorkload = stoi(argv[7]);

    return 0;
}

// main
int main(int argc, char *argv[])
{
    // local main variables
    uint32_t M;
    uint32_t K;
    uint32_t N;
    uint32_t Scale;
    uint32_t Iterations;
    uint32_t ShortWorkload;
    uint32_t LongWorkload;
    uint32_t deadlineHNS;
    WorkQueue dbsRtworkQ;
    int ret;

    std::cout << "deadline based task scheduling." << std::endl << endl;

    // get task parameters
    ret = getParams(argc, argv, M, K, N, Scale, Iterations, ShortWorkload, LongWorkload);
    if (ret != 0)
        return -1;

    // setup realtime work queue
    ret = dbsRtworkQ.initializeRtworkQ();
    ret = dbsRtworkQ.lockWorkQ();
    deadlineHNS = ShortWorkload * 10000; // multiply ms by 100 to get HNS
    ret = dbsRtworkQ.setDeadline2(deadlineHNS, 0.80);
    ret = dbsRtworkQ.createAsyncResult();

    std::cout << "small matrix task" << std::endl;
    // initialize the matrix data
    g_matrixTask.Allocate(M, K, N);
    g_matrixTask.Initialize();
    for (uint32_t i = 0; i < Iterations; i++)
    {
        ret = dbsRtworkQ.setLongRunning(TRUE);
        ret = dbsRtworkQ.putWorkItem();
        ret = dbsRtworkQ.waitForWorkToComplete();
        ret = dbsRtworkQ.setLongRunning(FALSE);
    }
    g_matrixTask.Delete();
    cout << endl;

    std::cout << "large matrix task" << std::endl;
    // initialize the matrix data
    g_matrixTask.Allocate(M * Scale, K * Scale, N * Scale);
    g_matrixTask.Initialize();
    for (uint32_t i = 0; i < Iterations; i++)
    {
        ret = dbsRtworkQ.setLongRunning(TRUE);
        ret = dbsRtworkQ.putWorkItem();
        ret = dbsRtworkQ.waitForWorkToComplete();
        ret = dbsRtworkQ.setLongRunning(FALSE);
    }
    g_matrixTask.Delete();
    cout << endl;

}
