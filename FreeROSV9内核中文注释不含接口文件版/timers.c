/* 标准包含文件。 */
#include <stdlib.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 以防止 task.h 重新定义
所有 API 函数以使用 MPU 包装器。这应该只在 task.h 从应用程序文件包含时完成。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* 包含 FreeRTOS 头文件。 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* 如果 INCLUDE_xTimerPendFunctionCall 被设为 1 并且 configUSE_TIMERS 为 0，则抛出编译错误。
确保 configUSE_TIMERS 设为 1 以使 xTimerPendFunctionCall() 函数可用。 */
#if ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 0 )
    #error configUSE_TIMERS 必须设为 1 才能使 xTimerPendFunctionCall() 函数可用。
#endif

/* 解除 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 的定义。lint 规则 e961 和 e750 被忽略，因为 MPU 端口要求
在上述头文件中定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，但在本文件中不需要，以生成正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750 */

/* 如果应用程序配置不包括软件定时器功能，则整个源文件将被跳过。此 #if 在文件最底部关闭。
如果你想包括软件定时器功能，请确保在 FreeRTOSConfig.h 中将 configUSE_TIMERS 设为 1。 */
#if ( configUSE_TIMERS == 1 )

/* 各种定义。 */
#define tmrNO_DELAY		( TickType_t ) 0U  /* 无延迟的定义，表示立即执行。 */

/* 定时器本身的定义。 */
typedef struct tmrTimerControl {
    const char *pcTimerName;        /* 文本名称。这不是内核使用的，仅仅是为了方便调试。 */
                                      /* lint !e971 允许字符串和单字符使用未限定的字符类型。 */
    ListItem_t xTimerListItem;      /* 用于事件管理的标准链表项。 */
    TickType_t xTimerPeriodInTicks; /* 定时器的周期，以滴答为单位。 */
    UBaseType_t uxAutoReload;       /* 如果设置为 pdTRUE，则定时器过期后自动重启；如果设置为 pdFALSE，则定时器为一次性定时器。 */
    void *pvTimerID;                /* 识别定时器的一个 ID。这允许在同一个回调用于多个定时器时识别定时器。 */
    TimerCallbackFunction_t pxCallbackFunction; /* 定时器过期时将被调用的函数。 */
    
    #if( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxTimerNumber;  /* 由跟踪工具（如 FreeRTOS+Trace）分配的 ID。 */
    #endif
    
    #if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated; /* 如果定时器静态创建，则设置为 pdTRUE，以避免删除定时器时尝试释放内存。 */
    #endif
} xTIMER;

/* 将旧的 xTIMER 名称保持在上面，并使用 typedef 赋予新的 Timer_t 名称，以启用对较旧的内核感知调试器的支持。 */
typedef xTIMER Timer_t;

/* 定义可以在定时器队列中发送和接收的消息类型。
可以排队两种类型的消息——用于操作软件定时器的消息和请求执行非定时器相关回调的消息。
这两种消息类型分别定义在两个不同的结构体中：xTimerParametersType 和 xCallbackParametersType。*/
typedef struct tmrTimerParameters {
    TickType_t xMessageValue;          /* 用于某些命令的可选值，例如，当更改定时器的周期时。 */
    Timer_t *pxTimer;                  /* 应用命令的定时器。 */
} TimerParameter_t;

typedef struct tmrCallbackParameters {
    PendedFunction_t pxCallbackFunction; /* 要执行的回调函数。 */
    void *pvParameter1;                  /* 作为回调函数第一个参数的值。 */
    uint32_t ulParameter2;               /* 作为回调函数第二个参数的值。 */
} CallbackParameters_t;

/* 包含两种消息类型的结构体，以及一个标识符，用于确定哪种消息类型有效。*/
typedef struct tmrTimerQueueMessage {
    BaseType_t xMessageID;              /* 发送到定时器服务任务的命令。 */
    union {
        TimerParameter_t xTimerParameters;

        /* 如果不打算使用 xCallbackParameters，则不要包含它，
           因为它会使结构体（以及因此定时器队列）变大。 */
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
            CallbackParameters_t xCallbackParameters;
        #endif /* INCLUDE_xTimerPendFunctionCall */
    } u;
} DaemonTaskMessage_t;

/* lint -e956 已经通过手动分析和检查确定哪些静态变量必须声明为 volatile。*/

/* 存储活动定时器的列表。定时器按照过期时间排序，最近的过期时间位于列表前面。
只有定时器服务任务允许访问这些列表。 */
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t *pxCurrentTimerList;
PRIVILEGED_DATA static List_t *pxOverflowTimerList;

/* 一个用于向定时器服务任务发送命令的队列。 */
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;
PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*lint +e956 */

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

/* 如果支持静态分配，则应用程序必须提供以下回调函数——这使得应用程序可以选择性地提供定时器任务将使用的内存，
作为任务的堆栈和TCB（任务控制块）。*/
extern void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,   /* 输出参数：指向用于存放定时器任务TCB的静态内存的指针 */
    StackType_t **ppxTimerTaskStackBuffer,  /* 输出参数：指向用于存放定时器任务堆栈的静态内存的指针 */
    uint32_t *pulTimerTaskStackSize         /* 输出参数：定时器任务堆栈的大小 */
);

#endif

/*
 * 初始化定时器服务任务使用的基础设施，如果尚未初始化的话。
 */
static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/*
 * 定时器服务任务（守护进程）。定时器功能由这个任务控制。其他任务通过 xTimerQueue 队列与定时器服务任务通信。
 */
static void prvTimerTask( void *pvParameters ) PRIVILEGED_FUNCTION;

/*
 * 由定时器服务任务调用，以解释和处理其在定时器队列上接收到的命令。
 */
static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/*
 * 将定时器插入 xActiveTimerList1 或 xActiveTimerList2，具体取决于过期时间是否导致定时器计数器溢出。
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/*
 * 一个活动定时器已经到达其过期时间。如果这是一个自动重载定时器，则重载定时器，然后调用其回调函数。
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * 滴答计数已溢出。在确保当前定时器列表不再引用任何定时器之后，切换定时器列表。
 */
static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/*
 * 获取当前的滴答计数值，并在上次调用 prvSampleTimeNow() 以来发生滴答计数溢出的情况下将 *pxTimerListsWereSwitched 设置为 pdTRUE。
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/*
 * 如果定时器列表包含任何活动定时器，则返回将要首先过期的定时器的过期时间，并将 *pxListWasEmpty 设置为 false。
 * 如果定时器列表不包含任何定时器，则返回 0 并将 *pxListWasEmpty 设置为 pdTRUE。
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * 如果定时器已过期，则处理它。否则，阻塞定时器服务任务直到定时器确实过期或接收到命令。
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * 在静态或动态分配 Timer_t 结构之后调用，以填充结构的成员。
 */
static void prvInitialiseNewTimer(	const char * const pcTimerName,     /* 定时器的文本名称 */
									const TickType_t xTimerPeriodInTicks, /* 定时器的周期（滴答为单位） */
									const UBaseType_t uxAutoReload,     /* 自动重载标志 */
									void * const pvTimerID,             /* 定时器的 ID */
									TimerCallbackFunction_t pxCallbackFunction, /* 定时器到期时调用的回调函数 */
									Timer_t *pxNewTimer ) PRIVILEGED_FUNCTION; /*lint !e971 仅允许字符串和单个字符使用未限定的字符类型。 */
/*-----------------------------------------------------------*/
									
/*
 * 该函数用于创建定时器服务任务（prvTimerTask），并在创建成功时返回 pdPASS
 *
 */
BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;

    /* 当配置了 configUSE_TIMERS 为 1 时，此函数会在调度器启动时被调用。
       检查定时器服务任务使用的基础设施是否已经被创建/初始化。
       如果已经有定时器被创建，则初始化应该已经完成。*/
    prvCheckForValidListAndQueue();

    if( xTimerQueue != NULL )
    {
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            StaticTask_t *pxTimerTaskTCBBuffer = NULL;
            StackType_t *pxTimerTaskStackBuffer = NULL;
            uint32_t ulTimerTaskStackSize;

            /* 获取用于定时器任务的静态内存（堆栈和TCB）。*/
            vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
            
            /* 创建定时器服务任务，并使用静态分配的内存。*/
            xTimerTaskHandle = xTaskCreateStatic(
                                    prvTimerTask,    /* 任务函数 */
                                    "Tmr Svc",       /* 任务名称 */
                                    ulTimerTaskStackSize, /* 堆栈大小 */
                                    NULL,            /* 任务参数 */
                                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, /* 优先级 */
                                    pxTimerTaskStackBuffer, /* 堆栈缓冲区 */
                                    pxTimerTaskTCBBuffer /* TCB 缓冲区 */
                                );

            if( xTimerTaskHandle != NULL )
            {
                xReturn = pdPASS;
            }
        }
        #else
        {
            /* 创建定时器服务任务，使用默认的堆栈深度和动态内存分配。*/
            xReturn = xTaskCreate(
                                    prvTimerTask,    /* 任务函数 */
                                    "Tmr Svc",       /* 任务名称 */
                                    configTIMER_TASK_STACK_DEPTH, /* 堆栈深度 */
                                    NULL,            /* 任务参数 */
                                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, /* 优先级 */
                                    &xTimerTaskHandle /* 任务句柄 */
                                );
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }
    else
    {
        /* 覆盖测试标记。*/
        mtCOVERAGE_TEST_MARKER();
    }

    /* 断言确保定时器服务任务创建成功。*/
    configASSERT( xReturn );
    return xReturn;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

/*
 * @brief xTimerCreate 创建一个定时器。
 *
 * 此函数用于创建一个新的定时器，并对其进行初始化。
 *
 * @param pcTimerName 定时器的名字（字符串）。可以为 NULL。
 * @param xTimerPeriodInTicks 定时器的周期，单位为滴答（ticks）。
 * @param uxAutoReload 是否自动重载。非零表示自动重载，零表示单次触发。
 * @param pvTimerID 定时器的 ID 或用户数据。
 * @param pxCallbackFunction 回调函数，当定时器超时时被调用。
 *
 * @return 返回创建的定时器句柄。如果创建失败，则返回 NULL。
 */
TimerHandle_t xTimerCreate(
    const char * const pcTimerName,
    const TickType_t xTimerPeriodInTicks,
    const UBaseType_t uxAutoReload,
    void * const pvTimerID,
    TimerCallbackFunction_t pxCallbackFunction )
{
    Timer_t *pxNewTimer; // 定义指向定时器结构的指针

    // 分配内存来创建新的定时器结构
    pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );

    if( pxNewTimer != NULL )
    {
        // 初始化新的定时器结构
        prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );

        // 如果启用了静态分配支持，则设置定时器的标志
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            // 标记定时器为动态分配
            pxNewTimer->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }

    // 返回创建的定时器句柄
    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
/*
 * 该函数用于静态创建一个定时器，并返回定时器的句柄
 *
 */
TimerHandle_t xTimerCreateStatic(
    const char * const pcTimerName,        /* 定时器的名称 */
    const TickType_t xTimerPeriodInTicks,  /* 定时器的周期（滴答为单位） */
    const UBaseType_t uxAutoReload,        /* 是否自动重载 */
    void * const pvTimerID,                /* 定时器的唯一标识符 */
    TimerCallbackFunction_t pxCallbackFunction, /* 定时器到期时调用的回调函数 */
    StaticTimer_t *pxTimerBuffer           /* 静态定时器缓冲区 */
) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    Timer_t *pxNewTimer;

    #if( configASSERT_DEFINED == 1 )
    {
        /* 检查 StaticTimer_t 结构体的大小是否等于实际定时器结构体的大小，以确保它们是兼容的。 */
        volatile size_t xSize = sizeof( StaticTimer_t );
        configASSERT( xSize == sizeof( Timer_t ) );
    }
    #endif /* configASSERT_DEFINED */

    /* 必须提供一个指向 StaticTimer_t 结构体的指针，使用它。 */
    configASSERT( pxTimerBuffer );
    pxNewTimer = ( Timer_t * ) pxTimerBuffer; /*lint !e740 特殊的类型转换是合理的，因为这两个结构体设计为具有相同的对齐方式，并且大小通过断言进行了检查。 */

    if( pxNewTimer != NULL )
    {
        /* 初始化新的定时器结构。 */
        prvInitialiseNewTimer( 
            pcTimerName, 
            xTimerPeriodInTicks, 
            uxAutoReload, 
            pvTimerID, 
            pxCallbackFunction, 
            pxNewTimer 
        );

        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* 定时器可以静态或动态创建，记录这个定时器是静态创建的，以防以后删除时需要用到这个信息。 */
            pxNewTimer->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }

    /* 返回定时器句柄。 */
    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*
 * 初始化新的定时器结构，并设置其各个成员变量。
 *
 * 参数:
 * @param pcTimerName: 定时器的名称，用于调试。
 * @param xTimerPeriodInTicks: 定时器的周期，以滴答为单位。
 * @param uxAutoReload: 布尔标志，指示定时器是否自动重载。
 * @param pvTimerID: 定时器的唯一标识符。
 * @param pxCallbackFunction: 定时器到期时调用的回调函数。
 * @param pxNewTimer: 指向新定时器结构的指针。
 */
static void prvInitialiseNewTimer(
    const char * const pcTimerName,        /* 定时器的名称 */
    const TickType_t xTimerPeriodInTicks,  /* 定时器的周期（滴答为单位） */
    const UBaseType_t uxAutoReload,        /* 是否自动重载 */
    void * const pvTimerID,                /* 定时器的唯一标识符 */
    TimerCallbackFunction_t pxCallbackFunction, /* 定时器到期时调用的回调函数 */
    Timer_t *pxNewTimer                    /* 新定时器结构的指针 */
) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    /* 0 不是一个有效的周期值。 */
    configASSERT( ( xTimerPeriodInTicks > 0 ) );

    if( pxNewTimer != NULL )
    {
        /* 确保定时器服务任务使用的基础设施已被创建/初始化。 */
        prvCheckForValidListAndQueue();

        /* 使用函数参数初始化定时器结构的成员。 */
        pxNewTimer->pcTimerName = pcTimerName;       /* 定时器的名称 */
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks; /* 定时器的周期 */
        pxNewTimer->uxAutoReload = uxAutoReload;     /* 是否自动重载 */
        pxNewTimer->pvTimerID = pvTimerID;           /* 定时器的唯一标识符 */
        pxNewTimer->pxCallbackFunction = pxCallbackFunction; /* 定时器到期时调用的回调函数 */
        
        /* 初始化定时器列表项。 */
        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
        
        /* 记录定时器创建的跟踪信息。 */
        traceTIMER_CREATE( pxNewTimer );
    }
}
/*-----------------------------------------------------------*/

/*
 * @brief xTimerGenericCommand 发送命令给定时器服务任务以执行特定操作。
 *
 * 此函数用于向定时器服务任务发送命令，以执行特定的操作，如启动、停止、重置等。
 *
 * @param xTimer 定时器句柄。
 * @param xCommandID 命令标识符。
 * @param xOptionalValue 可选的命令参数。
 * @param pxHigherPriorityTaskWoken 如果此函数导致更高优先级的任务被唤醒，则设置为 pdTRUE。
 * @param xTicksToWait 等待队列空闲的时间（以滴答为单位）。
 *
 * @return 返回操作的结果。如果成功发送命令，则返回 pdPASS，否则返回 pdFAIL。
 */
BaseType_t xTimerGenericCommand(
    TimerHandle_t xTimer,
    const BaseType_t xCommandID,
    const TickType_t xOptionalValue,
    BaseType_t * const pxHigherPriorityTaskWoken,
    const TickType_t xTicksToWait )
{
    BaseType_t xReturn = pdFAIL; // 初始化返回值为失败
    DaemonTaskMessage_t xMessage; // 定义消息结构体

    // 断言确保定时器句柄有效
    configASSERT( xTimer );

    /* 如果定时器队列已初始化，则发送消息给定时器服务任务执行特定动作 */
    if( xTimerQueue != NULL )
    {
        /* 构建消息 */
        xMessage.xMessageID = xCommandID;
        xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
        xMessage.u.xTimerParameters.pxTimer = ( Timer_t * ) xTimer;

        if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
        {
            /* 如果调度器正在运行，则允许等待队列 */
            if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
            {
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
            }
            else
            {
                /* 如果调度器未运行，则不允许等待队列 */
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
            }
        }
        else
        {
            /* 如果是从中断服务程序调用，则立即发送消息，并检查是否有更高优先级任务被唤醒 */
            xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
        }

        /* 记录命令发送情况 */
        traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
    }
    else
    {
        /* 覆盖测试标记 */
        mtCOVERAGE_TEST_MARKER();
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * 获取定时器守护任务的句柄。
 *
 * 返回值:
 * 返回定时器守护任务的句柄。
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
    /* 如果在调度器启动之前调用了 xTimerGetTimerDaemonTaskHandle()，
       则 xTimerTaskHandle 将为 NULL。 */
    configASSERT( ( xTimerTaskHandle != NULL ) );
    
    return xTimerTaskHandle;
}
/*-----------------------------------------------------------*/

/*
 * 获取指定定时器的周期。
 *
 * 参数:
 * @param xTimer: 定时器的句柄。
 *
 * 返回值:
 * 返回定时器的周期（滴答为单位）。
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
    Timer_t *pxTimer = ( Timer_t * ) xTimer;

    /* 确保定时器句柄有效。 */
    configASSERT( xTimer );
    
    return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/

/*
 * 获取指定定时器的过期时间。
 *
 * 参数:
 * @param xTimer: 定时器的句柄。
 *
 * 返回值:
 * 返回定时器的过期时间（滴答为单位）。
 */
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
    Timer_t *pxTimer = ( Timer_t * ) xTimer;
    TickType_t xReturn;

    /* 确保定时器句柄有效。 */
    configASSERT( xTimer );

    /* 获取定时器列表项的值，即过期时间。 */
    xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
    
    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * 获取指定定时器的名称。
 *
 * 参数:
 * @param xTimer: 定时器的句柄。
 *
 * 返回值:
 * 返回定时器的名称。
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    Timer_t *pxTimer = ( Timer_t * ) xTimer;

    /* 确保定时器句柄有效。 */
    configASSERT( xTimer );
    
    return pxTimer->pcTimerName;
}
/*-----------------------------------------------------------*/

/*
 * 处理已过期的定时器。
 *
 * 参数:
 * @param xNextExpireTime: 下一个过期时间（滴答为单位）。
 * @param xTimeNow: 当前时间（滴答为单位）。
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )
{
    BaseType_t xResult;
    Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

    /* 从活动定时器列表中移除定时器。已经执行了检查以确保列表非空。 */
    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
    traceTIMER_EXPIRED( pxTimer );

    /* 如果定时器是自动重载定时器，则计算下一个过期时间，并重新插入到活动定时器列表中。 */
    if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
    {
        /* 定时器使用相对于当前时间以外的时间被插入到列表中。因此，它将被插入到相对于当前认为的时间的正确列表中。 */
        if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
        {
            /* 定时器在被添加到活动定时器列表之前已经过期。现在重新加载它。 */
            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
            configASSERT( xResult );
            ( void ) xResult;
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

    /* 调用定时器回调函数。 */
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}
/*-----------------------------------------------------------*/

/*
 * 定时器服务任务（守护进程）。定时器功能由这个任务控制。其他任务通过 xTimerQueue 队列与定时器服务任务通信。
 *
 * 参数:
 * @param pvParameters: 传递给任务的参数（未使用）。
 */
static void prvTimerTask( void *pvParameters )
{
    TickType_t xNextExpireTime;
    BaseType_t xListWasEmpty;

    /* 只是为了避免编译器警告。 */
    ( void ) pvParameters;

    #if( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
    {
        extern void vApplicationDaemonTaskStartupHook( void );

        /* 允许应用程序编写者在此任务开始执行时，在此任务上下文中执行一些代码。这是有用的，如果应用程序包含一些初始化代码，这些代码将在调度器启动后受益于执行。 */
        vApplicationDaemonTaskStartupHook();
    }
    #endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

    for( ;; )
    {
        /* 查询定时器列表以查看其中是否包含任何定时器，如果存在，则获取下一个定时器将过期的时间。 */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

        /* 如果定时器已过期，则处理它。否则，阻塞此任务直到定时器确实过期或接收到命令。 */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

        /* 清空命令队列。 */
        prvProcessReceivedCommands();
    }
}
/*-----------------------------------------------------------*/

/*
 * 如果定时器已过期，则处理它。否则，阻塞定时器服务任务直到定时器确实过期或接收到命令。
 *
 * 参数:
 * @param xNextExpireTime: 下一个过期时间（滴答为单位）。
 * @param xListWasEmpty: 标志，指示定时器列表是否为空。
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty )
{
    TickType_t xTimeNow;
    BaseType_t xTimerListsWereSwitched;

    vTaskSuspendAll();
    {
        xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );
        if( xTimerListsWereSwitched == pdFALSE )
        {

            if( ( xListWasEmpty == pdFALSE ) && ( xNextExpireTime <= xTimeNow ) )
            {
                ( void ) xTaskResumeAll();
                prvProcessExpiredTimer( xNextExpireTime, xTimeNow );
            }
            else
            {

                if( xListWasEmpty != pdFALSE )
                {

                    xListWasEmpty = listLIST_IS_EMPTY( pxOverflowTimerList );
                }

                vQueueWaitForMessageRestricted( xTimerQueue, ( xNextExpireTime - xTimeNow ), xListWasEmpty );

                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        else
        {
            ( void ) xTaskResumeAll();
        }
    }
}
/*-----------------------------------------------------------*/

/*
 * 获取最近过期时间。
 *
 * 参数:
 * @param pxListWasEmpty: 输出参数，指示定时器列表是否为空。
 *
 * 返回值:
 * 返回最近过期时间（以滴答为单位）。
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    TickType_t xNextExpireTime;

    /* 定时器按过期时间排序，列表头部引用的是最先过期的任务。
       获取最近过期时间。
       如果没有活动的定时器，则将过期时间设置为0。
       这将导致任务在计数溢出时解除阻塞，此时将切换定时器列表并重新评估最近的过期时间。 */
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
    if( *pxListWasEmpty == pdFALSE )
    {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* 确保任务在计数溢出时解除阻塞。 */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    return xNextExpireTime;
}
/*-----------------------------------------------------------*/

/*
 * 获取当前时间，并检测是否发生了计数溢出。
 *
 * 参数:
 * @param pxTimerListsWereSwitched: 输出参数，指示是否发生了定时器列表的切换。
 *
 * 返回值:
 * 返回当前时间（以滴答为单位）。
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
    TickType_t xTimeNow;
    PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 变量仅对一个任务可访问。 */

    xTimeNow = xTaskGetTickCount();

    if( xTimeNow < xLastTime )
    {
        /* 如果当前时间小于上次时间，表明计数发生了溢出。 */
        prvSwitchTimerLists();
        *pxTimerListsWereSwitched = pdTRUE;
    }
    else
    {
        *pxTimerListsWereSwitched = pdFALSE;
    }

    xLastTime = xTimeNow;

    return xTimeNow;
}
/*-----------------------------------------------------------*/

/*
 * 将定时器插入到活动定时器列表中。
 *
 * 参数:
 * @param pxTimer: 定时器结构的指针。
 * @param xNextExpiryTime: 下一个过期时间（以滴答为单位）。
 * @param xTimeNow: 当前时间（以滴答为单位）。
 * @param xCommandTime: 发出命令的时间（以滴答为单位）。
 *
 * 返回值:
 * 如果定时器应该立即处理，则返回 pdTRUE，否则返回 pdFALSE。
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )
{
    BaseType_t xProcessTimerNow = pdFALSE;

    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    if( xNextExpiryTime <= xTimeNow )
    {
        /* 检查定时器过期时间是否在发出命令和处理命令之间已经过去。 */
        if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) /*lint !e961 MISRA 例外，因为类型转换对于某些端口只是冗余的。 */
        {
            /* 实际上，命令发出和处理之间的时间超过了定时器的周期。 */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            vListInsert( pxOverflowTimerList, &( pxTimer->xTimerListItem ) );
        }
    }
    else
    {
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
        {
            /* 如果自命令发出以来计数已经溢出，但过期时间尚未到达，
               则定时器实际上已经过了它的过期时间，应该立即处理。 */
            xProcessTimerNow = pdTRUE;
        }
        else
        {
            vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
        }
    }

    return xProcessTimerNow;
}
/*-----------------------------------------------------------*/

/*
 * 处理从定时器队列接收的命令。
 *
 * 功能:
 * 此函数会从定时器队列 xTimerQueue 中读取所有可用的消息，并根据消息类型执行相应的操作。
 * 包括处理定时器的启动、重启、停止、删除等命令，以及执行挂起的函数调用。
 */
static void prvProcessReceivedCommands( void )
{
    DaemonTaskMessage_t xMessage;
    Timer_t *pxTimer;
    BaseType_t xTimerListsWereSwitched, xResult;
    TickType_t xTimeNow;

    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) /*lint !e603 xMessage 不必初始化，因为它是在传出而不是传入，并且除非 xQueueReceive() 返回 pdTRUE 否则不会使用它。*/
    {
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
        {
            /* 负数命令是挂起的函数调用而非定时器命令。 */
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                /* 定时器使用 xCallbackParameters 成员请求执行回调。检查回调不是 NULL。 */
                configASSERT( pxCallback );

                /* 执行回调函数。 */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* 正数命令是定时器命令而非挂起的函数调用。 */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            /* 消息使用 xTimerParameters 成员来处理软件定时器。 */
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
            {
                /* 定时器在列表中，将其移除。 */
                ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

            /* 在这种情况下，xTimerListsWereSwitched 参数没有使用，但它必须出现在函数调用中。
            prvSampleTimeNow() 必须在从 xTimerQueue 接收消息之后调用，以便没有任何更高优先级的任务
            在时间比定时器守护进程任务提前的情况下将消息添加到消息队列（因为它抢占了定时器守护进程任务后设置了 xTimeNow 值）。 */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            switch( xMessage.xMessageID )
            {
                case tmrCOMMAND_START :
                case tmrCOMMAND_START_FROM_ISR :
                case tmrCOMMAND_RESET :
                case tmrCOMMAND_RESET_FROM_ISR :
                case tmrCOMMAND_START_DONT_TRACE :
                    /* 启动或重启定时器。 */
                    if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
                    {
                        /* 定时器在被加入活动定时器列表之前就已经过期。现在处理它。 */
                        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
                        traceTIMER_EXPIRED( pxTimer );

                        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
                        {
                            xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, NULL, tmrNO_DELAY );
                            configASSERT( xResult );
                            ( void ) xResult;
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
                    break;

                case tmrCOMMAND_STOP :
                case tmrCOMMAND_STOP_FROM_ISR :
                    /* 定时器已经被从活动列表中移除。这里没有什么要做的。 */
                    break;

                case tmrCOMMAND_CHANGE_PERIOD :
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
                    pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
                    configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

                    /* 新周期没有真正的参考，并且可能比旧周期长或短。
                       命令时间因此被设置为当前时间，并且由于周期不能为零，
                       下一个过期时间只能在未来，意味着（不像上面的 xTimerStart() 情况）
                       这里没有需要处理的失败情况。 */
                    ( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
                    break;

                case tmrCOMMAND_DELETE :
                    /* 定时器已经被从活动列表中移除，如果内存是动态分配的，则释放内存。 */
                    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
                    {
                        /* 定时器只能是动态分配的 - 再次释放它。 */
                        vPortFree( pxTimer );
                    }
                    #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
                    {
                        /* 定时器可能是静态分配的或动态分配的，所以在尝试释放内存之前检查一下。 */
                        if( pxTimer->ucStaticallyAllocated == ( uint8_t ) pdFALSE )
                        {
                            vPortFree( pxTimer );
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
                    break;

                default:
                    /* 不期望到达这里。 */
                    break;
            }
        }
    }
}
/*-----------------------------------------------------------*/

/*
 * 切换定时器列表。
 *
 * 功能:
 * 此函数用于处理系统计数器溢出的情况。当系统计数器溢出时，定时器列表需要进行切换。
 * 在切换之前，需要处理所有当前列表中的定时器，以确保它们不会错过它们的到期时间。
 * 如果有自动重载的定时器，还需要确保它们被正确地重新启动。
 */
static void prvSwitchTimerLists( void )
{
    TickType_t xNextExpireTime, xReloadTime;
    List_t *pxTemp;
    Timer_t *pxTimer;
    BaseType_t xResult;

    /* 系统计数器已经溢出。定时器列表必须切换。
       如果当前定时器列表中还有定时器，则它们一定已经过期，应在列表切换前处理它们。 */
    while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
    {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* 从列表中移除定时器。 */
        pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
        traceTIMER_EXPIRED( pxTimer );

        /* 执行其回调函数，然后如果它是自动重载定时器，则发送命令重启定时器。
           不能在这里重启定时器，因为列表还没有切换。 */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
        {
            /* 计算重载值，如果重载值导致定时器进入相同的定时器列表，则它已经过期，
               应该将定时器重新插入到当前列表中，以便在这个循环内再次处理它。
               否则，应发送命令重启定时器，以确保只有在列表交换后才将其插入到列表中。 */
            xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );
            if( xReloadTime > xNextExpireTime )
            {
                listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xReloadTime );
                listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );
                vListInsert( pxCurrentTimerList, &( pxTimer->xTimerListItem ) );
            }
            else
            {
                xResult = xTimerGenericCommand( pxTimer, tmrCOMMAND_START_DONT_TRACE, xNextExpireTime, NULL, tmrNO_DELAY );
                configASSERT( xResult );
                ( void ) xResult;
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    /* 交换定时器列表。 */
    pxTemp = pxCurrentTimerList;
    pxCurrentTimerList = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}
/*-----------------------------------------------------------*/

/*
 * 检查定时器列表和队列是否已初始化。
 *
 * 功能:
 * 此函数用于确保用于引用活动定时器的列表和用于与定时器服务通信的队列已被初始化。
 * 如果这些结构尚未初始化，则此函数将进行初始化。
 */
static void prvCheckForValidListAndQueue( void )
{
    /* 检查用于引用活动定时器的列表和用于与定时器服务通信的队列是否已初始化。 */
    taskENTER_CRITICAL();
    {
        if( xTimerQueue == NULL )
        {
            /* 初始化两个活动定时器列表。 */
            vListInitialise( &xActiveTimerList1 );
            vListInitialise( &xActiveTimerList2 );

            /* 设置当前定时器列表和溢出定时器列表。 */
            pxCurrentTimerList = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

            #if( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                /* 如果 configSUPPORT_DYNAMIC_ALLOCATION 为 0，则静态分配定时器队列。 */
                static StaticQueue_t xStaticTimerQueue;
                static uint8_t ucStaticTimerQueueStorage[ configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ];

                /* 创建静态定时器队列。 */
                xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
            }
            #else
            {
                /* 动态创建定时器队列。 */
                xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
            }
            #endif /* configSUPPORT_STATIC_ALLOCATION */

            #if ( configQUEUE_REGISTRY_SIZE > 0 )
            {
                /* 如果队列注册表大小大于0，则注册定时器队列。 */
                if( xTimerQueue != NULL )
                {
                    /* 将定时器队列注册到队列注册表中。 */
                    vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                }
                else
                {
                    /* 用于覆盖率测试的标记。 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {
            /* 用于覆盖率测试的标记。 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*
 * @brief xTimerIsTimerActive 检查定时器是否处于活动状态。
 *
 * 此函数用于检查指定的定时器是否处于活动状态（即是否在当前或溢出定时器列表中）。
 *
 * @param xTimer 定时器句柄。
 *
 * @return 返回值指示定时器是否处于活动状态。如果定时器处于活动状态，则返回 pdTRUE，否则返回 pdFALSE。
 */
BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer)
{
    BaseType_t xTimerIsInActiveList; // 初始化返回值
    Timer_t *pxTimer = (Timer_t *)xTimer; // 定义指向定时器结构的指针

    // 断言确保定时器句柄有效
    configASSERT(xTimer);

    /* 检查定时器是否在活动定时器列表中 */
    taskENTER_CRITICAL();
    {
        /* 检查定时器是否在空列表中实际上是为了检查它是否被包含在当前定时器列表或溢出定时器列表中，
         * 但由于逻辑需要反转，所以使用 '!' 进行取反操作。 */
        xTimerIsInActiveList = (BaseType_t)!listIS_CONTAINED_WITHIN(NULL, &(pxTimer->xTimerListItem));
    }
    taskEXIT_CRITICAL();

    return xTimerIsInActiveList;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/
/*
 * 获取定时器的标识符。
 *
 * 参数:
 * @param xTimer: 定时器句柄。
 *
 * 返回值:
 * 返回定时器的标识符。
 */
void *pvTimerGetTimerID( const TimerHandle_t xTimer )
{
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;
    void *pvReturn;

    // 断言确保定时器句柄有效
    configASSERT( xTimer );

    taskENTER_CRITICAL();
    {
        // 在临界区获取定时器的标识符
        pvReturn = pxTimer->pvTimerID;
    }
    taskEXIT_CRITICAL();

    return pvReturn;
}
/*-----------------------------------------------------------*/

/*
 * 设置定时器的标识符。
 *
 * 参数:
 * @param xTimer: 定时器句柄。
 * @param pvNewID: 新的定时器标识符。
 */
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )
{
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;

    // 断言确保定时器句柄有效
    configASSERT( xTimer );

    taskENTER_CRITICAL();
    {
        // 在临界区设置定时器的新标识符
        pxTimer->pvTimerID = pvNewID;
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*
 * 从中断服务程序中挂起一个函数调用。
 *
 * 功能:
 * 此函数用于在中断上下文中安排一个函数的执行。该函数将在定时器守护进程任务中被执行。
 *
 * 参数:
 * @param xFunctionToPend: 需要在稍后执行的函数指针。
 * @param pvParameter1: 传递给函数的第一个参数。
 * @param ulParameter2: 传递给函数的第二个参数。
 * @param pxHigherPriorityTaskWoken: 如果此函数导致更高优先级的任务被唤醒，则设置为 pdTRUE。
 *
 * 返回值:
 * 返回 xQueueSendFromISR 的结果，指示是否成功发送消息。
 */
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )
{
    DaemonTaskMessage_t xMessage;
    BaseType_t xReturn;

    /* 使用函数参数填充消息并将其发布到守护进程任务。 */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* 从中断服务程序向定时器队列发送消息。 */
    xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

    /* 记录函数挂起的信息。 */
    tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*
 * 挂起一个函数调用。
 *
 * 功能:
 * 此函数用于在任务上下文中安排一个函数的执行。该函数将在定时器守护进程任务中被执行。
 *
 * 参数:
 * @param xFunctionToPend: 需要在稍后执行的函数指针。
 * @param pvParameter1: 传递给函数的第一个参数。
 * @param ulParameter2: 传递给函数的第二个参数。
 * @param xTicksToWait: 等待的时间（滴答数），用于等待消息发送到队列。
 *
 * 返回值:
 * 返回 xQueueSendToBack 的结果，指示是否成功发送消息。
 */
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait )
{
    DaemonTaskMessage_t xMessage;
    BaseType_t xReturn;

    /* 只能在创建定时器之后或调度器启动之后调用此函数，
       因为在此之前，定时器队列不存在。 */
    configASSERT( xTimerQueue );

    /* 使用函数参数填充消息并将其发布到守护进程任务。 */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* 将消息发送到定时器队列的末尾，并等待指定的时间。 */
    xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

    /* 记录函数挂起的信息。 */
    tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

/* 如果应用程序没有配置包含软件定时器功能，那么整个源文件将会被忽略。如果你想包含软件定时器功能，请确保在 FreeRTOSConfig.h 中将 configUSE_TIMERS 设置为 1。*/
#endif /* configUSE_TIMERS == 1 */



