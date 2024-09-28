
#include <stdlib.h>
#include <string.h>

/* 
 * 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 宏可以防止 task.h 重新定义
 * 所有的 API 函数以使用 MPU 包装函数。这应该只在 task.h 被包含在
 * 应用程序文件中时进行。
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if ( configUSE_CO_ROUTINES == 1 )
	#include "croutine.h"
#endif

/* 
 * 抑制 lint 错误 e961 和 e750，因为这是一个被允许的 MISRA 例外情况。
 * 由于 MPU 端口要求在上述头文件中定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
 * 但在本文件中不需要定义，以便生成正确的特权与非特权链接和放置。
 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750.*/


/* 
 * 用于 cRxLock 和 cTxLock 结构成员的常量。
 */
#define queueUNLOCKED                    ( ( int8_t ) -1 ) // 定义队列解锁状态
#define queueLOCKED_UNMODIFIED           ( ( int8_t ) 0 )  // 定义队列锁定但未修改的状态

/* 
 * 当 Queue_t 结构用于表示基本队列时，其 pcHead 和 pcTail 成员作为指向队列存储区的指针。
 * 当 Queue_t 结构用于表示互斥量时，pcHead 和 pcTail 指针是不必要的，并且 pcHead 指针被设置为 NULL，
 * 以指示 pcTail 指针实际上指向互斥量持有者（如果有）。为了确保代码的可读性，尽管存在对两个结构成员的双重使用，
 * 仍然将替代名称映射到 pcHead 和 pcTail 结构成员。另一种实现方法是使用联合体（union），但使用联合体违反了编码标准
 * （尽管在双重使用也显著改变了结构成员类型的情况下允许对标准的例外）。
 */
#define pxMutexHolder                    pcTail // 互斥量持有者的别名
#define uxQueueType                      pcHead // 队列类型的别名
#define queueQUEUE_IS_MUTEX              NULL   // 表示队列是互斥量的标志

/* 
 * 信号量实际上不存储也不复制数据，因此其项目大小为零。
 */
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH  ( ( UBaseType_t ) 0 )
#define queueMUTEX_GIVE_BLOCK_TIME        ( ( TickType_t ) 0U )

#if( configUSE_PREEMPTION == 0 )
    /* 如果使用的是协作式调度器，则不应仅仅因为唤醒了一个优先级更高的任务就执行调度让步。 */
    #define queueYIELD_IF_USING_PREEMPTION()
#else
    /* 如果使用的是抢占式调度器，则需要执行调度让步。 */
    #define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/*
 * 定义了调度器使用的队列。
 * 项目是通过拷贝而非引用排队的。详情请参见以下链接：http://www.freertos.org/Embedded-RTOS-Queues.html
 */
typedef struct QueueDefinition
{
    int8_t *pcHead;                    /*!< 指向队列存储区的起始位置。 */
    int8_t *pcTail;                    /*!< 指向队列存储区末尾的字节。分配的字节数比存储队列项所需的多一个，用于作为标记。 */
    int8_t *pcWriteTo;                 /*!< 指向存储区中的下一个空闲位置。 */

    union                              /*!< 使用联合体是对编码标准的一个例外，以确保两个互斥的结构成员不会同时出现（浪费 RAM）。 */
    {
        int8_t *pcReadFrom;            /*!< 当结构用作队列时，指向最后一次从中读取队列项的位置。 */
        UBaseType_t uxRecursiveCallCount; /*!< 当结构用作互斥量时，记录了递归“获取”互斥量的次数。 */
    } u;

    List_t xTasksWaitingToSend;        /*!< 等待向此队列发送数据的被阻塞任务列表。按优先级顺序存储。 */
    List_t xTasksWaitingToReceive;     /*!< 等待从此队列读取数据的被阻塞任务列表。按优先级顺序存储。 */

    volatile UBaseType_t uxMessagesWaiting; /*!< 当前队列中的项数。 */
    UBaseType_t uxLength;              /*!< 队列的长度，定义为队列可以容纳的项数，而不是字节数。 */
    UBaseType_t uxItemSize;            /*!< 队列将保存的每项的大小。 */

    volatile int8_t cRxLock;           /*!< 存储在队列锁定期间从队列接收（移除）的项数。当队列未锁定时设置为 queueUNLOCKED。 */
    volatile int8_t cTxLock;           /*!< 存储在队列锁定期间向队列发送（添加）的项数。当队列未锁定时设置为 queueUNLOCKED。 */

    #if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated; /*!< 如果使用静态分配的内存，则设置为 pdTRUE，以确保不会尝试释放内存。 */
    #endif

    #if ( configUSE_QUEUE_SETS == 1 )
        struct QueueDefinition *pxQueueSetContainer; /*!< 如果启用了队列集功能，则指向队列所属的队列集。 */
    #endif

    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxQueueNumber;     /*!< 队列编号，用于跟踪设施。 */
        uint8_t ucQueueType;            /*!< 队列类型，用于跟踪设施。 */
    #endif

} xQUEUE;

/*
 * 保留旧的 xQUEUE 名称，并将其转换为新的 Queue_t 名称，
 * 以便能够使用较旧的内核感知调试器。
 */
typedef xQUEUE Queue_t;

/*-----------------------------------------------------------*/

/*
 * 队列注册表只是内核感知调试器定位队列结构的一种手段。
 * 它没有其他用途，因此是一个可选组件。
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )

    /* 
     * 存储在队列注册表数组中的类型。
     * 这允许为每个队列分配一个名称，使内核感知调试更加用户友好。
     */
    typedef struct QUEUE_REGISTRY_ITEM
    {
        const char *pcQueueName; /*!< lint !e971 允许使用未限定的 char 类型用于字符串和单字符。 */
        QueueHandle_t xHandle;   /*!< 队列的句柄。 */
    } xQueueRegistryItem;

    /* 
     * 保留旧的 xQueueRegistryItem 名称，并将其转换为新的 QueueRegistryItem_t 名称，
     * 以便能够使用较旧的内核感知调试器。
     */
    typedef xQueueRegistryItem QueueRegistryItem_t;

    /* 
     * 队列注册表只是一个 QueueRegistryItem_t 结构的数组。
     * 结构中的 pcQueueName 成员为 NULL 表示该数组位置是空闲的。
     */
    PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];

#endif /* configQUEUE_REGISTRY_SIZE */

/*
 * 解锁由 prvLockQueue 调用锁定的队列。
 * 锁定队列并不会阻止中断服务例程（ISR）向队列添加或移除项，但会阻止中断服务例程从队列事件列表中移除任务。
 * 如果中断服务例程发现队列被锁定，它将递增相应的队列锁定计数，以指示可能需要解除任务的阻塞。
 * 当队列解锁时，会检查这些锁定计数，并采取适当的措施。
 */
static void prvUnlockQueue(Queue_t * const pxQueue) PRIVILEGED_FUNCTION;

/*
 * 使用临界区来确定队列中是否有数据。
 *
 * @return 如果队列中没有任何项，则返回 pdTRUE，否则返回 pdFALSE。
 */
static BaseType_t prvIsQueueEmpty(const Queue_t *pxQueue) PRIVILEGED_FUNCTION;

/*
 * 使用临界区来确定队列中是否有空间。
 *
 * @return 如果队列中没有空间，则返回 pdTRUE，否则返回 pdFALSE。
 */
static BaseType_t prvIsQueueFull(const Queue_t *pxQueue) PRIVILEGED_FUNCTION;

/*
 * 将一项数据复制到队列中，可以复制到队列的前端或后端。
 */
static BaseType_t prvCopyDataToQueue(Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition) PRIVILEGED_FUNCTION;

/*
 * 从队列中复制一项数据。
 */
static void prvCopyDataFromQueue(Queue_t * const pxQueue, void * const pvBuffer) PRIVILEGED_FUNCTION;

#if (configUSE_QUEUE_SETS == 1)
    /*
     * 检查队列是否属于队列集，并如果是的话，通知队列集该队列包含数据。
     */
    static BaseType_t prvNotifyQueueSetContainer(const Queue_t * const pxQueue, const BaseType_t xCopyPosition) PRIVILEGED_FUNCTION;
#endif

/*
 * 在静态或动态分配 Queue_t 结构后调用，以填充结构的成员。
 */
static void prvInitialiseNewQueue(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue) PRIVILEGED_FUNCTION;

/*
 * 互斥量是一种特殊的队列类型。当创建一个互斥量时，首先创建队列，
 * 然后调用 prvInitialiseMutex() 函数来将队列配置为互斥量。
 */
#if( configUSE_MUTEXES == 1 )
    static void prvInitialiseMutex(Queue_t *pxNewQueue) PRIVILEGED_FUNCTION;
#endif

/*-----------------------------------------------------------*/

/*
 * 标记队列为锁定状态的宏。锁定队列可以防止中断服务例程（ISR）访问队列事件列表。
 */
#define prvLockQueue(pxQueue) \
    taskENTER_CRITICAL();     \
    {                         \
        if ((pxQueue)->cRxLock == queueUNLOCKED)          \
        {                                                  \
            (pxQueue)->cRxLock = queueLOCKED_UNMODIFIED;  \
        }                                                  \
        if ((pxQueue)->cTxLock == queueUNLOCKED)          \
        {                                                  \
            (pxQueue)->cTxLock = queueLOCKED_UNMODIFIED;  \
        }                                                  \
    }                                                      \
    taskEXIT_CRITICAL()
/*-----------------------------------------------------------*/

/**
 * @brief 重置队列的状态。
 * 
 * 此函数用于重置队列，使其恢复到初始状态，清空队列中的所有消息，并根据参数 xNewQueue 的值来决定是否重新初始化队列的等待列表。
 * 
 * @param xQueue 需要被重置的队列的句柄。
 * @param xNewQueue 一个标志位，用于指示是否像初始化新队列一样处理队列：
 *                  - 如果为 pdFALSE，则不初始化等待列表。
 *                  - 如果为 pdTRUE，则初始化等待列表。
 * 
 * @return 返回 pdPASS，用于与先前版本的语义一致性。
 */
BaseType_t xQueueGenericReset(QueueHandle_t xQueue, BaseType_t xNewQueue)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 确保队列句柄不是 NULL
    configASSERT(pxQueue);

    // 进入临界区，确保对队列状态的修改是原子的
    taskENTER_CRITICAL();
    {
        // 将队列的尾指针重置为队列存储区的末尾位置
        pxQueue->pcTail = pxQueue->pcHead + (pxQueue->uxLength * pxQueue->uxItemSize);

        // 将队列中的消息数量重置为 0
        pxQueue->uxMessagesWaiting = (UBaseType_t)0U;

        // 将写指针重置为队列存储区的起始位置
        pxQueue->pcWriteTo = pxQueue->pcHead;

        // 将读指针重置为队列最后一个项的后面（如果队列长度为 n，则读指针指向 n * 项大小）
        pxQueue->u.pcReadFrom = pxQueue->pcHead + ((pxQueue->uxLength - (UBaseType_t)1U) * pxQueue->uxItemSize);

        // 将接收和发送的锁定计数重置为未锁定状态
        pxQueue->cRxLock = queueUNLOCKED;
        pxQueue->cTxLock = queueUNLOCKED;

        // 根据 xNewQueue 参数值来决定是否初始化等待列表
        if (xNewQueue == pdFALSE)
        {
            /* 如果有任务因为等待从队列读取数据而被阻塞，那么这些任务将保持阻塞状态，
             * 因为当此函数退出后，队列仍然是空的。
             * 如果有任务因为等待向队列写入数据而被阻塞，那么应该解除其中一个任务的阻塞，
             * 因为当此函数退出后，将可以向队列写入数据。 */
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
            {
                if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                {
                    queueYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            /* 确保事件队列开始时处于正确的状态。 */
            vListInitialise(&(pxQueue->xTasksWaitingToSend));
            vListInitialise(&(pxQueue->xTasksWaitingToReceive));
        }
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    /* 为了与先前版本的调用语义保持一致，返回一个值。 */
    return pdPASS;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
/**
 * @brief 静态创建一个队列。
 * 
 * 此函数用于静态创建一个队列，包括初始化队列的各个字段，并根据提供的参数配置队列的长度和项大小。
 * 
 * @param uxQueueLength 队列的长度，定义为队列可以容纳的项数。
 * @param uxItemSize 每个队列项的大小（以字节为单位）。
 * @param pucQueueStorage 指向静态分配的队列存储区域的指针。
 * @param pxStaticQueue 指向静态分配的队列结构的指针。
 * @param ucQueueType 队列的类型。
 * 
 * @return 返回指向新创建队列的句柄。
 */
QueueHandle_t xQueueGenericCreateStatic(
    const UBaseType_t uxQueueLength,
    const UBaseType_t uxItemSize,
    uint8_t *pucQueueStorage,
    StaticQueue_t *pxStaticQueue,
    const uint8_t ucQueueType
)
{
    Queue_t *pxNewQueue;

    // 确保队列长度大于零
    configASSERT(uxQueueLength > (UBaseType_t)0);

    // 静态队列结构和队列存储区必须提供
    configASSERT(pxStaticQueue != NULL);

    // 如果项大小不为零，则应提供队列存储区；如果项大小为零，则不应提供队列存储区
    configASSERT(!(pucQueueStorage != NULL && uxItemSize == 0));
    configASSERT(!(pucQueueStorage == NULL && uxItemSize != 0));

    #if (configASSERT_DEFINED == 1)
    {
        // 确认 StaticQueue_t 结构的大小等于实际队列结构的大小
        volatile size_t xSize = sizeof(StaticQueue_t);
        configASSERT(xSize == sizeof(Queue_t));
    }
    #endif /* configASSERT_DEFINED */

    // 使用传递进来的静态分配的队列地址
    pxNewQueue = (Queue_t *)pxStaticQueue; /*lint !e740 特殊的类型转换是合理的，因为这些结构设计成具有相同的对齐方式，并且大小通过断言进行检查。 */

    if (pxNewQueue != NULL)
    {
        #if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
        {
            // 队列可以静态或动态分配，因此记录该队列是静态分配的，以防以后删除队列时尝试释放内存
            pxNewQueue->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        // 初始化新的队列结构
        prvInitialiseNewQueue(uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue);
    }

    // 返回新创建的队列句柄
    return pxNewQueue;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

/*
 * @brief xQueueGenericCreate 创建一个队列。
 *
 * 该函数用于创建一个指定长度和元素大小的队列，并根据需要初始化队列结构。
 *
 * @param uxQueueLength 队列的长度，即队列可以容纳的元素数量。
 * @param uxItemSize 队列中每个元素的大小（以字节为单位）。
 * @param ucQueueType 队列的类型，用于区分不同类型的队列。
 *
 * @return 返回创建的队列句柄。如果创建失败，则返回 NULL。
 */
QueueHandle_t xQueueGenericCreate(
    const UBaseType_t uxQueueLength, 
    const UBaseType_t uxItemSize, 
    const uint8_t ucQueueType )
{
    Queue_t *pxNewQueue;                     // 定义指向队列结构的指针
    size_t xQueueSizeInBytes;                // 计算队列所需的字节数
    uint8_t *pucQueueStorage;                // 指向队列存储区的指针

    // 确保队列长度大于0
    configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

    // 计算队列所需的字节数
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        xQueueSizeInBytes = ( size_t ) 0;
    }
    else
    {
        xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize );
    }

    // 分配队列结构及存储区所需的空间
    // 使用 pvPortMalloc 分配内存，这是一个便携的内存分配函数
    pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

    if( pxNewQueue != NULL )
    {
        // 计算队列存储区的起始地址
        // 队列存储区紧跟着队列结构之后
        pucQueueStorage = ( ( uint8_t * ) pxNewQueue ) + sizeof( Queue_t );

        // 如果启用了静态分配支持，则设置队列的标志
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            pxNewQueue->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        // 初始化队列结构
        // 传入队列长度、元素大小、存储区地址、队列类型以及队列结构指针
        prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
    }

    // 返回队列句柄
    return pxNewQueue;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/**
 * @brief 初始化一个新的队列。
 * 
 * 此函数用于初始化一个新的队列结构，包括设置队列的基本属性，如长度、项大小等，并根据配置选项执行额外的初始化步骤。
 * 
 * @param uxQueueLength 队列的长度（队列可以容纳的项数）。
 * @param uxItemSize 每个队列项的大小（以字节为单位）。
 * @param pucQueueStorage 指向队列存储区域的指针。
 * @param ucQueueType 队列的类型。
 * @param pxNewQueue 指向新队列结构的指针。
 */
static void prvInitialiseNewQueue(
    const UBaseType_t uxQueueLength,
    const UBaseType_t uxItemSize,
    uint8_t *pucQueueStorage,
    const uint8_t ucQueueType,
    Queue_t *pxNewQueue
)
{
    // 移除编译器关于未使用参数的警告，除非启用了跟踪设施
    (void) ucQueueType;

    if (uxItemSize == (UBaseType_t)0)
    {
        // 如果没有为队列分配存储区，但是 pcHead 不能设置为 NULL，因为 NULL 用来表示队列作为互斥量使用。
        // 因此，将 pcHead 设置为指向队列本身的一个无害值，这个值在内存映射中是已知的。
        pxNewQueue->pcHead = (int8_t *)pxNewQueue;
    }
    else
    {
        // 将 pcHead 设置为指向队列存储区的起始位置。
        pxNewQueue->pcHead = (int8_t *)pucQueueStorage;
    }

    // 初始化队列成员，如队列的长度和项大小等。
    pxNewQueue->uxLength = uxQueueLength;
    pxNewQueue->uxItemSize = uxItemSize;

    // 重置队列，初始化队列的状态。
    (void)xQueueGenericReset(pxNewQueue, pdTRUE);

    #if (configUSE_TRACE_FACILITY == 1)
    {
        // 如果启用了跟踪设施，则记录队列类型。
        pxNewQueue->ucQueueType = ucQueueType;
    }
    #endif /* configUSE_TRACE_FACILITY */

    #if (configUSE_QUEUE_SETS == 1)
    {
        // 如果启用了队列集功能，则初始化队列集容器指针为 NULL。
        pxNewQueue->pxQueueSetContainer = NULL;
    }
    #endif /* configUSE_QUEUE_SETS */

    // 记录队列创建的跟踪信息。
    traceQUEUE_CREATE(pxNewQueue);
}
/*-----------------------------------------------------------*/

#if( configUSE_MUTEXES == 1 )

	/**
 * @brief 初始化一个互斥量。
 * 
 * 此函数用于将一个普通队列转换为互斥量，并设置互斥量所需的特定成员。
 * 
 * @param pxNewQueue 指向新创建的队列结构的指针。
 */
static void prvInitialiseMutex(Queue_t *pxNewQueue)
{
    if (pxNewQueue != NULL)
    {
        // 队列创建函数已经为通用队列正确设置了所有队列结构成员，但此函数正在创建一个互斥量。
        // 重写那些需要不同设置的成员，特别是优先级继承所需的信息。
        pxNewQueue->pxMutexHolder = NULL;
        pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

        // 如果这是一个递归互斥量。
        pxNewQueue->u.uxRecursiveCallCount = 0;

        // 记录互斥量创建的跟踪信息。
        traceCREATE_MUTEX(pxNewQueue);

        // 使信号量处于预期状态。
        (void)xQueueGenericSend(pxNewQueue, NULL, (TickType_t)0U, queueSEND_TO_BACK);
    }
    else
    {
        // 如果队列指针为 NULL，则记录互斥量创建失败的跟踪信息。
        traceCREATE_MUTEX_FAILED();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/*
 * @brief xQueueCreateMutex 创建一个互斥信号量。
 *
 * 该函数根据提供的类型创建一个用于互斥访问控制的队列，并对其进行初始化。
 *
 * @param ucQueueType 指定互斥信号量的类型，例如二进制信号量或递归信号量等。
 *
 * @return 返回创建的互斥信号量句柄。如果创建失败，则返回 NULL。
 */
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
    Queue_t *pxNewQueue;                    // 定义指向队列结构的指针
    const UBaseType_t uxMutexLength = ( UBaseType_t ) 1; // 定义互斥信号量的长度为1
    const UBaseType_t uxMutexSize = ( UBaseType_t ) 0;   // 定义互斥信号量的大小为0（无需额外空间）

    // 通过通用队列创建函数创建互斥信号量
    // uxMutexLength 表示队列能存放的元素数量，在这里是1，因为互斥量只能有一个持有者。
    // uxMutexSize 表示队列中每个元素的大小，在这里是0，因为互斥量不需要存储任何数据。
    pxNewQueue = ( Queue_t * ) xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );

    // 如果创建成功，则初始化互斥信号量
    // 初始化过程通常包括设置一些内部状态，比如所有权标志等。
    if( pxNewQueue != NULL )
    {
        prvInitialiseMutex( pxNewQueue );
    }

    // 返回创建的互斥信号量句柄
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

/**
 * @brief 静态创建一个互斥量。
 * 
 * 此函数用于静态创建一个互斥量，并初始化其状态。
 * 
 * @param ucQueueType 互斥量的类型。
 * @param pxStaticQueue 指向静态分配的队列结构的指针。
 * 
 * @return 返回指向新创建互斥量的句柄。
 */
QueueHandle_t xQueueCreateMutexStatic(const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue)
{
    Queue_t *pxNewQueue;
    const UBaseType_t uxMutexLength = (UBaseType_t)1, uxMutexSize = (UBaseType_t)0;

    // 防止编译器在 configUSE_TRACE_FACILITY 不等于 1 时发出未使用的参数警告。
    (void)ucQueueType;

    // 使用指定的静态队列结构创建一个长度为 1、项大小为 0 的队列。
    pxNewQueue = (Queue_t *)xQueueGenericCreateStatic(uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType);

    // 初始化互斥量。
    prvInitialiseMutex(pxNewQueue);

    // 返回新创建的互斥量句柄。
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

/**
 * @brief 获取互斥量的持有者。
 * 
 * 此函数用于获取指定互斥量的持有者。注意，此函数由 xSemaphoreGetMutexHolder() 调用，不应该直接调用。
 * 注意：这是判断调用任务是否为互斥量持有者的良好方法，但不是确定互斥量持有者身份的好方法，因为持有者可能在以下临界区退出和函数返回之间发生变化。
 * 
 * @param xSemaphore 指向互斥量的句柄。
 * 
 * @return 返回指向互斥量持有者的指针，如果没有持有者，则返回 NULL。
 */
void* xQueueGetMutexHolder(QueueHandle_t xSemaphore)
{
    void *pxReturn;

    // 进入临界区，以确保对互斥量状态的访问是原子的
    taskENTER_CRITICAL();
    {
        // 检查传入的句柄是否代表一个互斥量
        if (((Queue_t *)xSemaphore)->uxQueueType == queueQUEUE_IS_MUTEX)
        {
            // 获取互斥量持有者的指针
            pxReturn = (void *)((Queue_t *)xSemaphore)->pxMutexHolder;
        }
        else
        {
            // 如果不是互斥量，则返回 NULL
            pxReturn = NULL;
        }
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    // 返回互斥量持有者的指针
    return pxReturn;
} /*lint !e818 xSemaphore 不能是指向常量的指针，因为它是一个类型定义。*/

#endif
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

/**
 * @brief 递归地释放互斥量。
 * 
 * 此函数用于递归地释放一个互斥量。如果当前任务持有该互斥量，则减少递归计数。如果递归计数变为零，则将互斥量归还给队列。
 * 
 * @param xMutex 互斥量的句柄。
 * 
 * @return 返回 pdPASS 表示成功释放互斥量，返回 pdFAIL 表示释放失败（当前任务不持有互斥量）。
 */
BaseType_t xQueueGiveMutexRecursive(QueueHandle_t xMutex)
{
    BaseType_t xReturn; // 返回值
    Queue_t * const pxMutex = (Queue_t *)xMutex; // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT(pxMutex);

    // 检查当前任务是否已经持有该互斥量
    if (pxMutex->pxMutexHolder == (void *)xTaskGetCurrentTaskHandle()) /*lint !e961 Not a redundant cast as TaskHandle_t is a typedef.*/
    {
        // 记录递归释放互斥量的操作
        traceGIVE_MUTEX_RECURSIVE(pxMutex);

        // 减少递归调用计数
        (pxMutex->u.uxRecursiveCallCount)--;

        // 如果递归调用计数变为0，则表示当前任务不再持有互斥量
        if (pxMutex->u.uxRecursiveCallCount == (UBaseType_t)0)
        {
            // 将互斥量归还给队列
            (void)xQueueGenericSend(pxMutex, NULL, queueMUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK);
        }
        else
        {
            // 如果递归调用计数不为0，则表示当前任务仍然持有互斥量
            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
        }

        // 设置返回值为成功
        xReturn = pdPASS;
    }
    else
    {
        // 如果当前任务没有持有互斥量，则不能释放
        xReturn = pdFAIL;

        // 记录递归释放互斥量失败的信息
        traceGIVE_MUTEX_RECURSIVE_FAILED(pxMutex);
    }

    // 返回释放互斥量的结果
    return xReturn;
}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

/**
 * @brief 递归地获取互斥量。
 * 
 * 此函数用于递归地获取一个互斥量。如果当前任务已经持有该互斥量，则增加递归计数。否则，尝试从队列中接收数据（实际上是尝试获取互斥量）。
 * 
 * @param xMutex 互斥量的句柄。
 * @param xTicksToWait 等待时间（以 tick 为单位）。
 * 
 * @return 返回 pdPASS 表示成功获取互斥量，返回 pdFAIL 表示获取失败。
 */
BaseType_t xQueueTakeMutexRecursive(QueueHandle_t xMutex, TickType_t xTicksToWait)
{
    BaseType_t xReturn; // 返回值
    Queue_t * const pxMutex = (Queue_t *)xMutex; // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT(pxMutex);

    // 记录递归获取互斥量的操作
    traceTAKE_MUTEX_RECURSIVE(pxMutex);

    // 检查当前任务是否已经持有该互斥量
    if (pxMutex->pxMutexHolder == (void *)xTaskGetCurrentTaskHandle()) /*lint !e961 Cast is not redundant as TaskHandle_t is a typedef.*/
    {
        // 如果当前任务已经持有该互斥量，则增加递归调用计数
        (pxMutex->u.uxRecursiveCallCount)++;
        xReturn = pdPASS; // 设置返回值为成功
    }
    else
    {
        // 尝试从队列中接收数据（实际上是尝试获取互斥量）
        xReturn = xQueueGenericReceive(pxMutex, NULL, xTicksToWait, pdFALSE);

        if (xReturn != pdFAIL)
        {
            // 如果成功获取互斥量，则增加递归调用计数
            (pxMutex->u.uxRecursiveCallCount)++;
        }
        else
        {
            // 如果未能获取互斥量，则记录获取失败信息
            traceTAKE_MUTEX_RECURSIVE_FAILED(pxMutex);
        }
    }

    // 返回获取互斥量的结果
    return xReturn;
}
#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

/**
 * @brief 静态创建一个计数信号量。
 * 
 * 此函数用于静态创建一个计数信号量，并初始化其最大计数值和初始计数值。
 * 
 * @param uxMaxCount 信号量的最大计数值。
 * @param uxInitialCount 信号量的初始计数值。
 * @param pxStaticQueue 指向静态分配的队列结构的指针。
 * 
 * @return 返回指向新创建的计数信号量的句柄。
 */
QueueHandle_t xQueueCreateCountingSemaphoreStatic(
    const UBaseType_t uxMaxCount,
    const UBaseType_t uxInitialCount,
    StaticQueue_t *pxStaticQueue
)
{
    QueueHandle_t xHandle;

    // 断言：确保最大计数值不为零
    configASSERT(uxMaxCount != 0);

    // 断言：确保初始计数值不大于最大计数值
    configASSERT(uxInitialCount <= uxMaxCount);

    // 创建一个计数信号量
    xHandle = xQueueGenericCreateStatic(
        uxMaxCount,          // 队列长度
        queueSEMAPHORE_QUEUE_ITEM_LENGTH, // 队列项大小
        NULL,                // 队列存储区
        pxStaticQueue,       // 静态分配的队列结构
        queueQUEUE_TYPE_COUNTING_SEMAPHORE // 队列类型
    );

    if (xHandle != NULL)
    {
        // 设置信号量的初始计数值
        ((Queue_t *)xHandle)->uxMessagesWaiting = uxInitialCount;

        // 记录计数信号量创建的跟踪信息
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        // 记录计数信号量创建失败的跟踪信息
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    // 返回新创建的计数信号量句柄
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/**
 * @brief 创建一个计数信号量。
 * 
 * 此函数用于创建一个计数信号量，并初始化其最大计数值和初始计数值。
 * 
 * @param uxMaxCount 信号量的最大计数值。
 * @param uxInitialCount 信号量的初始计数值。
 * 
 * @return 返回指向新创建的计数信号量的句柄。
 */
QueueHandle_t xQueueCreateCountingSemaphore(
    const UBaseType_t uxMaxCount,
    const UBaseType_t uxInitialCount
)
{
    QueueHandle_t xHandle;                   // 定义队列句柄

    // 确保最大计数不为零
    configASSERT(uxMaxCount != 0);

    // 确保初始计数不超过最大计数
    configASSERT(uxInitialCount <= uxMaxCount);

    // 通过通用队列创建函数创建计数信号量
    xHandle = xQueueGenericCreate(
        uxMaxCount,                          // 队列长度
        queueSEMAPHORE_QUEUE_ITEM_LENGTH,    // 队列项大小
        queueQUEUE_TYPE_COUNTING_SEMAPHORE   // 队列类型
    );

    if (xHandle != NULL)
    {
        // 设置队列中的消息等待数量（即计数器的初始值）
        ((Queue_t *)xHandle)->uxMessagesWaiting = uxInitialCount;

        // 记录创建计数信号量的信息（可选的跟踪功能）
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        // 记录创建计数信号量失败的信息（可选的跟踪功能）
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    // 返回队列句柄
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

/*
 * @brief xQueueGenericSend 用于向队列中发送数据。
 *
 * 该函数用于向指定的队列中发送数据，并支持超时和覆盖（overwrite）模式。
 *
 * @param xQueue 需要向其发送数据的队列句柄。
 * @param pvItemToQueue 要发送的数据项。
 * @param xTicksToWait 等待发送数据的最大时间（以滴答数为单位）。
 * @param xCopyPosition 指定是否允许覆盖队列中的现有数据。
 *
 * @return 返回pdPASS表示成功，errQUEUE_FULL表示队列已满。
 */
BaseType_t xQueueGenericSend(
    QueueHandle_t xQueue, 
    const void * const pvItemToQueue, 
    TickType_t xTicksToWait, 
    const BaseType_t xCopyPosition)
{
    BaseType_t xEntryTimeSet = pdFALSE, xYieldRequired; // 标记是否设置了超时时间和是否需要调度
    TimeOut_t xTimeOut;                                 // 时间超时结构
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;     // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT( pxQueue );

    // 断言：确保缓冲区不为NULL，除非队列项大小为0
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    // 断言：确保在非单元素队列中不会发生覆盖
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        // 断言：确保当调度器暂停时，等待时间为0
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // 无限循环，直到成功发送数据或超时
    for( ;; )
    {
        taskENTER_CRITICAL(); // 进入临界区
        {
            // 检查队列是否还有空间或者是否允许覆盖
            if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
            {
                // 记录发送队列的信息
                traceQUEUE_SEND( pxQueue );

                // 将数据复制到队列
                xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                #if ( configUSE_QUEUE_SETS == 1 )
                {
                    // 如果队列属于某个队列集
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        // 通知队列集容器
                        if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION(); // 如果使用抢占式调度，则调度
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    else
                    {
                        // 如果有等待接收的任务列表
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            // 移除等待接收的任务
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                queueYIELD_IF_USING_PREEMPTION(); // 如果使用抢占式调度，则调度
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                            }
                        }
                        else if( xYieldRequired != pdFALSE )
                        {
                            // 如果需要调度
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    // 如果有等待接收的任务列表
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // 移除等待接收的任务
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION(); // 如果使用抢占式调度，则调度
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    else if( xYieldRequired != pdFALSE )
                    {
                        // 如果需要调度
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                    }
                }
                #endif /* configUSE_QUEUE_SETS */

                taskEXIT_CRITICAL(); // 退出临界区
                return pdPASS; // 成功发送数据
            }
            else
            {
                // 如果等待时间为0
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL(); // 退出临界区

                    // 记录发送失败的信息
                    traceQUEUE_SEND_FAILED( pxQueue );
                    return errQUEUE_FULL; // 队列已满
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    // 设置超时时间
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                }
            }
        }
        taskEXIT_CRITICAL(); // 退出临界区

        // 暂停所有任务
        vTaskSuspendAll();
        // 锁定队列
        prvLockQueue( pxQueue );

        // 检查是否超时
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 如果队列已满
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                // 记录阻塞发送的信息
                traceBLOCKING_ON_QUEUE_SEND( pxQueue );

                // 将当前任务添加到等待发送的任务列表
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                // 解锁队列
                prvUnlockQueue( pxQueue );

                // 恢复所有任务
                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API(); // 任务调度
                }
            }
            else
            {
                // 解锁队列
                prvUnlockQueue( pxQueue );
                // 恢复所有任务
                ( void ) xTaskResumeAll();
            }
        }
        else
        {
            // 解锁队列
            prvUnlockQueue( pxQueue );
            // 恢复所有任务
            ( void ) xTaskResumeAll();

            // 记录发送失败的信息
            traceQUEUE_SEND_FAILED( pxQueue );
            return errQUEUE_FULL; // 队列已满
        }
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 从中断服务程序发送数据到队列。
 *
 * 此函数用于从中断服务程序向队列发送数据。它不会阻塞，如果队列没有空间，则返回错误。
 * 如果有更高优先级的任务被唤醒，则返回一个标志，指示是否需要上下文切换。
 *
 * @param xQueue 队列句柄。
 * @param pvItemToQueue 要发送的数据项。
 * @param pxHigherPriorityTaskWoken 指向一个布尔变量的指针，用于指示是否有更高优先级的任务被唤醒。
 * @param xCopyPosition 数据复制的位置（queueOVERWRITE 或其他位置）。
 *
 * @return 返回 pdPASS 表示成功发送，返回 errQUEUE_FULL 表示队列满。
 */
BaseType_t xQueueGenericSendFromISR(
    QueueHandle_t xQueue,
    const void * const pvItemToQueue,
    BaseType_t * const pxHigherPriorityTaskWoken,
    const BaseType_t xCopyPosition
)
{
    BaseType_t xReturn;
    UBaseType_t uxSavedInterruptStatus;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 断言确保队列句柄有效
    configASSERT(pxQueue);

    // 断言确保数据项不为空或者队列大小为零
    configASSERT(!( (pvItemToQueue == NULL) && (pxQueue->uxItemSize != (UBaseType_t)0U) ));

    // 断言确保覆盖位置不是 queueOVERWRITE 或者队列长度为 1
    configASSERT(!( (xCopyPosition == queueOVERWRITE) && (pxQueue->uxLength != 1) ));

    // 确保中断服务程序调用的优先级有效性
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 获取中断禁用标志
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        if ((pxQueue->uxMessagesWaiting < pxQueue->uxLength) || (xCopyPosition == queueOVERWRITE))
        {
            const int8_t cTxLock = pxQueue->cTxLock;

            // 记录从 ISR 发送数据到队列的操作
            traceQUEUE_SEND_FROM_ISR(pxQueue);

            // 复制数据到队列
            (void)prvCopyDataToQueue(pxQueue, pvItemToQueue, xCopyPosition);

            // 如果队列未锁定，则处理事件列表
            if (cTxLock == queueUNLOCKED)
            {
                #if (configUSE_QUEUE_SETS == 1)
                {
                    if (pxQueue->pxQueueSetContainer != NULL)
                    {
                        if (prvNotifyQueueSetContainer(pxQueue, xCopyPosition) != pdFALSE)
                        {
                            // 队列属于队列集，且向队列集的发送导致更高优先级的任务解阻塞，需要上下文切换
                            if (pxHigherPriorityTaskWoken != NULL)
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                        {
                            if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                            {
                                // 等待接收的高优先级任务需要上下文切换
                                if (pxHigherPriorityTaskWoken != NULL)
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE;
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER();
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                    {
                        if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                        {
                            // 等待接收的高优先级任务需要上下文切换
                            if (pxHigherPriorityTaskWoken != NULL)
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else
            {
                // 增加锁计数，以便解锁队列的任务知道在锁定期间有数据发送
                pxQueue->cTxLock = (int8_t)(cTxLock + 1);
            }

            xReturn = pdPASS;
        }
        else
        {
            // 记录从 ISR 发送数据到队列失败
            traceQUEUE_SEND_FROM_ISR_FAILED(pxQueue);
            xReturn = errQUEUE_FULL;
        }
    }

    // 恢复中断状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // 返回发送结果
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 从中断服务程序中归还信号量或互斥量。
 *
 * 此函数用于从中断服务程序中归还之前获取的信号量或互斥量。如果归还后有等待接收的任务，可能会导致这些任务被唤醒。
 *
 * @param xQueue 信号量或互斥量的句柄。
 * @param pxHigherPriorityTaskWoken 指向一个布尔变量的指针，用于指示是否有更高优先级的任务被唤醒。
 *
 * @return 返回 pdPASS 表示成功归还，返回 errQUEUE_FULL 表示队列已满。
 */
BaseType_t xQueueGiveFromISR(
    QueueHandle_t xQueue,
    BaseType_t * const pxHigherPriorityTaskWoken
)
{
    BaseType_t xReturn; // 返回值
    UBaseType_t uxSavedInterruptStatus; // 保存中断屏蔽状态
    Queue_t * const pxQueue = (Queue_t *)xQueue; // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT(pxQueue);

    // 断言：确保队列项大小为0（通常用于信号量）
    configASSERT(pxQueue->uxItemSize == 0);

    // 断言：确保在使用互斥量时没有持有者（通常用于信号量）
    configASSERT(!( (pxQueue->uxQueueType == queueQUEUE_IS_MUTEX) && (pxQueue->pxMutexHolder != NULL) ));

    // 断言：确保中断优先级有效
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 保存当前中断屏蔽状态
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting; // 获取队列中等待的消息数量

        // 如果队列还没有达到最大长度
        if (uxMessagesWaiting < pxQueue->uxLength)
        {
            const int8_t cTxLock = pxQueue->cTxLock; // 获取发送锁状态

            // 记录从中断服务程序发送队列的信息
            traceQUEUE_SEND_FROM_ISR(pxQueue);

            // 增加等待的消息数量
            pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

            // 如果发送锁状态为未锁定
            if (cTxLock == queueUNLOCKED)
            {
                #if (configUSE_QUEUE_SETS == 1)
                {
                    // 如果队列属于某个队列集
                    if (pxQueue->pxQueueSetContainer != NULL)
                    {
                        // 通知队列集容器
                        if (prvNotifyQueueSetContainer(pxQueue, queueSEND_TO_BACK) != pdFALSE)
                        {
                            // 如果提供了更高的优先级任务唤醒标志
                            if (pxHigherPriorityTaskWoken != NULL)
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE; // 标记有更高优先级的任务被唤醒
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    else
                    {
                        // 如果有等待接收的任务列表
                        if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                        {
                            // 移除等待接收的任务
                            if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                            {
                                // 如果提供了更高的优先级任务唤醒标志
                                if (pxHigherPriorityTaskWoken != NULL)
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE; // 标记有更高优先级的任务被唤醒
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    // 如果有等待接收的任务列表
                    if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                    {
                        // 移除等待接收的任务
                        if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                        {
                            // 如果提供了更高的优先级任务唤醒标志
                            if (pxHigherPriorityTaskWoken != NULL)
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE; // 标记有更高优先级的任务被唤醒
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                    }
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else
            {
                // 增加发送锁状态
                pxQueue->cTxLock = (int8_t)(cTxLock + 1);
            }

            xReturn = pdPASS; // 设置返回值为成功
        }
        else
        {
            // 记录从中断服务程序发送队列失败的信息
            traceQUEUE_SEND_FROM_ISR_FAILED(pxQueue);
            xReturn = errQUEUE_FULL; // 设置返回值为队列已满
        }
    }

    // 恢复中断屏蔽状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // 返回发送结果
    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * @brief xQueueGenericReceive 用于从队列中接收数据。
 *
 * 该函数用于从指定的队列中接收数据，并支持超时和仅预览（peeking）模式。
 *
 * @param xQueue 需要从中接收数据的队列句柄。
 * @param pvBuffer 接收的数据将存放在该缓冲区中。
 * @param xTicksToWait 等待接收数据的最大时间（以滴答数为单位）。
 * @param xJustPeeking 如果为pdTRUE，则仅预览而不移除数据。
 *
 * @return 返回pdPASS表示成功，errQUEUE_EMPTY表示队列为空。
 */
BaseType_t xQueueGenericReceive(
    QueueHandle_t xQueue, 
    void * const pvBuffer, 
    TickType_t xTicksToWait, 
    const BaseType_t xJustPeeking)
{
    BaseType_t xEntryTimeSet = pdFALSE; // 标记是否设置了超时时间
    TimeOut_t xTimeOut;                 // 用于超时检测的时间结构
    int8_t *pcOriginalReadPosition;     // 原始读取位置
    Queue_t * const pxQueue = ( Queue_t * ) xQueue; // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT( pxQueue );

    // 断言：确保缓冲区不为NULL，除非队列项大小为0
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        // 断言：确保当调度器暂停时，等待时间为0
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // 无限循环，直到成功接收到数据或超时
    for( ;; )
    {
        taskENTER_CRITICAL(); // 进入临界区
        {
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting; // 获取队列中等待的消息数量

            // 如果队列中有消息
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                // 记录原始读取位置
                pcOriginalReadPosition = pxQueue->u.pcReadFrom;

                // 从队列中复制数据到缓冲区
                prvCopyDataFromQueue( pxQueue, pvBuffer );

                // 如果不是只查看（peek）
                if( xJustPeeking == pdFALSE )
                {
                    // 记录接收队列的信息
                    traceQUEUE_RECEIVE( pxQueue );

                    // 减少等待的消息数量
                    pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

                    #if ( configUSE_MUTEXES == 1 )
                    {
                        // 如果是互斥信号量
                        if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                        {
                            // 更新互斥信号量持有者
                            pxQueue->pxMutexHolder = ( int8_t * ) pvTaskIncrementMutexHeldCount(); /*lint !e961 Cast is not redundant as TaskHandle_t is a typedef. */
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    #endif

                    // 如果有等待发送的任务列表
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                    {
                        // 移除等待发送的任务
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION(); // 如果使用抢占式调度，则调度
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                    }
                }
                else
                {
                    // 记录查看队列的信息
                    traceQUEUE_PEEK( pxQueue );

                    // 恢复原始读取位置
                    pxQueue->u.pcReadFrom = pcOriginalReadPosition;

                    // 如果有等待接收的任务列表
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // 移除等待接收的任务
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            // 如果使用抢占式调度，则调度
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                    }
                }

                taskEXIT_CRITICAL(); // 退出临界区
                return pdPASS; // 成功接收数据
            }
            else
            {
                // 如果等待时间为0
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL(); // 退出临界区
                    traceQUEUE_RECEIVE_FAILED( pxQueue ); // 记录接收失败的信息
                    return errQUEUE_EMPTY; // 队列为空
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    // 设置超时时间
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                }
            }
        }
        taskEXIT_CRITICAL(); // 退出临界区

        // 暂停所有任务
        vTaskSuspendAll();
        // 锁定队列
        prvLockQueue( pxQueue );

        // 检查是否超时
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // 如果队列为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue ); // 记录阻塞接收的信息

                #if ( configUSE_MUTEXES == 1 )
                {
                    // 如果是互斥信号量
                    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                    {
                        taskENTER_CRITICAL(); // 进入临界区
                        {
                            // 优先级继承
                            vTaskPriorityInherit( ( void * ) pxQueue->pxMutexHolder );
                        }
                        taskEXIT_CRITICAL(); // 退出临界区
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                    }
                }
                #endif

                // 将当前任务添加到等待接收的任务列表
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // 解锁队列
                prvUnlockQueue( pxQueue );
                // 恢复所有任务
                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API(); // 任务调度
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
                }
            }
            else
            {
                // 解锁队列
                prvUnlockQueue( pxQueue );
                // 恢复所有任务
                ( void ) xTaskResumeAll();
            }
        }
        else
        {
            // 解锁队列
            prvUnlockQueue( pxQueue );
            // 恢复所有任务
            ( void ) xTaskResumeAll();

            // 如果队列仍然为空
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                traceQUEUE_RECEIVE_FAILED( pxQueue ); // 记录接收失败的信息
                return errQUEUE_EMPTY; // 队列为空
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 覆盖测试标记
            }
        }
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 从中断服务程序接收队列中的数据。
 *
 * 此函数用于从中断服务程序接收队列中的数据。如果队列中有可用数据，则会将数据复制到提供的缓冲区，
 * 并可能唤醒正在等待该队列的更高优先级任务。
 *
 * @param xQueue 队列句柄。
 * @param pvBuffer 接收数据的缓冲区。
 * @param pxHigherPriorityTaskWoken 指向一个布尔变量的指针，用于指示是否有更高优先级的任务被唤醒。
 *
 * @return 返回 pdPASS 表示成功接收，返回 pdFAIL 表示接收失败。
 */
BaseType_t xQueueReceiveFromISR(
    QueueHandle_t xQueue,
    void * const pvBuffer,
    BaseType_t * const pxHigherPriorityTaskWoken
)
{
    BaseType_t xReturn;                         // 返回值
    UBaseType_t uxSavedInterruptStatus;         // 保存中断屏蔽状态
    Queue_t * const pxQueue = (Queue_t *)xQueue; // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT(pxQueue);

    // 断言：确保缓冲区不为NULL，除非队列项大小为0
    configASSERT(!( (pvBuffer == NULL) && (pxQueue->uxItemSize != (UBaseType_t)0U) ));

    // 断言：确保中断优先级有效
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 保存当前中断屏蔽状态
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting; // 获取队列中等待的消息数量

        // 如果队列中有消息
        if (uxMessagesWaiting > (UBaseType_t)0)
        {
            const int8_t cRxLock = pxQueue->cRxLock; // 获取接收锁状态

            // 记录从中断服务程序接收队列的信息
            traceQUEUE_RECEIVE_FROM_ISR(pxQueue);

            // 从队列中复制数据到缓冲区
            prvCopyDataFromQueue(pxQueue, pvBuffer);

            // 减少等待的消息数量
            pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

            // 如果接收锁状态为未锁定
            if (cRxLock == queueUNLOCKED)
            {
                // 如果有等待发送的任务列表
                if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
                {
                    // 移除等待发送的任务
                    if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                    {
                        // 如果提供了更高的优先级任务唤醒标志
                        if (pxHigherPriorityTaskWoken != NULL)
                        {
                            // 标记有更高优先级的任务被唤醒
                            *pxHigherPriorityTaskWoken = pdTRUE;
                        }
                        else
                        {
                            // 覆盖测试标记
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        // 覆盖测试标记
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 增加接收锁状态
                pxQueue->cRxLock = (int8_t)(cRxLock + 1);
            }

            // 设置返回值为成功
            xReturn = pdPASS;
        }
        else
        {
            // 设置返回值为失败，并记录从中断服务程序接收队列失败的信息
            xReturn = pdFAIL;
            traceQUEUE_RECEIVE_FROM_ISR_FAILED(pxQueue);
        }
    }

    // 恢复中断屏蔽状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // 返回接收结果
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 从中断服务程序中预览队列中的数据而不移除它。
 *
 * 此函数用于从中断服务程序预览队列中的数据。如果队列中有可用的数据，则会将数据复制到提供的缓冲区，
 * 但不会改变队列的状态，即数据仍然保留在队列中。
 *
 * @param xQueue 队列句柄。
 * @param pvBuffer 存放预览数据的缓冲区。
 *
 * @return 返回 pdPASS 表示预览成功，返回 pdFAIL 表示预览失败。
 */
BaseType_t xQueuePeekFromISR(
    QueueHandle_t xQueue,
    void * const pvBuffer
)
{
    BaseType_t xReturn;                         // 返回值
    UBaseType_t uxSavedInterruptStatus;         // 保存中断屏蔽状态
    int8_t *pcOriginalReadPosition;             // 原始读取位置
    Queue_t * const pxQueue = (Queue_t *)xQueue; // 队列句柄转换为队列结构指针

    // 断言：确保队列句柄有效
    configASSERT(pxQueue);

    // 断言：确保缓冲区不为NULL，除非队列项大小为0
    configASSERT(!( (pvBuffer == NULL) && (pxQueue->uxItemSize != (UBaseType_t)0U) ));

    // 断言：确保队列项大小不为0，因为不能预览信号量
    configASSERT(pxQueue->uxItemSize != 0); 

    // 断言：确保中断优先级有效
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 保存当前中断屏蔽状态
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        // 检查队列中是否有数据
        if (pxQueue->uxMessagesWaiting > (UBaseType_t)0)
        {
            // 记录从中断服务程序预览队列的信息
            traceQUEUE_PEEK_FROM_ISR(pxQueue);

            // 记录原始读取位置以便之后恢复
            pcOriginalReadPosition = pxQueue->u.pcReadFrom;

            // 从队列中复制数据到缓冲区
            prvCopyDataFromQueue(pxQueue, pvBuffer);

            // 恢复原始读取位置
            pxQueue->u.pcReadFrom = pcOriginalReadPosition;

            // 设置返回值为成功
            xReturn = pdPASS;
        }
        else
        {
            // 设置返回值为失败，并记录从中断服务程序预览队列失败的信息
            xReturn = pdFAIL;
            traceQUEUE_PEEK_FROM_ISR_FAILED(pxQueue);
        }
    }

    // 恢复中断屏蔽状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // 返回预览结果
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 获取指定队列中的消息数量。
 *
 * 此函数用于获取指定队列中的消息数量。为了保证数据的一致性，在读取队列状态时需要进入临界区。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回队列中的消息数量。
 */
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn; // 返回值，用于存储当前队列中的消息数量

    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 进入临界区，防止并发访问队列数据结构
    taskENTER_CRITICAL();

    {
        // 获取队列中当前等待的消息数量
        uxReturn = ((Queue_t *)xQueue)->uxMessagesWaiting;
    }

    // 退出临界区，允许其他任务或中断继续执行
    taskEXIT_CRITICAL();

    // 返回当前队列中的消息数量
    return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

/**
 * @brief 获取指定队列中的可用空间数量。
 *
 * 此函数用于获取指定队列中的可用空间数量。为了保证数据的一致性，在计算队列可用空间时需要进入临界区。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回队列中的可用空间数量。
 */
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn; // 返回值，用于存储当前队列中的可用空间数量
    Queue_t *pxQueue;     // 队列结构指针

    // 将队列句柄转换为队列结构指针
    pxQueue = (Queue_t *)xQueue;

    // 断言：确保队列句柄有效
    configASSERT(pxQueue);

    // 进入临界区，防止并发访问队列数据结构
    taskENTER_CRITICAL();

    {
        // 计算队列中的可用空间数量
        uxReturn = pxQueue->uxLength - pxQueue->uxMessagesWaiting;
    }

    // 退出临界区，允许其他任务或中断继续执行
    taskEXIT_CRITICAL();

    // 返回当前队列中的可用空间数量
    return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

/**
 * @brief 获取指定队列在中断服务程序中的等待消息数量。
 *
 * 此函数用于在中断服务程序中获取指定队列中的等待消息数量。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回队列中的等待消息数量。
 */
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;

    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 直接获取队列中等待的消息数量
    uxReturn = ((Queue_t *)xQueue)->uxMessagesWaiting;

    return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

/**
 * @brief 删除一个队列。
 *
 * 此函数用于删除一个队列。根据配置选项的不同，队列可能是动态分配的也可能是静态分配的。
 *
 * @param xQueue 需要删除的队列句柄。
 */
void vQueueDelete( QueueHandle_t xQueue )
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 断言：确保队列句柄有效
    configASSERT(pxQueue);

    // 记录删除队列的信息
    traceQUEUE_DELETE(pxQueue);

    #if (configQUEUE_REGISTRY_SIZE > 0)
    {
        // 如果启用了队列注册表功能，则取消注册队列
        vQueueUnregisterQueue(pxQueue);
    }
    #endif

    #if (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 0)
    {
        // 如果仅支持动态分配，则释放队列内存
        vPortFree(pxQueue);
    }
    #elif (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 1)
    {
        // 如果同时支持动态和静态分配，则检查分配方式并释放内存
        if (pxQueue->ucStaticallyAllocated == (uint8_t)pdFALSE)
        {
            vPortFree(pxQueue);
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #else
    {
        // 如果只支持静态分配，则不做任何操作
        (void)pxQueue;
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief 获取队列的编号。
 *
 * 此函数用于获取指定队列的编号。只有当配置了追踪设施时，此函数才可用。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回队列的编号。
 */
UBaseType_t uxQueueGetQueueNumber(QueueHandle_t xQueue)
{
    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 获取队列的编号
    return ((Queue_t *)xQueue)->uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief 设置队列的编号。
 *
 * 此函数用于设置指定队列的编号。只有当配置了追踪设施时，此函数才可用。
 *
 * @param xQueue 队列句柄。
 * @param uxQueueNumber 需要设置的队列编号。
 */
void vQueueSetQueueNumber(QueueHandle_t xQueue, UBaseType_t uxQueueNumber)
{
    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 设置队列的编号
    ((Queue_t *)xQueue)->uxQueueNumber = uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief 获取队列的类型。
 *
 * 此函数用于获取指定队列的类型。只有当配置了追踪设施时，此函数才可用。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回队列的类型。
 */
uint8_t ucQueueGetQueueType(QueueHandle_t xQueue)
{
    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 获取队列的类型
    return ((Queue_t *)xQueue)->ucQueueType;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/**
 * @brief 将数据复制到队列中。
 *
 * 此函数用于将数据复制到队列中。该函数假设调用时已经处于临界区，因此不需要额外的互斥保护。
 *
 * @param pxQueue 队列结构指针。
 * @param pvItemToQueue 需要复制到队列的数据。
 * @param xPosition 数据插入队列的位置标志。
 *
 * @return 返回 pdTRUE 表示成功（仅当处理互斥锁时），否则返回 pdFALSE。
 */
static BaseType_t prvCopyDataToQueue(Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition)
{
    BaseType_t xReturn = pdFALSE; // 默认返回值为失败
    UBaseType_t uxMessagesWaiting; // 当前队列中的消息数量

    // 获取当前队列中的消息数量
    uxMessagesWaiting = pxQueue->uxMessagesWaiting;

    if (pxQueue->uxItemSize == (UBaseType_t)0)
    {
        #if (configUSE_MUTEXES == 1)
        {
            if (pxQueue->uxQueueType == queueQUEUE_IS_MUTEX)
            {
                // 如果是互斥锁队列，释放持有者优先级继承
                xReturn = xTaskPriorityDisinherit((void *)pxQueue->pxMutexHolder);
                pxQueue->pxMutexHolder = NULL;
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* configUSE_MUTEXES */
    }
    else if (xPosition == queueSEND_TO_BACK)
    {
        // 将数据复制到队列尾部
        (void)memcpy((void *)pxQueue->pcWriteTo, pvItemToQueue, (size_t)pxQueue->uxItemSize); /*lint !e961 !e418 MISRA exception as the casts are only redundant for some ports, plus previous logic ensures a null pointer can only be passed to memcpy() if the copy size is 0. */
        pxQueue->pcWriteTo += pxQueue->uxItemSize;
        if (pxQueue->pcWriteTo >= pxQueue->pcTail) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
        {
            // 如果写指针到达尾部，则重置到头部
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // 将数据复制到队列头部
        (void)memcpy((void *)pxQueue->u.pcReadFrom, pvItemToQueue, (size_t)pxQueue->uxItemSize); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
        pxQueue->u.pcReadFrom -= pxQueue->uxItemSize;
        if (pxQueue->u.pcReadFrom < pxQueue->pcHead) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
        {
            // 如果读指针小于头部，则重置为尾部减去项大小
            pxQueue->u.pcReadFrom = (pxQueue->pcTail - pxQueue->uxItemSize);
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        if (xPosition == queueOVERWRITE)
        {
            if (uxMessagesWaiting > (UBaseType_t)0)
            {
                // 如果覆盖现有项目，则先减去一个已有的项目数
                --uxMessagesWaiting;
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // 更新队列中的消息数量
    pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 从队列中复制数据到指定缓冲区。
 *
 * 此函数用于从队列中复制数据到指定的缓冲区。该函数假定调用时已经处于临界区，因此不需要额外的互斥保护。
 *
 * @param pxQueue 队列结构指针。
 * @param pvBuffer 存放从队列中读取的数据的缓冲区。
 */
static void prvCopyDataFromQueue(Queue_t * const pxQueue, void * const pvBuffer)
{
    // 检查队列项大小是否非零
    if (pxQueue->uxItemSize != (UBaseType_t)0)
    {
        // 更新读指针
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;

        // 如果读指针到达或超过尾部地址，则重置为头部地址
        if (pxQueue->u.pcReadFrom >= pxQueue->pcTail) /*lint !e946 MISRA exception justified as use of the relational operator is the cleanest solutions. */
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        // 将数据从队列中复制到缓冲区
        (void)memcpy((void *)pvBuffer, (void *)pxQueue->u.pcReadFrom, (size_t)pxQueue->uxItemSize); /*lint !e961 !e418 MISRA exception as the casts are only redundant for some ports.  Also previous logic ensures a null pointer can only be passed to memcpy() when the count is 0. */
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 解锁队列。
 *
 * 此函数用于解锁队列。当队列被锁定时，可以向队列添加或移除数据项，但事件列表不会更新。
 * 解锁队列时，需要处理在锁定期间添加或移除的数据项，并通知等待的任务。
 *
 * 注意：此函数必须在调度器暂停的情况下调用。
 *
 * @param pxQueue 队列结构指针。
 */
static void prvUnlockQueue(Queue_t * const pxQueue)
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */

    /* 锁定计数包含在队列被锁定期间额外放置或移除的数据项的数量。 */
    
    // 进入临界区
    taskENTER_CRITICAL();
    {
        int8_t cTxLock = pxQueue->cTxLock;

        // 检查是否有数据被添加到队列中
        while (cTxLock > queueLOCKED_UNMODIFIED)
        {
            // 数据在队列锁定期间被发布。是否有任务正等待数据？
            #if (configUSE_QUEUE_SETS == 1)
            {
                if (pxQueue->pxQueueSetContainer != NULL)
                {
                    if (prvNotifyQueueSetContainer(pxQueue, queueSEND_TO_BACK) != pdFALSE)
                    {
                        // 队列是队列集的一部分，并且向队列集发布数据导致更高优先级的任务解除阻塞。需要进行上下文切换。
                        vTaskMissedYield();
                    }
                    else
                    {
                        // 覆盖测试标记
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    // 等待接收数据的任务将被添加到待处理就绪列表中，因为此时调度器仍处于暂停状态。
                    if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                    {
                        if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                        {
                            // 等待的任务具有更高的优先级，因此记录需要上下文切换。
                            vTaskMissedYield();
                        }
                        else
                        {
                            // 覆盖测试标记
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            #else /* configUSE_QUEUE_SETS */
            {
                // 等待接收数据的任务将被添加到待处理就绪列表中，因为此时调度器仍处于暂停状态。
                if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                {
                    if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                    {
                        // 等待的任务具有更高的优先级，因此记录需要上下文切换。
                        vTaskMissedYield();
                    }
                    else
                    {
                        // 覆盖测试标记
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    break;
                }
            }
            #endif /* configUSE_QUEUE_SETS */

            --cTxLock;
        }

        // 解锁发送锁
        pxQueue->cTxLock = queueUNLOCKED;
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    // 对接收锁做同样的处理
    taskENTER_CRITICAL();
    {
        int8_t cRxLock = pxQueue->cRxLock;

        while (cRxLock > queueLOCKED_UNMODIFIED)
        {
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
            {
                if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                {
                    vTaskMissedYield();
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }

                --cRxLock;
            }
            else
            {
                break;
            }
        }

        // 解锁接收锁
        pxQueue->cRxLock = queueUNLOCKED;
    }
    // 退出临界区
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/**
 * @brief 判断队列是否为空。
 *
 * 此函数用于判断指定队列是否为空。为了保证数据的一致性，在检查队列状态时需要进入临界区。
 *
 * @param pxQueue 队列结构指针。
 *
 * @return 返回 pdTRUE 表示队列为空，否则返回 pdFALSE。
 */
static BaseType_t prvIsQueueEmpty(const Queue_t *pxQueue)
{
    BaseType_t xReturn; // 返回值，用于存储队列是否为空的结果

    // 进入临界区，防止并发访问队列数据结构
    taskENTER_CRITICAL();
    {
        // 检查队列中的消息数量是否为零
        if (pxQueue->uxMessagesWaiting == (UBaseType_t)0)
        {
            xReturn = pdTRUE; // 队列为空
        }
        else
        {
            xReturn = pdFALSE; // 队列不为空
        }
    }
    // 退出临界区，允许其他任务或中断继续执行
    taskEXIT_CRITICAL();

    // 返回队列是否为空的结果
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 在中断服务程序中判断队列是否为空。
 *
 * 此函数用于在中断服务程序中判断指定队列是否为空。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回 pdTRUE 表示队列为空，否则返回 pdFALSE。
 */
BaseType_t xQueueIsQueueEmptyFromISR(const QueueHandle_t xQueue)
{
    BaseType_t xReturn; // 返回值，用于存储队列是否为空的结果

    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 检查队列中的消息数量是否为零
    if (((Queue_t *)xQueue)->uxMessagesWaiting == (UBaseType_t)0)
    {
        xReturn = pdTRUE; // 队列为空
    }
    else
    {
        xReturn = pdFALSE; // 队列不为空
    }

    // 返回队列是否为空的结果
    return xReturn;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

/**
 * @brief 判断队列是否已满。
 *
 * 此函数用于判断指定队列是否已满。为了保证数据的一致性，在检查队列状态时需要进入临界区。
 *
 * @param pxQueue 队列结构指针。
 *
 * @return 返回 pdTRUE 表示队列已满，否则返回 pdFALSE。
 */
static BaseType_t prvIsQueueFull(const Queue_t *pxQueue)
{
    BaseType_t xReturn; // 返回值，用于存储队列是否已满的结果

    // 进入临界区，防止并发访问队列数据结构
    taskENTER_CRITICAL();
    {
        // 检查队列中的消息数量是否等于队列的最大长度
        if (pxQueue->uxMessagesWaiting == pxQueue->uxLength)
        {
            xReturn = pdTRUE; // 队列已满
        }
        else
        {
            xReturn = pdFALSE; // 队列未满
        }
    }
    // 退出临界区，允许其他任务或中断继续执行
    taskEXIT_CRITICAL();

    // 返回队列是否已满的结果
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 在中断服务程序中判断队列是否已满。
 *
 * 此函数用于在中断服务程序中判断指定队列是否已满。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回 pdTRUE 表示队列已满，否则返回 pdFALSE。
 */
BaseType_t xQueueIsQueueFullFromISR(const QueueHandle_t xQueue)
{
    BaseType_t xReturn; // 返回值，用于存储队列是否已满的结果

    // 断言：确保队列句柄有效
    configASSERT(xQueue);

    // 检查队列中的消息数量是否等于队列的最大长度
    if (((Queue_t *)xQueue)->uxMessagesWaiting == ((Queue_t *)xQueue)->uxLength)
    {
        xReturn = pdTRUE; // 队列已满
    }
    else
    {
        xReturn = pdFALSE; // 队列未满
    }

    // 返回队列是否已满的结果
    return xReturn;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief 向队列发送数据。
 *
 * 此函数用于向指定队列发送数据。如果队列已满，根据等待时间的不同，函数会返回错误或尝试阻塞当前协程。
 *
 * @param xQueue 队列句柄。
 * @param pvItemToQueue 要发送的数据指针。
 * @param xTicksToWait 阻塞等待的时间（以滴答为单位）。
 *
 * @return 返回 pdPASS 表示成功发送，errQUEUE_FULL 表示队列已满，errQUEUE_BLOCKED 表示需要阻塞，errQUEUE_YIELD 表示需要调度。
 */
BaseType_t xQueueCRSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 进入临界区，防止中断在检查队列是否已满与阻塞之间修改队列
    portDISABLE_INTERRUPTS();
    {
        if (prvIsQueueFull(pxQueue) != pdFALSE)
        {
            // 队列已满 - 我们是要阻塞还是直接返回？
            if (xTicksToWait > (TickType_t)0)
            {
                // 由于这是在协程中调用的，我们不能直接阻塞，而是返回表示需要阻塞的状态。
                vCoRoutineAddToDelayedList(xTicksToWait, &(pxQueue->xTasksWaitingToSend));
                portENABLE_INTERRUPTS();
                return errQUEUE_BLOCKED;
            }
            else
            {
                portENABLE_INTERRUPTS();
                return errQUEUE_FULL;
            }
        }
    }
    // 退出临界区
    portENABLE_INTERRUPTS();

    // 再次进入临界区，防止并发访问队列数据结构
    portDISABLE_INTERRUPTS();
    {
        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            // 队列中有空间，将数据复制到队列中
            prvCopyDataToQueue(pxQueue, pvItemToQueue, queueSEND_TO_BACK);
            xReturn = pdPASS;

            // 是否有协程正在等待数据？
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
            {
                // 在这种情况下，可以在临界区内直接将协程放入就绪列表。
                // 但是这里采用与中断中相同的方式处理。
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                {
                    // 等待的协程优先级较高，记录可能需要调度。
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 队列已满
            xReturn = errQUEUE_FULL;
        }
    }
    // 退出临界区
    portENABLE_INTERRUPTS();

    // 返回结果
    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief 从队列接收数据。
 *
 * 此函数用于从指定队列接收数据。如果队列为空，根据等待时间的不同，函数会返回错误或尝试阻塞当前协程。
 *
 * @param xQueue 队列句柄。
 * @param pvBuffer 接收的数据将存放在该缓冲区中。
 * @param xTicksToWait 阻塞等待的时间（以滴答为单位）。
 *
 * @return 返回 pdPASS 表示成功接收，pdFAIL 表示队列为空，errQUEUE_BLOCKED 表示需要阻塞，errQUEUE_YIELD 表示需要调度。
 */
BaseType_t xQueueCRReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 进入临界区，防止中断在检查队列是否为空与阻塞之间修改队列
    portDISABLE_INTERRUPTS();
    {
        if (pxQueue->uxMessagesWaiting == (UBaseType_t)0)
        {
            // 队列中没有消息，是要阻塞还是直接返回？
            if (xTicksToWait > (TickType_t)0)
            {
                // 由于这是在协程中调用的，我们不能直接阻塞，而是返回表示需要阻塞的状态。
                vCoRoutineAddToDelayedList(xTicksToWait, &(pxQueue->xTasksWaitingToReceive));
                portENABLE_INTERRUPTS();
                return errQUEUE_BLOCKED;
            }
            else
            {
                portENABLE_INTERRUPTS();
                return pdFAIL; // 应该是 errQUEUE_EMPTY 而不是 errQUEUE_FULL
            }
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    // 退出临界区
    portENABLE_INTERRUPTS();

    // 再次进入临界区，防止并发访问队列数据结构
    portDISABLE_INTERRUPTS();
    {
        if (pxQueue->uxMessagesWaiting > (UBaseType_t)0)
        {
            // 队列中有数据可用
            pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
            if (pxQueue->u.pcReadFrom >= pxQueue->pcTail)
            {
                pxQueue->u.pcReadFrom = pxQueue->pcHead;
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
            --(pxQueue->uxMessagesWaiting);
            (void)memcpy((void *)pvBuffer, (void *)pxQueue->u.pcReadFrom, (unsigned)pxQueue->uxItemSize);

            xReturn = pdPASS;

            // 是否有协程正在等待队列空间？
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
            {
                // 在这种情况下，可以在临界区内直接将协程放入就绪列表。
                // 但是这里采用与中断中相同的方式处理。
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                {
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            xReturn = pdFAIL;
        }
    }
    // 退出临界区
    portENABLE_INTERRUPTS();

    // 返回结果
    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief 在中断服务程序中向队列发送数据。
 *
 * 此函数用于在中断服务程序中向指定队列发送数据。如果队列已满，则不执行任何操作直接返回。
 *
 * @param xQueue 队列句柄。
 * @param pvItemToQueue 要发送的数据指针。
 * @param xCoRoutinePreviouslyWoken 如果之前已经有协程被唤醒，则设置为 pdTRUE，否则为 pdFALSE。
 *
 * @return 返回 pdTRUE 如果本次调用唤醒了一个协程，否则返回传入的 xCoRoutinePreviouslyWoken 值。
 */
BaseType_t xQueueCRSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t xCoRoutinePreviouslyWoken)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 如果队列中还有空间
    if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
    {
        // 将数据复制到队列
        prvCopyDataToQueue(pxQueue, pvItemToQueue, queueSEND_TO_BACK);

        // 中断服务程序中只能唤醒一个协程，因此检查是否已有协程被唤醒
        if (xCoRoutinePreviouslyWoken == pdFALSE)
        {
            // 如果有协程正在等待接收数据
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
            {
                // 移除协程使其准备运行
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                {
                    // 返回 pdTRUE 表示本次调用唤醒了一个协程
                    return pdTRUE;
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // 队列已满，不做任何操作
        mtCOVERAGE_TEST_MARKER();
    }

    // 返回传入的 xCoRoutinePreviouslyWoken 值
    return xCoRoutinePreviouslyWoken;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief 在中断服务程序中从队列接收数据。
 *
 * 此函数用于在中断服务程序中从指定队列接收数据。如果队列为空，则不执行任何操作直接返回。
 *
 * @param xQueue 队列句柄。
 * @param pvBuffer 接收的数据将存放在该缓冲区中。
 * @param pxCoRoutineWoken 指针，用来指示是否有协程被唤醒。如果被唤醒则设置为 pdTRUE，否则为 pdFALSE。
 *
 * @return 返回 pdPASS 如果成功接收数据，否则返回 pdFAIL。
 */
BaseType_t xQueueCRReceiveFromISR(QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxCoRoutineWoken)
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 如果队列中有数据可用
    if (pxQueue->uxMessagesWaiting > (UBaseType_t)0)
    {
        // 从队列中读取数据
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
        if (pxQueue->u.pcReadFrom >= pxQueue->pcTail)
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
        --(pxQueue->uxMessagesWaiting);
        (void)memcpy((void *)pvBuffer, (void *)pxQueue->u.pcReadFrom, (unsigned)pxQueue->uxItemSize);

        // 检查是否有协程正在等待发送空间
        if (*pxCoRoutineWoken == pdFALSE)
        {
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
            {
                // 如果有协程正在等待队列空间
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                {
                    // 设置标志表示有协程被唤醒
                    *pxCoRoutineWoken = pdTRUE;
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        // 返回 pdPASS 表示成功接收数据
        xReturn = pdPASS;
    }
    else
    {
        // 队列为空，返回 pdFAIL
        xReturn = pdFAIL;
    }

    // 返回接收结果
    return xReturn;
}
#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

/**
 * @brief 将队列添加到注册表。
 *
 * 此函数用于将指定的队列及其名称添加到队列注册表中。注册表用于跟踪系统中所有队列的使用情况。
 *
 * @param xQueue 队列句柄。
 * @param pcQueueName 队列的名称。
 */
void vQueueAddToRegistry(QueueHandle_t xQueue, const char *pcQueueName) /*lint !e971 Unqualified char types are allowed for strings and single characters only.*/
{
    UBaseType_t ux;

    // 查找注册表中的空闲位置。NULL 名称表示空闲槽位。
    for (ux = (UBaseType_t)0U; ux < (UBaseType_t)configQUEUE_REGISTRY_SIZE; ux++)
    {
        if (xQueueRegistry[ux].pcQueueName == NULL)
        {
            // 存储队列的信息
            xQueueRegistry[ux].pcQueueName = pcQueueName;
            xQueueRegistry[ux].xHandle = xQueue;

            // 可选：记录队列注册信息
            traceQUEUE_REGISTRY_ADD(xQueue, pcQueueName);
            
            break;
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
}
#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

/**
 * @brief 获取队列的名称。
 *
 * 此函数用于从队列注册表中查找给定队列句柄对应的名称。
 *
 * @param xQueue 队列句柄。
 *
 * @return 返回队列的名称，如果未找到则返回 NULL。
 */
const char *pcQueueGetName(QueueHandle_t xQueue) /*lint !e971 Unqualified char types are allowed for strings and single characters only.*/
{
    UBaseType_t ux;
    const char *pcReturn = NULL; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

    // 注意：此处没有任何保护措施来防止其他任务在搜索注册表时添加或删除条目。
    for (ux = (UBaseType_t)0U; ux < (UBaseType_t)configQUEUE_REGISTRY_SIZE; ux++)
    {
        if (xQueueRegistry[ux].xHandle == xQueue)
        {
            // 找到了对应的队列名称
            pcReturn = xQueueRegistry[ux].pcQueueName;
            break;
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // 返回队列名称
    return pcReturn;
}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

/**
 * @brief 从队列注册表中注销队列。
 *
 * 此函数用于从队列注册表中移除指定队列的信息。
 *
 * @param xQueue 需要注销的队列句柄。
 */
void vQueueUnregisterQueue(QueueHandle_t xQueue)
{
    UBaseType_t ux;

    // 查找队列句柄是否在注册表中存在。
    for (ux = (UBaseType_t)0U; ux < (UBaseType_t)configQUEUE_REGISTRY_SIZE; ux++)
    {
        if (xQueueRegistry[ux].xHandle == xQueue)
        {
            // 将名称设置为 NULL，表示该槽位再次可用。
            xQueueRegistry[ux].pcQueueName = NULL;

            // 将句柄设置为 NULL，以确保相同的队列句柄不会出现在注册表中两次。
            xQueueRegistry[ux].xHandle = (QueueHandle_t)NULL;
            
            break;
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )

/**
 * @brief 使任务等待队列中的消息（受限制）。
 *
 * 此函数不是公共API的一部分，专为内核代码设计。它具有特殊的调用要求，并且应在锁定调度器的情况下调用，
 * 而不是在临界区中调用。它可以导致在只能有一个项目的列表上调用 vListInsert()，因此列表处理速度很快，
 * 但仍需小心使用。
 *
 * @param xQueue 需要等待消息的队列句柄。
 * @param xTicksToWait 阻塞等待的时间（以滴答为单位）。
 * @param xWaitIndefinitely 是否无限期等待。
 */
void vQueueWaitForMessageRestricted(QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // 锁定队列，防止并发访问队列数据结构
    prvLockQueue(pxQueue);

    // 只有在队列中没有消息时才做些事情。
    if (pxQueue->uxMessagesWaiting == (UBaseType_t)0U)
    {
        // 队列中没有消息，根据指定的时间阻塞任务。
        vTaskPlaceOnEventListRestricted(&(pxQueue->xTasksWaitingToReceive), xTicksToWait, xWaitIndefinitely);
    }
    else
    {
        // 覆盖测试标记
        mtCOVERAGE_TEST_MARKER();
    }

    // 解锁队列
    prvUnlockQueue(pxQueue);
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/**
 * @brief 创建队列集。
 *
 * 此函数用于创建一个新的队列集，队列集可以包含多个队列，用于实现类似信号量组的功能。
 *
 * @param uxEventQueueLength 队列集中事件队列的长度，即队列集可以容纳的最大事件数量。
 *
 * @return 返回创建的队列集的句柄，如果创建失败则返回 NULL。
 */
QueueSetHandle_t xQueueCreateSet(const UBaseType_t uxEventQueueLength)
{
    QueueSetHandle_t pxQueue;

    // 使用通用队列创建函数创建队列集
    pxQueue = xQueueGenericCreate(uxEventQueueLength, sizeof(Queue_t *), queueQUEUE_TYPE_SET);

    // 返回创建的队列集句柄
    return pxQueue;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief 将队列或信号量添加到队列集。
 *
 * 此函数用于将一个队列或信号量添加到指定的队列集中。队列集可以用于管理一组队列或信号量。
 *
 * @param xQueueOrSemaphore 需要添加到队列集的队列或信号量的句柄。
 * @param xQueueSet 目标队列集的句柄。
 *
 * @return 返回 pdPASS 如果添加成功，否则返回 pdFAIL。
 */
BaseType_t xQueueAddToSet(QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet)
{
    BaseType_t xReturn;

    // 进入临界区，防止并发访问队列数据结构
    taskENTER_CRITICAL();
    {
        // 检查队列或信号量是否已经属于另一个队列集
        if (((Queue_t *)xQueueOrSemaphore)->pxQueueSetContainer != NULL)
        {
            // 不能将同一个队列或信号量添加到多个队列集中
            xReturn = pdFAIL;
        }
        else if (((Queue_t *)xQueueOrSemaphore)->uxMessagesWaiting != (UBaseType_t)0)
        {
            // 如果队列或信号量中已经有项目，则不能将其添加到队列集中
            xReturn = pdFAIL;
        }
        else
        {
            // 将队列或信号量添加到指定的队列集中
            ((Queue_t *)xQueueOrSemaphore)->pxQueueSetContainer = xQueueSet;
            xReturn = pdPASS;
        }
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    // 返回操作结果
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	/**
 * @brief 从队列集中移除队列或信号量。
 *
 * 此函数用于将一个队列或信号量从指定的队列集中移除。队列集可以用于管理一组队列或信号量。
 *
 * @param xQueueOrSemaphore 需要从队列集中移除的队列或信号量的句柄。
 * @param xQueueSet 需要移除队列或信号量的队列集句柄。
 *
 * @return 返回 pdPASS 如果移除成功，否则返回 pdFAIL。
 */
BaseType_t xQueueRemoveFromSet(QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet)
{
    BaseType_t xReturn;
    Queue_t * const pxQueueOrSemaphore = (Queue_t *)xQueueOrSemaphore;

    // 检查队列或信号量是否属于指定的队列集
    if (pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet)
    {
        // 如果队列或信号量不属于指定的队列集
        xReturn = pdFAIL;
    }
    else if (pxQueueOrSemaphore->uxMessagesWaiting != (UBaseType_t)0)
    {
        // 如果队列或信号量中还有消息，移除是危险的，因为队列集仍然会持有队列的待处理事件
        xReturn = pdFAIL;
    }
    else
    {
        // 进入临界区，防止并发访问队列数据结构
        taskENTER_CRITICAL();
        {
            // 将队列或信号量从队列集中移除
            pxQueueOrSemaphore->pxQueueSetContainer = NULL;
        }
        // 退出临界区
        taskEXIT_CRITICAL();
        xReturn = pdPASS;
    }

    // 返回操作结果
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief 从队列集中选择一个可接收的队列或信号量。
 *
 * 此函数用于从指定的队列集中选择一个可以接收数据的队列或信号量。如果在指定时间内没有队列或信号量变为可接收状态，则返回 NULL。
 *
 * @param xQueueSet 队列集的句柄。
 * @param xTicksToWait 等待时间（以滴答为单位），如果设置为 `portMAX_DELAY` 则表示无限等待。
 *
 * @return 返回可以接收数据的队列或信号量的句柄，如果没有找到则返回 NULL。
 */
QueueSetMemberHandle_t xQueueSelectFromSet(QueueSetHandle_t xQueueSet, TickType_t const xTicksToWait)
{
    QueueSetMemberHandle_t xReturn = NULL;

    // 使用通用队列接收函数尝试从队列集中选择一个可接收的队列或信号量
    (void)xQueueGenericReceive((QueueHandle_t)xQueueSet, &xReturn, xTicksToWait, pdFALSE); /*lint !e961 Casting from one typedef to another is not redundant.*/

    // 返回选中的队列或信号量句柄
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief 从队列集中选择一个可接收的队列或信号量（中断服务程序）。
 *
 * 此函数用于从指定的队列集中选择一个可以接收数据的队列或信号量，并且此函数应该在中断服务程序中调用。
 * 如果没有找到可以接收数据的队列或信号量，则返回 NULL。
 *
 * @param xQueueSet 队列集的句柄。
 *
 * @return 返回可以接收数据的队列或信号量的句柄，如果没有找到则返回 NULL。
 */
QueueSetMemberHandle_t xQueueSelectFromSetFromISR(QueueSetHandle_t xQueueSet)
{
    QueueSetMemberHandle_t xReturn = NULL;

    // 使用中断安全的队列接收函数尝试从队列集中选择一个可接收的队列或信号量
    (void)xQueueReceiveFromISR((QueueHandle_t)xQueueSet, &xReturn, NULL); /*lint !e961 Casting from one typedef to another is not redundant.*/

    // 返回选中的队列或信号量句柄
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief 通知队列集容器。
 *
 * 此函数用于在向队列集发送数据时更新队列集的状态，并通知等待接收数据的任务。
 * 该函数必须在临界区中调用。
 *
 * @param pxQueue 发送数据的队列。
 * @param xCopyPosition 数据复制的位置标志。
 *
 * @return 返回 pdTRUE 如果有更高优先级的任务准备好运行，否则返回 pdFALSE。
 */
static BaseType_t prvNotifyQueueSetContainer(const Queue_t * const pxQueue, const BaseType_t xCopyPosition)
{
    Queue_t *pxQueueSetContainer = pxQueue->pxQueueSetContainer;
    BaseType_t xReturn = pdFALSE;

    // 断言确保队列集容器不为空并且队列集中的消息数量小于队列集的长度
    configASSERT(pxQueueSetContainer);
    configASSERT(pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength);

    // 检查队列集中的消息数量是否小于队列集的长度
    if (pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength)
    {
        const int8_t cTxLock = pxQueueSetContainer->cTxLock;

        // 记录队列发送信息（用于调试或跟踪）
        traceQUEUE_SEND(pxQueueSetContainer);

        // 复制数据到队列集，数据为包含数据的队列的句柄
        xReturn = prvCopyDataToQueue(pxQueueSetContainer, &pxQueue, xCopyPosition);

        // 检查队列集的发送锁状态
        if (cTxLock == queueUNLOCKED)
        {
            // 检查是否有任务正在等待接收数据
            if (listLIST_IS_EMPTY(&(pxQueueSetContainer->xTasksWaitingToReceive)) == pdFALSE)
            {
                // 移除等待接收数据的任务
                if (xTaskRemoveFromEventList(&(pxQueueSetContainer->xTasksWaitingToReceive)) != pdFALSE)
                {
                    // 等待的任务具有更高的优先级
                    xReturn = pdTRUE;
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 更新发送锁状态
            pxQueueSetContainer->cTxLock = (int8_t)(cTxLock + 1);
        }
    }
    else
    {
        // 覆盖测试标记
        mtCOVERAGE_TEST_MARKER();
    }

    // 返回操作结果
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */












