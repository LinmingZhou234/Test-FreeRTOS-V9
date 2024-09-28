/* 标准库头文件包括。 */
#include <stdlib.h>
#include <string.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 防止 task.h 重新定义所有 API 函数以使用 MPU 包装器。
这应该只在 task.h 从应用程序文件中包含时发生。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS 头文件包括。 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "StackMacros.h"

/* Lint e961 和 e750 被取消定义为 MISRA 异常，原因是 MPU 端口要求在上面的头文件中定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
但不能在这个文件中定义，以便生成正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* 设置 configUSE_STATS_FORMATTING_FUNCTIONS 为 2 以包含统计格式化函数，但不在此处包含 stdio.h。 */
#if ( configUSE_STATS_FORMATTING_FUNCTIONS == 1 )
	/* 在这个文件的底部有两个可选的函数，可以用来从 uxTaskGetSystemState() 函数生成的原始数据中生成人类可读的文本。
	注意，格式化函数只是为了方便提供，并且不被认为是内核的一部分。 */
	#include <stdio.h>
#endif /* configUSE_STATS_FORMATTING_FUNCTIONS == 1 */

#if( configUSE_PREEMPTION == 0 )
	/* 如果使用协作调度器，则不应该仅仅因为唤醒了一个更高优先级的任务就执行上下文切换。 */
	#define taskYIELD_IF_USING_PREEMPTION()
#else
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/* 可以为 TCB 中的 ucNotifyState 成员赋的值。 */
#define taskNOT_WAITING_NOTIFICATION	( ( uint8_t ) 0 )
#define taskWAITING_NOTIFICATION		( ( uint8_t ) 1 )
#define taskNOTIFICATION_RECEIVED		( ( uint8_t ) 2 )

/*
 * 当任务创建时用于填充任务栈的值。这纯粹是为了检查任务的高水位标记。
 */
#define tskSTACK_FILL_BYTE	( 0xa5U )

/* 
 * 有时 FreeRTOSConfig.h 设置仅允许使用动态分配的 RAM 创建任务，在这种情况下，当任何任务被删除时，已知需要释放任务的栈和TCB。
 * 有时 FreeRTOSConfig.h 设置仅允许使用静态分配的 RAM 创建任务，在这种情况下，当任何任务被删除时，已知不需要释放任务的栈或TCB。
 * 有时 FreeRTOSConfig.h 设置允许使用静态或动态分配的 RAM 创建任务，在这种情况下，TCB的一个成员用于记录栈和/或TCB是静态还是动态分配的，
 * 因此当任务被删除时，再次释放动态分配的 RAM，并且不尝试释放静态分配的 RAM。
 * tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE 只有在可能使用静态或动态分配的 RAM 创建任务时才为真。
 * 注意，如果 portUSING_MPU_WRAPPERS 为 1，则可以创建具有静态分配的栈和动态分配的TCB的受保护任务。
 */
#define tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE ( ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) || ( portUSING_MPU_WRAPPERS == 1 ) )
#define tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB 		( ( uint8_t ) 0 )	/* 动态分配的栈和TCB */
#define tskSTATICALLY_ALLOCATED_STACK_ONLY 			( ( uint8_t ) 1 )	/* 静态分配的栈 */
#define tskSTATICALLY_ALLOCATED_STACK_AND_TCB		( ( uint8_t ) 2 )	/* 静态分配的栈和TCB */

/*
 * 由 vListTasks 使用的宏，用于指示任务所处的状态。
 */
#define tskBLOCKED_CHAR		( 'B' )	/* 阻塞状态 */
#define tskREADY_CHAR		( 'R' )	/* 就绪状态 */
#define tskDELETED_CHAR		( 'D' )	/* 已删除状态 */
#define tskSUSPENDED_CHAR	( 'S' )	/* 挂起状态 */

/*
 * 某些内核感知的调试器要求调试器需要访问的数据是全局的，而不是文件作用域的。
 */
#ifdef portREMOVE_STATIC_QUALIFIER
	#define static	/* 取消静态限定符 */
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )

	/* 如果 configUSE_PORT_OPTIMISED_TASK_SELECTION 为 0，则任务选择是以通用的方式进行的，这种方式不对任何特定的微控制器架构进行优化。 */

/* uxTopReadyPriority 保存最高优先级就绪状态任务的优先级。 */
#define taskRECORD_READY_PRIORITY( uxPriority ) \
{ \
    if( ( uxPriority ) > uxTopReadyPriority ) \
    { \
        uxTopReadyPriority = ( uxPriority ); \
    } \
} /* taskRECORD_READY_PRIORITY */

/*-----------------------------------------------------------*/

#define taskSELECT_HIGHEST_PRIORITY_TASK() \
{ \
    UBaseType_t uxTopPriority = uxTopReadyPriority; \
 \
    /* 查找包含就绪任务的最高优先级队列。 */ \
    while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) ) \
    { \
        configASSERT( uxTopPriority ); \
        --uxTopPriority; \
    } \
 \
    /* listGET_OWNER_OF_NEXT_ENTRY 通过列表索引，使得相同优先级的任务能够平等地分享处理器时间。 */ \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
    uxTopReadyPriority = uxTopPriority; \
} /* taskSELECT_HIGHEST_PRIORITY_TASK */

/*-----------------------------------------------------------*/

/* 当不使用特定架构优化的任务选择时，定义掉 taskRESET_READY_PRIORITY() 和 portRESET_READY_PRIORITY()。 */
#define taskRESET_READY_PRIORITY( uxPriority )
#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/* 如果 configUSE_PORT_OPTIMISED_TASK_SELECTION 为 1，则任务选择是以特定微控制器架构优化的方式进行的。 */

/* 提供了针对特定架构优化的版本。调用端口定义的宏。 */
#define taskRECORD_READY_PRIORITY( uxPriority ) portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

/*-----------------------------------------------------------*/

#define taskSELECT_HIGHEST_PRIORITY_TASK() \
{ \
    UBaseType_t uxTopPriority; \
 \
    /* 查找包含就绪任务的最高优先级列表。 */ \
    portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority ); \
    configASSERT( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ uxTopPriority ] ) ) > 0 ); \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

/*-----------------------------------------------------------*/

/* 提供了针对特定架构优化的版本，仅在被重置的 TCB 被从就绪列表引用时调用。如果它被从延迟或挂起列表引用，则它不会出现在就绪列表中。 */
#define taskRESET_READY_PRIORITY( uxPriority ) \
{ \
    if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 ) \
    { \
        portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) ); \
    } \
}

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/*-----------------------------------------------------------*/

/* 当计数器溢出时，pxDelayedTaskList 和 pxOverflowDelayedTaskList 会被交换。 */
#define taskSWITCH_DELAYED_LISTS() \
{ \
    List_t *pxTemp; \
 \
    /* 切换列表时，延迟任务列表应该是空的。 */ \
    configASSERT( ( listLIST_IS_EMPTY( pxDelayedTaskList ) ) ); \
 \
    pxTemp = pxDelayedTaskList; \
    pxDelayedTaskList = pxOverflowDelayedTaskList; \
    pxOverflowDelayedTaskList = pxTemp; \
    xNumOfOverflows++; \
    prvResetNextTaskUnblockTime(); \
} /* taskSWITCH_DELAYED_LISTS */

/*-----------------------------------------------------------*/

/*
 * 将 pxTCB 所代表的任务放入适当的就绪列表中。它被插入到列表的末尾。
 */
#define prvAddTaskToReadyList( pxTCB ) \
do { \
    traceMOVED_TASK_TO_READY_STATE( pxTCB ); \
    taskRECORD_READY_PRIORITY( ( pxTCB )->uxPriority ); \
    vListInsertEnd( &( pxReadyTasksLists[ ( pxTCB )->uxPriority ] ), &( ( pxTCB )->xStateListItem ) ); \
    tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB ); \
} while(0)

/*-----------------------------------------------------------*/

/*
 * 多个函数接受一个可以为 NULL 的 TaskHandle_t 参数，其中 NULL 表示应使用当前正在执行的任务的句柄来代替参数。
 * 此宏仅仅是检查参数是否为 NULL，并返回指向适当 TCB 的指针。
 */
#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? ( TCB_t * ) pxCurrentTCB : ( TCB_t * ) ( pxHandle ) )

/* 
 * 事件列表项的值通常用于保存所属任务的优先级（编码以允许其按逆序优先级顺序存储）。
 * 然而，有时它会被借用于其他目的。重要的是，在它被借用于其他目的时，其值不应由于任务优先级的变化而被更新。
 * 下面的位定义用于告知调度器该值不应被改变——在这种情况下，借用该值的模块有责任确保在它被释放时恢复其原始值。
 */
#if( configUSE_16_BIT_TICKS == 1 )
    /* 对于使用16位ticks的情况 */
    #define taskEVENT_LIST_ITEM_VALUE_IN_USE 0x8000U
#else
    /* 对于使用32位ticks的情况 */
    #define taskEVENT_LIST_ITEM_VALUE_IN_USE 0x80000000UL
#endif /* configUSE_16_BIT_TICKS == 1 */

/*
 * 任务控制块（TCB）。每个任务都会分配一个任务控制块（TCB），用于存储任务的状态信息，
 * 包括指向任务上下文（运行时环境，包括寄存器值）的指针。
 */
typedef struct tskTaskControlBlock
{
    volatile StackType_t *pxTopOfStack; /*< 指向任务栈顶的位置。这是最后一个放入任务栈的项。必须是 TCB 结构的第一个成员。 */

    #if (portUSING_MPU_WRAPPERS == 1)
        xMPU_SETTINGS xMPUSettings;     /*< 在使用 MPU 的平台上，定义了 MPU 设置。必须是 TCB 结构的第二个成员。 */
    #endif

    ListItem_t xStateListItem;          /*< 任务状态列表项。任务从该列表项引用的状态列表决定了任务的状态（就绪、阻塞、挂起）。 */
    ListItem_t xEventListItem;          /*< 用于从事件列表中引用任务。 */

    UBaseType_t uxPriority;             /*< 任务的优先级。0 是最低优先级。 */

    StackType_t *pxStack;               /*< 指向任务栈的起始位置。 */

    char pcTaskName[configMAX_TASK_NAME_LEN]; /*< 在任务创建时给任务的描述性名称。仅用于调试。 */
    /* lint !e971 允许字符串和单字符使用未限定的 char 类型。 */

    #if (portSTACK_GROWTH > 0)
        StackType_t *pxEndOfStack;      /*< 指向架构上栈增长方向的末端位置。对于栈向下增长的架构，指向栈底。 */
    #endif

    #if (portCRITICAL_NESTING_IN_TCB == 1)
        UBaseType_t uxCriticalNesting;  /*< 对于不在端口层维护自身计数的平台，保存关键区嵌套深度。 */
    #endif

    #if (configUSE_TRACE_FACILITY == 1)
        UBaseType_t uxTCBNumber;        /*< 每次创建 TCB 时递增的数字。允许调试器确定任务何时被删除并重新创建。 */
        UBaseType_t uxTaskNumber;       /*< 专用于第三方跟踪代码的数字。 */
    #endif

    #if (configUSE_MUTEXES == 1)
        UBaseType_t uxBasePriority;     /*< 任务最后分配的优先级。用于优先级继承机制。 */
        UBaseType_t uxMutexesHeld;      /*< 任务持有的互斥锁数量。 */
    #endif

    #if (configUSE_APPLICATION_TASK_TAG == 1)
        TaskHookFunction_t pxTaskTag;   /*< 应用程序任务标签。 */
    #endif

    #if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
        void *pvThreadLocalStoragePointers[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
    #endif

    #if (configGENERATE_RUN_TIME_STATS == 1)
        uint32_t ulRunTimeCounter;      /*< 存储任务处于运行状态的时间量。 */
    #endif

    #if (configUSE_NEWLIB_REENTRANT == 1)
        /* 分配一个特定于任务的 Newlib reent 结构。
         * 注意：应用户要求加入了 Newlib 支持，但 FreeRTOS 维护者自己并未使用。
         * FreeRTOS 不负责由此导致的 Newlib 操作。用户必须熟悉 Newlib 并提供必要的存根实现。
         * 警告：目前 Newlib 设计实现了一个系统范围内的 malloc()，必须提供锁。 */
        struct _reent xNewLib_reent;
    #endif

    #if (configUSE_TASK_NOTIFICATIONS == 1)
        volatile uint32_t ulNotifiedValue; /*< 通知值。 */
        volatile uint8_t ucNotifyState;    /*< 通知状态。 */
    #endif

    /* 参见 tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE 定义上方的注释。 */
    #if (tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0)
        uint8_t ucStaticallyAllocated;    /*< 如果任务是静态分配的，则设置为 pdTRUE，以确保不会尝试释放内存。 */
    #endif

    #if (INCLUDE_xTaskAbortDelay == 1)
        uint8_t ucDelayAborted;           /*< 延迟是否被取消。 */
    #endif

} tskTCB;

/* 
 * 保留旧的 tskTCB 名称，并将其 typedef 为新的 TCB_t 名称，以便使用较旧的内核感知调试器。
 */
typedef tskTCB TCB_t;

/*lint -e956 通过对静态变量的手动分析和检查确定了哪些必须声明为 volatile。 */

PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;

/* 用于就绪和阻塞任务的列表。 ---------------------------- */
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ]; /*< 按优先级排序的就绪任务列表。 */
PRIVILEGED_DATA static List_t xDelayedTaskList1;                        /*< 延迟任务列表。 */
PRIVILEGED_DATA static List_t xDelayedTaskList2;                        /*< 延迟任务列表（使用两个列表——一个用于已超出当前计数值的延迟）。 */
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;             /*< 指向当前使用的延迟任务列表。 */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;     /*< 指向用于保存已超出当前计数值的任务的延迟任务列表。 */
PRIVILEGED_DATA static List_t xPendingReadyList;                        /*< 当调度器被挂起时变为就绪状态的任务。它们将在调度器恢复时被移到就绪列表中。 */

#if( INCLUDE_vTaskDelete == 1 )

    PRIVILEGED_DATA static List_t xTasksWaitingTermination;              /*< 已被删除但其内存尚未释放的任务。 */
    PRIVILEGED_DATA static volatile UBaseType_t uxDeletedTasksWaitingCleanUp = ( UBaseType_t ) 0U;

#endif

#if ( INCLUDE_vTaskSuspend == 1 )

    PRIVILEGED_DATA static List_t xSuspendedTaskList;                   /*< 当前被挂起的任务列表。 */

#endif

/* 其他文件私有的变量。 ------------------------------------ */
//PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;      /*< 当前系统的任务总数。 */

//为了使用keil的debug功能查看cpu的利用率，在main.c里重新实现了任务运行时间的查看函数vTaskGetRunTime，需要在main.c里引用uxCurrentNumberOfTasks，所以对其进行了重新定义
UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;      /*< 当前系统的任务总数。 */


PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) 0U;                    /*< 当前系统的时间刻度计数值。 */
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;            /*< 当前最高优先级就绪任务的优先级。 */
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;                       /*< 调度器是否正在运行的标志。 */
PRIVILEGED_DATA static volatile UBaseType_t uxPendedTicks = ( UBaseType_t ) 0U;              /*< 未处理的tick数量。 */
PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;                           /*< 是否有待处理的任务切换请求。 */
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;               /*< 计数器溢出次数。 */
PRIVILEGED_DATA static UBaseType_t uxTaskNumber = ( UBaseType_t ) 0U;                        /*< 任务编号。 */
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;         /*< 下一个任务解阻塞的时间。在调度器启动前初始化为 portMAX_DELAY。 */
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;                                  /*< 保存空闲任务的句柄。空闲任务在调度器启动时自动创建。 */




/* 上下文切换在调度器挂起时被挂起。此外，
中断在调度器挂起时不能操纵TCB的xStateListItem或任何xStateListItem可以引用的列表。
如果中断需要在调度器挂起时解除任务的阻塞，则将任务的事件列表项移动到xPendingReadyList中，
以便内核在调度器解除挂起时将任务从待处理就绪列表移动到实际的就绪列表。
待处理就绪列表本身只能在临界区中访问。 */
PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;

#if ( configGENERATE_RUN_TIME_STATS == 1 )

    PRIVILEGED_DATA static uint32_t ulTaskSwitchedInTime = 0UL;   /*< 保存上次任务切换进入时定时器/计数器的值。 */
    PRIVILEGED_DATA static uint32_t ulTotalRunTime = 0UL;         /*< 保存总执行时间，由运行时计数器时钟定义。 */

#endif

/*lint +e956 */

/*-----------------------------------------------------------*/

/* 回调函数原型声明。 --------------------------------------*/
#if(  configCHECK_FOR_STACK_OVERFLOW > 0 )
    extern void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );
#endif

#if( configUSE_TICK_HOOK > 0 )
    extern void vApplicationTickHook( void );
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
    extern void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
#endif

/* 文件私有函数声明。 --------------------------------*/

/**
 * 辅助函数，用于判断由 xTask 引用的任务是否当前处于挂起状态。
 * 如果 xTask 引用的任务处于挂起状态，则返回 pdTRUE；否则返回 pdFALSE。
 */
#if ( INCLUDE_vTaskSuspend == 1 )
	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif /* INCLUDE_vTaskSuspend */

/*
 * 初始化所有由调度器使用的列表。这会在创建第一个任务时自动调用。
 */
static void prvInitialiseTaskLists( void ) PRIVILEGED_FUNCTION;

/*
 * 空闲任务，和其他所有任务一样，是一个永无止境的循环。
 * 空闲任务会在创建第一个用户任务时自动创建并加入就绪列表。
 *
 * 使用 portTASK_FUNCTION_PROTO() 宏是为了允许特定于端口/编译器的语言扩展。
 * 该函数的等效原型为：
 *
 * void prvIdleTask( void *pvParameters );
 */
static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );

/*
 * 用于释放由调度器分配的所有内存，包括TCB所指向的栈空间。
 *
 * 这不包括任务自身分配的内存（即，在任务的应用代码中通过调用 pvPortMalloc 分配的内存）。
 */
#if ( INCLUDE_vTaskDelete == 1 )

	static void prvDeleteTCB( TCB_t *pxTCB ) PRIVILEGED_FUNCTION;

#endif

/*
 * 仅由空闲任务使用。检查是否有任务被放置在等待删除的任务列表中。
 * 如果有，则清理任务并删除其TCB。
 */
static void prvCheckTasksWaitingTermination( void ) PRIVILEGED_FUNCTION;

/*
 * 当前执行的任务即将进入阻塞状态。将任务添加到当前的延迟任务列表或溢出延迟任务列表。
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * 用信息填充一个 TaskStatus_t 结构体，这些信息来自于 pxList 列表中的每个任务（pxList 可能是一个就绪列表、延迟列表、挂起列表等）。
 *
 * 此函数仅供调试使用，不应在正常应用程序代码中调用。
 */
#if ( configUSE_TRACE_FACILITY == 1 )

	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState ) PRIVILEGED_FUNCTION;

#endif

/*
 * 在 pxList 中搜索名为 pcNameToQuery 的任务 - 如果找到则返回该任务的句柄，如果没有找到则返回 NULL。
 */
#if ( INCLUDE_xTaskGetHandle == 1 )

	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] ) PRIVILEGED_FUNCTION;

#endif

/*
 * 创建任务时，任务的栈被填充为一个已知的值。此函数通过确定栈中仍然保持初始预设值的部分来确定任务栈的“高水位标记”。
 */
#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

	static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte ) PRIVILEGED_FUNCTION;

#endif

/*
 * 返回内核下次将任务从阻塞状态移至运行状态所需的时间（以滴答为单位）。
 *
 * 此条件编译应该使用不等于 0 的条件，而不是等于 1 的条件。
 * 这是为了确保即使用户定义的低功耗模式实现要求 configUSE_TICKLESS_IDLE 设置为其他值时，也能调用 portSUPPRESS_TICKS_AND_SLEEP()。
 */
#if ( configUSE_TICKLESS_IDLE != 0 )

	static TickType_t prvGetExpectedIdleTime( void ) PRIVILEGED_FUNCTION;

#endif

/*
 * 将 xNextTaskUnblockTime 设置为下一个阻塞状态任务退出阻塞状态的时间。
 */
static void prvResetNextTaskUnblockTime( void );

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	/*
	 * 用于在打印任务信息的人类可读表格时，在任务名称后填充空格的辅助函数。
	 */
	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName ) PRIVILEGED_FUNCTION;

#endif
/**
 * @brief 填充新创建的任务的 TCB 结构体。
 *
 * 该函数在分配了 Task_t 结构体之后（无论是静态还是动态）用于初始化结构体成员。
 *
 * @param pxTaskCode 任务的入口函数指针。
 * @param pcName 任务的名称，用于调试或统计。
 * @param ulStackDepth 任务栈的深度，以字节数为单位。
 * @param pvParameters 传递给任务函数的参数。
 * @param uxPriority 任务的优先级。
 * @param pxCreatedTask 指向任务句柄的指针，用于返回创建的任务的句柄。
 * @param pxNewTCB 指向新创建的任务控制块 (TCB) 的指针。
 * @param xRegions 内存区域信息，如果为 NULL 则不使用。
 */
static void prvInitialiseNewTask( TaskFunction_t pxTaskCode,
								  const char * const pcName,
								  const uint32_t ulStackDepth,
								  void * const pvParameters,
								  UBaseType_t uxPriority,
								  TaskHandle_t * const pxCreatedTask,
								  TCB_t *pxNewTCB,
								  const MemoryRegion_t * const xRegions ) PRIVILEGED_FUNCTION;

/*
 * 在创建并初始化新任务之后，用于将任务置于调度器的控制之下。
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

/**
 * @brief 静态创建一个任务。
 *
 * 此函数用于创建一个任务，使用静态分配的内存来存储任务控制块 (TCB) 和栈。
 *
 * @param pxTaskCode 指向任务入口函数的指针。
 * @param pcName 任务的名称，用于调试或统计。
 * @param ulStackDepth 任务栈的深度，以字节数为单位。
 * @param pvParameters 传递给任务函数的参数。
 * @param uxPriority 任务的优先级。
 * @param puxStackBuffer 静态分配的栈缓冲区。
 * @param pxTaskBuffer 静态分配的任务控制块 (TCB) 缓冲区。
 *
 * @return TaskHandle_t 创建的任务的句柄，如果创建失败则返回 NULL。
 */
TaskHandle_t xTaskCreateStatic(
    TaskFunction_t pxTaskCode,          /**< 指向任务入口函数的指针。 */
    const char * const pcName,          /**< 任务的名称，用于调试或统计。 */
    const uint32_t ulStackDepth,        /**< 任务栈的深度，以字节数为单位。 */
    void * const pvParameters,          /**< 传递给任务函数的参数。 */
    UBaseType_t uxPriority,             /**< 任务的优先级。 */
    StackType_t * const puxStackBuffer, /**< 静态分配的栈缓冲区。 */
    StaticTask_t * const pxTaskBuffer   /**< 静态分配的任务控制块 (TCB) 缓冲区。 */
)
{
    TCB_t *pxNewTCB;
    TaskHandle_t xReturn;

    /* 
     * 断言检查栈缓冲区和任务缓冲区是否不为空。
     * 如果任何一个为 NULL，则表明输入参数错误。
     */
    configASSERT( puxStackBuffer != NULL );
    configASSERT( pxTaskBuffer != NULL );

    if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
    {
        /* 
         * 使用传入的内存作为任务的 TCB 和栈。
         * 注意这里的类型转换是安全的，因为 TCB 和 StaticTask_t 设计了相同的对齐方式，并且大小由断言检查。
         */
        pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 不寻常的类型转换是可以接受的，因为结构设计了相同的对齐方式，并且大小由断言检查。 */
        pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            /* 
             * 任务可以静态或动态创建，因此记录该任务是静态创建的，以防任务稍后被删除。
             * 标记 TCB 为静态分配的，以便在删除任务时正确释放资源。
             */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* 
         * 初始化新任务，并将任务句柄存储在 xReturn 中。
         * 这里调用 prvInitialiseNewTask 来完成 TCB 的初始化。
         */
        prvInitialiseNewTask( pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, &xReturn, pxNewTCB, NULL );

        /* 
         * 将新任务添加到就绪列表。
         * 调用 prvAddNewTaskToReadyList 将任务加入到调度器的就绪队列。
         */
        prvAddNewTaskToReadyList( pxNewTCB );
    }
    else
    {
        /* 如果输入参数错误，则返回 NULL。 */
        xReturn = NULL;
    }

    /* 返回创建的任务的句柄。 */
    return xReturn;
}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( portUSING_MPU_WRAPPERS == 1 )

/**
 * @brief 受限创建一个任务。
 *
 * 此函数用于受限条件下创建一个任务，使用提供的栈缓冲区和任务定义结构体。
 *
 * @param pxTaskDefinition 指向包含任务定义信息的结构体的指针。
 * @param pxCreatedTask 指向任务句柄的指针，用于返回创建的任务的句柄。
 *
 * @return BaseType_t 返回创建状态，成功返回 pdPASS，否则返回 errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY。
 */
BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
{
    TCB_t *pxNewTCB;
    BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

    /* 
     * 断言检查栈缓冲区是否不为空。
     * 如果栈缓冲区为 NULL，则表明输入参数错误。
     */
    configASSERT( pxTaskDefinition->puxStackBuffer );

    if( pxTaskDefinition->puxStackBuffer != NULL )
    {
        /* 
         * 分配 TCB 的空间。
         * 内存来源取决于端口 malloc 函数的实现以及是否使用静态分配。
         */
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

        if( pxNewTCB != NULL )
        {
            /* 
             * 将栈位置存储在 TCB 中。
             * 这一步将任务栈的起始地址赋值给 TCB 的栈指针。
             */
            pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

            /* 
             * 记录任务具有静态分配的栈，以防稍后删除任务。
             * 设置 TCB 标记，表明栈是静态分配的。
             */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_ONLY;

            /* 
             * 初始化新任务，并将任务句柄存储在 pxCreatedTask 中。
             * 调用 prvInitialiseNewTask 来初始化 TCB 并设置任务的相关属性。
             */
            prvInitialiseNewTask( pxTaskDefinition->pvTaskCode,
                                  pxTaskDefinition->pcName,
                                  ( uint32_t ) pxTaskDefinition->usStackDepth,
                                  pxTaskDefinition->pvParameters,
                                  pxTaskDefinition->uxPriority,
                                  pxCreatedTask, pxNewTCB,
                                  pxTaskDefinition->xRegions );

            /* 
             * 将新任务添加到就绪列表。
             * 调用 prvAddNewTaskToReadyList 将任务加入到调度器的就绪队列。
             */
            prvAddNewTaskToReadyList( pxNewTCB );
            xReturn = pdPASS;
        }
    }

    /* 返回创建状态。 */
    return xReturn;
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,                // 任务入口函数指针
                        const char * const pcName,              // 任务名称，用于调试
                        const uint16_t usStackDepth,           // 任务栈的深度
                        void * const pvParameters,              // 传递给任务的参数
                        UBaseType_t uxPriority,                // 任务优先级
                        TaskHandle_t * const pxCreatedTask)    // 输出参数，接收创建的任务句柄
{
    TCB_t *pxNewTCB;                                     // 定义一个指向任务控制块（TCB）的指针
    BaseType_t xReturn;                                  // 创建任务的返回值

    #if( portSTACK_GROWTH > 0 )                          // 如果栈向上增长
    {
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );  // 动态分配一个任务控制块
        if( pxNewTCB != NULL )                           // 检查分配是否成功
        {
            pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ); // 分配任务栈空间

            if( pxNewTCB->pxStack == NULL )             // 检查栈空间分配是否成功
            {
                vPortFree( pxNewTCB );                  // 如果栈分配失败，释放之前分配的 TCB
                pxNewTCB = NULL;                        // 将 TCB 指针设为 NULL
            }
        }
    }
    #else                                                  // 否则（栈向下增长）
    {
        StackType_t *pxStack;                            // 定义指向栈空间的指针
        pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); // 分配任务栈空间

        if( pxStack != NULL )                            // 检查栈空间分配是否成功
        {
            pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); // 动态分配一个任务控制块

            if( pxNewTCB != NULL )                       // 检查 TCb 分配是否成功
            {
                pxNewTCB->pxStack = pxStack;          // 将分配的栈指针赋值给 TCB
            }
            else
            {
                vPortFree( pxStack );                  // 如果 TCB 分配失败，释放之前分配的栈
            }
        }
        else
        {
            pxNewTCB = NULL;                            // 如果栈空间分配失败，设置 TCB 指针为 NULL
        }
    }
    #endif 

    if( pxNewTCB != NULL )                               // 检查 TCB 是否成功分配
    {
        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB; // 标记为动态分配的 TCB
        }
        #endif

        // 初始化新任务，并准备其内部状态
        prvInitialiseNewTask(pxTaskCode, pcName, ( uint32_t ) usStackDepth, pvParameters, uxPriority, pxCreatedTask, pxNewTCB, NULL );
        
        // 将新任务添加到调度队列中
        prvAddNewTaskToReadyList(pxNewTCB);
        xReturn = pdPASS;                               // 任务创建成功，返回成功标志
    }
    else
    {
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; // 分配失败，返回内存不足错误
    }

    return xReturn;                                     // 返回任务创建的结果
}


#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/**
 * @brief 初始化新创建的任务的 TCB 结构体。
 *
 * 该函数在分配了 Task_t 结构体之后（无论是静态还是动态）用于初始化结构体成员。
 *
 * @param pxTaskCode 任务的入口函数指针。
 * @param pcName 任务的名称，用于调试或统计。
 * @param ulStackDepth 任务栈的深度，以字节数为单位。
 * @param pvParameters 传递给任务函数的参数。
 * @param uxPriority 任务的优先级。
 * @param pxCreatedTask 指向任务句柄的指针，用于返回创建的任务的句柄。
 * @param pxNewTCB 指向新创建的任务控制块 (TCB) 的指针。
 * @param xRegions 内存区域信息，如果为 NULL 则不使用。
 */
static void prvInitialiseNewTask(
    TaskFunction_t pxTaskCode,          /**< 任务的入口函数指针。 */
    const char * const pcName,          /**< 任务的名称，用于调试或统计。 */
    const uint32_t ulStackDepth,        /**< 任务栈的深度，以字节数为单位。 */
    void * const pvParameters,          /**< 传递给任务函数的参数。 */
    UBaseType_t uxPriority,             /**< 任务的优先级。 */
    TaskHandle_t * const pxCreatedTask, /**< 指向任务句柄的指针，用于返回创建的任务的句柄。 */
    TCB_t *pxNewTCB,                    /**< 指向新创建的任务控制块 (TCB) 的指针。 */
    const MemoryRegion_t * const xRegions /**< 内存区域信息，如果为 NULL 则不使用。 */
) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    StackType_t *pxTopOfStack;
    UBaseType_t x;

    #if( portUSING_MPU_WRAPPERS == 1 )
    {
        /* 应该任务在特权模式下创建吗？ */
        BaseType_t xRunPrivileged;
        if( ( uxPriority & portPRIVILEGE_BIT ) != 0U )
        {
            xRunPrivileged = pdTRUE;
        }
        else
        {
            xRunPrivileged = pdFALSE;
        }
        uxPriority &= ~portPRIVILEGE_BIT;
    }
    #endif /* portUSING_MPU_WRAPPERS == 1 */

    /* 避免依赖 memset() 如果不需要的话。 */
    #if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
    {
        /* 用已知值填充栈以协助调试。 */
        ( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
    }
    #endif /* ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) ) */

    /* 计算栈顶地址。这取决于栈是从高地址到低地址增长（如 80x86），还是相反。
    portSTACK_GROWTH 用于使结果正负号符合端口的要求。 */
    #if( portSTACK_GROWTH < 0 )
    {
        pxTopOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
        pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA exception. 避免指针和整数之间的转换是不现实的。大小差异通过 portPOINTER_SIZE_TYPE 类型解决。 */

        /* 检查计算的栈顶地址是否对齐正确。 */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );
    }
    #else /* portSTACK_GROWTH */
    {
        pxTopOfStack = pxNewTCB->pxStack;

        /* 检查栈缓冲区的对齐是否正确。 */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

        /* 如果进行栈检查，则需要栈空间的另一端。 */
        pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
    }
    #endif /* portSTACK_GROWTH */

    /* 存储任务名称在 TCB 中。 */
    for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
    {
        pxNewTCB->pcTaskName[ x ] = pcName[ x ];

        /* 如果字符串长度小于 configMAX_TASK_NAME_LEN 字符，不要复制所有 configMAX_TASK_NAME_LEN，
        以防字符串末尾的内存不可访问（极其不可能）。 */
        if( pcName[ x ] == 0x00 )
        {
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    /* 确保字符串终止，以防字符串长度大于等于 configMAX_TASK_NAME_LEN。 */
    pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

    /* 这是一个数组索引，必须确保它不超过最大值。首先去除特权位（如果存在）。 */
    if( uxPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        uxPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    pxNewTCB->uxPriority = uxPriority;
    #if ( configUSE_MUTEXES == 1 )
    {
        pxNewTCB->uxBasePriority = uxPriority;
        pxNewTCB->uxMutexesHeld = 0;
    }
    #endif /* configUSE_MUTEXES */

    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* 设置 pxNewTCB 作为从 ListItem_t 回溯的链接。这是为了我们能从通用项中回到包含的 TCB。 */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* 事件列表总是按优先级顺序排列。 */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA exception 由于某些端口，转换只是多余的。 */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    #if ( portCRITICAL_NESTING_IN_TCB == 1 )
    {
        pxNewTCB->uxCriticalNesting = ( UBaseType_t ) 0U;
    }
    #endif /* portCRITICAL_NESTING_IN_TCB */

    #if ( configUSE_APPLICATION_TASK_TAG == 1 )
    {
        pxNewTCB->pxTaskTag = NULL;
    }
    #endif /* configUSE_APPLICATION_TASK_TAG */

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        pxNewTCB->ulRunTimeCounter = 0UL;
    }
    #endif /* configGENERATE_RUN_TIME_STATS */

    #if ( portUSING_MPU_WRAPPERS == 1 )
    {
        vPortStoreTaskMPUSettings( &( pxNewTCB->xMPUSettings ), xRegions, pxNewTCB->pxStack, ulStackDepth );
    }
    #else
    {
        /* 避免编译器警告关于未引用的参数。 */
        ( void ) xRegions;
    }
    #endif

    #if( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )
    {
        for( x = 0; x < ( UBaseType_t ) configNUM_THREAD_LOCAL_STORAGE_POINTERS; x++ )
        {
            pxNewTCB->pvThreadLocalStoragePointers[ x ] = NULL;
        }
    }
    #endif

    #if ( configUSE_TASK_NOTIFICATIONS == 1 )
    {
        pxNewTCB->ulNotifiedValue = 0;
        pxNewTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    #endif

    #if ( configUSE_NEWLIB_REENTRANT == 1 )
    {
        /* 初始化此任务的 Newlib 重入结构。 */
        _REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
    }
    #endif

    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        pxNewTCB->ucDelayAborted = pdFALSE;
    }
    #endif

    /* 初始化 TCB 栈，使其看起来像任务已经在运行，但被调度程序中断。
    返回地址设置为任务函数的开始处。一旦栈已经初始化，栈顶变量就会更新。 */
    #if( portUSING_MPU_WRAPPERS == 1 )
    {
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters, xRunPrivileged );
    }
    #else /* portUSING_MPU_WRAPPERS */
    {
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTaskCode, pvParameters );
    }
    #endif /* portUSING_MPU_WRAPPERS */

    if( ( void * ) pxCreatedTask != NULL )
    {
        /* 以匿名方式传递句柄。句柄可用于更改创建的任务的优先级、删除创建的任务等。*/
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief 将新任务添加到就绪列表。
 *
 * 此函数负责将一个新创建的任务添加到系统的就绪列表，并根据当前调度状态可能切换到新任务。
 *
 * @param pxNewTCB 指向新创建的任务控制块 (TCB) 的指针。
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
    /* 确保在更新任务列表时中断不会访问任务列表。 */
    taskENTER_CRITICAL();
    {
        uxCurrentNumberOfTasks++; /* 增加当前任务数量。 */

        if( pxCurrentTCB == NULL )
        {
            /* 如果没有其他任务，或者所有其他任务都处于挂起状态，则将此任务设为当前任务。 */
            pxCurrentTCB = pxNewTCB;

            if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
            {
                /* 如果这是创建的第一个任务，则执行所需的初步初始化。
                如果此调用失败，我们将报告失败但无法恢复。 */
                prvInitialiseTaskLists();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
            }
        }
        else
        {
            /* 如果调度器尚未运行，则将此任务设为当前任务，前提是它是迄今为止创建的最高优先级任务。 */
            if( xSchedulerRunning == pdFALSE )
            {
                if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
                {
                    pxCurrentTCB = pxNewTCB;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
            }
        }

        uxTaskNumber++; /* 增加任务计数器。 */

        #if ( configUSE_TRACE_FACILITY == 1 )
        {
            /* 为追踪目的，在 TCB 中添加一个计数器。 */
            pxNewTCB->uxTCBNumber = uxTaskNumber;
        }
        #endif /* configUSE_TRACE_FACILITY */

        traceTASK_CREATE( pxNewTCB ); /* 记录任务创建的日志。 */

        prvAddTaskToReadyList( pxNewTCB ); /* 将任务添加到就绪列表。 */

        portSETUP_TCB( pxNewTCB ); /* 为特定端口设置 TCB。 */
    }
    taskEXIT_CRITICAL(); /* 退出临界区。 */

    if( xSchedulerRunning != pdFALSE )
    {
        /* 如果创建的任务比当前任务具有更高的优先级，则应立即运行新任务。 */
        if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
        {
            taskYIELD_IF_USING_PREEMPTION(); /* 如果使用抢占式调度，则切换到更高优先级的任务。 */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
    }
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )
/*
 * @brief vTaskDelete 用于删除指定的任务。
 *
 * 该函数用于删除指定的任务，包括将任务从就绪列表和事件列表中删除，并释放相关的内存。
 *
 * @param xTaskToDelete 要删除的任务的句柄。
 */
void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    TCB_t *pxTCB;                                         // 定义一个指向任务控制块（TCB）的指针

    taskENTER_CRITICAL();                                 // 进入临界区，保护共享资源
    {
        pxTCB = prvGetTCBFromHandle(xTaskToDelete);       // 从任务句柄获取任务控制块（TCB）

        // 从就绪列表中移除该任务，如果移除后列表为空，则重置优先级
        if (uxListRemove(&(pxTCB->xStateListItem)) == (UBaseType_t) 0)
        {
            taskRESET_READY_PRIORITY(pxTCB->uxPriority);  // 重置任务的优先级
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                     // 执行覆盖率测试标记（非关键操作，可用于调试）
        }

        // 如果任务的事件列表不为空，则从事件列表中移除该任务
        if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL)
        {
            (void) uxListRemove(&(pxTCB->xEventListItem)); // 从事件列表中移除任务
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                     // 执行覆盖率测试标记（非关键操作，可用于调试）
        }

        uxTaskNumber++;                                   // 增加任务数量计数器

        if (pxTCB == pxCurrentTCB)                        // 检查是否删除的是当前正在运行的任务
        {
            // 将当前任务添加到等待清理的任务列表中
            vListInsertEnd(&xTasksWaitingTermination, &(pxTCB->xStateListItem));

            ++uxDeletedTasksWaitingCleanUp;               // 增加等待清理的任务数量
            
            portPRE_TASK_DELETE_HOOK(pxTCB, &xYieldPending); // 执行任务删除钩子，比如清理资源
        }
        else
        {
            --uxCurrentNumberOfTasks;                     // 非当前任务，减少当前任务数量计数器
            prvDeleteTCB(pxTCB);                         // 删除任务控制块（TCB），释放资源

            prvResetNextTaskUnblockTime();               // 重置下一个任务解锁时间
        }

        traceTASK_DELETE(pxTCB);                         // 记录任务删除事件（调试用）
    }
    taskEXIT_CRITICAL();                                 // 退出临界区，恢复共享资源的访问

    if (xSchedulerRunning != pdFALSE)                    // 检查调度器是否正在运行
    {
        if (pxTCB == pxCurrentTCB)                       // 如果删除的是当前运行的任务
        {
            configASSERT(uxSchedulerSuspended == 0);     // 断言调度器未暂停
            portYIELD_WITHIN_API();                      // 进行上下文切换
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                    // 执行覆盖率测试标记（非关键操作，可用于调试）
        }
    }
}


#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelayUntil == 1 )

	/*
 * @brief vTaskDelayUntil 使当前任务延迟到指定的时间点才唤醒。
 *
 * 此函数用于使当前任务延迟到指定的时间点才唤醒，确保任务按固定的时间间隔执行。
 *
 * @param pxPreviousWakeTime 指向一个变量的指针，该变量存储了任务上次唤醒的时间点。
 *                           这个值将被更新为下次预计唤醒的时间点。
 * @param xTimeIncrement 表示任务下次唤醒前需要等待的滴答数。
 */
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )
{
    TickType_t xTimeToWake;                  // 下次唤醒的时间点
    BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE; // 标记是否应该延迟

    // 断言确保 pxPreviousWakeTime 不为空
    configASSERT( pxPreviousWakeTime );
    // 断言确保延时时长大于0
    configASSERT( ( xTimeIncrement > 0U ) );
    // 断言确保调度器未被暂停
    configASSERT( uxSchedulerSuspended == 0 );

    // 暂停所有任务调度
    vTaskSuspendAll();
    {
        // 保存当前的滴答计数值，防止在此块内发生改变
        const TickType_t xConstTickCount = xTickCount;

        // 生成任务想要唤醒的时间点
        xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

        // 检查滴答计数是否溢出
        if( xConstTickCount < *pxPreviousWakeTime )
        {
            // 如果滴答计数已溢出，则需要进一步判断
            if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
            {
                // 如果唤醒时间也溢出，并且唤醒时间大于当前滴答计数，则需要延迟
                xShouldDelay = pdTRUE;
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 如果滴答计数未溢出，则需要进一步判断
            if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
            {
                // 如果唤醒时间溢出或唤醒时间大于当前滴答计数，则需要延迟
                xShouldDelay = pdTRUE;
            }
            else
            {
                // 覆盖测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }

        // 更新唤醒时间，为下次调用做准备
        *pxPreviousWakeTime = xTimeToWake;

        // 如果需要延迟，则将当前任务添加到延迟列表
        if( xShouldDelay != pdFALSE )
        {
            // 记录延迟信息（用于跟踪）
            traceTASK_DELAY_UNTIL( xTimeToWake );

            // 计算实际需要延迟的滴答数（唤醒时间减去当前滴答计数）
            prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // 恢复任务调度
    xAlreadyYielded = xTaskResumeAll();

    // 如果 xTaskResumeAll 已经进行了调度，则不需要再进行调度
    if( xAlreadyYielded == pdFALSE )
    {
        // 强制进行调度（如果当前任务已将自己置为睡眠状态）
        portYIELD_WITHIN_API();
    }
    else
    {
        // 覆盖测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}
#endif /* INCLUDE_vTaskDelayUntil */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelay == 1 )
//总的来说，任务阻塞函数就是将任务添加到阻塞列表，指定阻塞时长
void vTaskDelay(const TickType_t xTicksToDelay)             // 函数定义，阻塞当前任务指定的 Tick 数
{
    BaseType_t xAlreadyYielded = pdFALSE;                   // 标志变量，指示是否已经发生了任务切换

    if (xTicksToDelay > (TickType_t) 0U)                     // 检查延迟时间是否大于 0
    {
        configASSERT(uxSchedulerSuspended == 0);            // 断言调度器未暂停

        vTaskSuspendAll();                                   // 暂停调度器，准备进行延迟操作
        {
            traceTASK_DELAY();                               // 记录任务阻塞事件（用于调试或监控）

            prvAddCurrentTaskToDelayedList(xTicksToDelay, pdFALSE); // 将当前任务添加到延迟列表，指定延迟时长
        }
        xAlreadyYielded = xTaskResumeAll();                 // 恢复调度器，并检查是否需要切换任务
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();                            // 执行覆盖率测试标记（表示此分支的执行）
    }

    if (xAlreadyYielded == pdFALSE)                          // 检查是否需要切换任务
    {
        portYIELD_WITHIN_API();                              // 如果没有发生任务切换，则强制进行上下文切换
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();                            // 执行覆盖率测试标记（表示此分支的执行）
    }
}


#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/

#if( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) )

/*
 * @brief eTaskGetState 用于查询指定任务的状态。
 *
 * 该函数通过查询任务所在的列表来判断任务的状态，并返回相应的枚举值。
 *
 * @param xTask 要查询状态的任务的句柄。
 *
 * @return 返回任务的状态，枚举类型为 eTaskState。
 */
eTaskState eTaskGetState( TaskHandle_t xTask )
{
    eTaskState eReturn;                      // 定义返回的任务状态
    List_t *pxStateList;                     // 定义一个指向列表的指针
    const TCB_t * const pxTCB = ( TCB_t * ) xTask;  // 获取任务的TCB指针

    // 断言检查，确保任务句柄是非空的
    configASSERT( pxTCB );

    // 如果当前任务句柄指向的就是正在运行的任务
    if( pxTCB == pxCurrentTCB )
    {
        eReturn = eRunning;                  // 设置状态为正在运行
    }
    else
    {
        // 进入临界区，防止中断发生，确保操作的原子性
        taskENTER_CRITICAL();
        {
            // 获取任务状态列表的容器
            pxStateList = ( List_t * ) listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
        }
        // 退出临界区
        taskEXIT_CRITICAL();

        // 检查任务是否在延迟任务列表中
        if( ( pxStateList == pxDelayedTaskList ) || ( pxStateList == pxOverflowDelayedTaskList ) )
        {
            eReturn = eBlocked;              // 设置状态为被阻塞
        }

        #if ( INCLUDE_vTaskSuspend == 1 )
            // 如果任务在挂起任务列表中
            else if( pxStateList == &xSuspendedTaskList )
            {
                // 检查任务是否有事件列表项
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL )
                {
                    eReturn = eSuspended;    // 设置状态为被挂起
                }
                else
                {
                    eReturn = eBlocked;      // 如果有事件列表项，则设置状态为被阻塞
                }
            }
        #endif

        #if ( INCLUDE_vTaskDelete == 1 )
            // 如果任务在等待终止的任务列表中，或者列表为空
            else if( ( pxStateList == &xTasksWaitingTermination ) || ( pxStateList == NULL ) )
            {
                eReturn = eDeleted;          // 设置状态为已被删除
            }
        #endif

        else 
        {
            eReturn = eReady;                // 默认设置状态为就绪
        }
    }

    // 返回任务的状态
    return eReturn;
}
#endif /* INCLUDE_eTaskGetState */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

/*
 * @brief uxTaskPriorityGet 用于获取指定任务的优先级。
 *
 * 该函数用于获取由 `xTask` 指定的任务的优先级。
 *
 * @param xTask 要获取优先级的任务的句柄。
 *
 * @return 返回任务的优先级。
 */
UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask )
{
    TCB_t *pxTCB;                                       // 定义指向任务控制块（TCB）的指针
    UBaseType_t uxReturn;                               // 存储返回的优先级值

    taskENTER_CRITICAL();                               // 进入临界区，防止在读取任务状态时发生上下文切换
    {
        /* 从任务句柄中获取任务控制块（TCB）。 */
        pxTCB = prvGetTCBFromHandle( xTask );

        /* 读取任务的优先级。 */
        uxReturn = pxTCB->uxPriority;
    }
    taskEXIT_CRITICAL();                                // 退出临界区，恢复上下文切换

    return uxReturn;                                    // 返回任务的优先级
}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

/**
 * @brief 在中断服务例程中获取任务的优先级。
 *
 * 此函数用于在中断服务例程中安全地获取指定任务的优先级。
 *
 * @param xTask 指向要获取优先级的任务的句柄。
 *
 * @return UBaseType_t 返回任务的优先级。
 */
UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask )
{
    TCB_t *pxTCB;
    UBaseType_t uxReturn, uxSavedInterruptState;

    /* 确保中断优先级有效。 */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 保存当前中断屏蔽状态，并设置新的中断屏蔽状态。 */
    uxSavedInterruptState = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* 从任务句柄获取 TCB。 */
        pxTCB = prvGetTCBFromHandle( xTask );
        /* 获取任务的优先级。 */
        uxReturn = pxTCB->uxPriority;
    }
    /* 恢复之前的中断屏蔽状态。 */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptState );

    /* 返回任务的优先级。 */
    return uxReturn;
}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 )

/**
 * @brief 设置任务的优先级。
 *
 * 此函数用于设置指定任务的新优先级。
 *
 * @param xTask 要设置优先级的任务的句柄。
 * @param uxNewPriority 新的优先级。
 */
void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )
{
    TCB_t *pxTCB;
    UBaseType_t uxCurrentBasePriority, uxPriorityUsedOnEntry;
    BaseType_t xYieldRequired = pdFALSE;

    /* 确认新的优先级是有效的。 */
    configASSERT( ( uxNewPriority < configMAX_PRIORITIES ) );

    /* 确保新的优先级是有效的。 */
    if( uxNewPriority >= ( UBaseType_t ) configMAX_PRIORITIES )
    {
        uxNewPriority = ( UBaseType_t ) configMAX_PRIORITIES - ( UBaseType_t ) 1U;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }

    taskENTER_CRITICAL();
    {
        /* 如果传入的是空句柄，则更改的是调用任务本身的优先级。 */
        pxTCB = prvGetTCBFromHandle( xTask );

        traceTASK_PRIORITY_SET( pxTCB, uxNewPriority );

        #if ( configUSE_MUTEXES == 1 )
        {
            uxCurrentBasePriority = pxTCB->uxBasePriority;
        }
        #else
        {
            uxCurrentBasePriority = pxTCB->uxPriority;
        }
        #endif

        if( uxCurrentBasePriority != uxNewPriority )
        {
            /* 优先级的更改可能会使比当前任务更高优先级的任务准备好执行。 */
            if( uxNewPriority > uxCurrentBasePriority )
            {
                if( pxTCB != pxCurrentTCB )
                {
                    /* 提升非当前运行任务的优先级。如果提升后的优先级高于当前任务的优先级，则可能需要切换。 */
                    if( uxNewPriority >= pxCurrentTCB->uxPriority )
                    {
                        xYieldRequired = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    /* 提升当前任务的优先级，但由于当前任务已经是最高可运行的优先级，因此不需要切换。 */
                }
            }
            else if( pxTCB == pxCurrentTCB )
            {
                /* 降低当前运行任务的优先级意味着可能存在另一个更高优先级的任务准备执行。 */
                xYieldRequired = pdTRUE;
            }
            else
            {
                /* 降低其他任务的优先级不需要切换，因为当前运行的任务的优先级一定高于被修改任务的新优先级。 */
            }

            /* 记录进入前任务可能被引用的就绪列表，以便 taskRESET_READY_PRIORITY 宏正常工作。 */
            uxPriorityUsedOnEntry = pxTCB->uxPriority;

            #if ( configUSE_MUTEXES == 1 )
            {
                /* 只有在任务未使用继承优先级的情况下才更改其使用的优先级。 */
                if( pxTCB->uxBasePriority == pxTCB->uxPriority )
                {
                    pxTCB->uxPriority = uxNewPriority;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                /* 无论如何都要设置基优先级。 */
                pxTCB->uxBasePriority = uxNewPriority;
            }
            #else
            {
                pxTCB->uxPriority = uxNewPriority;
            }
            #endif

            /* 只有在事件列表项值未被用于其他用途的情况下才重置其值。 */
            if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
            {
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxNewPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            /* 如果任务在阻塞或挂起列表中，只需要更改其优先级变量即可。然而，如果任务在就绪列表中，则需要先移除再添加到新的就绪列表。 */
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                /* 任务当前在其就绪列表中 - 先移除再添加到新的就绪列表。即使调度器暂停，由于我们在临界区内也可以这样做。 */
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    /* 已知任务在其就绪列表中，因此无需再次检查，可以直接调用端口级别重置宏。 */
                    portRESET_READY_PRIORITY( uxPriorityUsedOnEntry, uxTopReadyPriority );
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            if( xYieldRequired != pdFALSE )
            {
                taskYIELD_IF_USING_PREEMPTION();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            /* 当端口优化的任务选择未使用时，移除未使用的变量警告。 */
            ( void ) uxPriorityUsedOnEntry;
        }
    }
    taskEXIT_CRITICAL();
}

#endif /* INCLUDE_vTaskPrioritySet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

	void vTaskSuspend( TaskHandle_t xTaskToSuspend )
{
    TCB_t *pxTCB;

    // 进入临界区，防止中断发生，确保操作的原子性
    taskENTER_CRITICAL();
    {
        // 从任务句柄获取对应的TCB指针
        pxTCB = prvGetTCBFromHandle( xTaskToSuspend );

        // 记录任务挂起的日志信息
        traceTASK_SUSPEND( pxTCB );

        // 从当前任务的就绪列表中移除任务状态项
        if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
        {
            // 重置任务的就绪优先级
            taskRESET_READY_PRIORITY( pxTCB->uxPriority );
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        // 从事件列表中移除任务事件项
        if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
        {
            ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
        }
        else
        {
            // 覆盖测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        // 将任务状态项插入到挂起任务列表的末尾
        vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    // 如果调度器正在运行
    if( xSchedulerRunning != pdFALSE )
    {
        // 再次进入临界区
        taskENTER_CRITICAL();
        {
            // 重置下一个任务解阻塞时间
            prvResetNextTaskUnblockTime();
        }
        // 退出临界区
        taskEXIT_CRITICAL();
    }
    else
    {
        // 覆盖测试标记
        mtCOVERAGE_TEST_MARKER();
    }

    // 如果挂起的任务是当前正在执行的任务
    if( pxTCB == pxCurrentTCB )
    {
        // 如果调度器正在运行
        if( xSchedulerRunning != pdFALSE )
        {
            // 断言检查，确保没有其他任务被挂起
            configASSERT( uxSchedulerSuspended == 0 );
            // 触发调度，让出CPU给其他任务
            portYIELD_WITHIN_API();
        }
        else
        {
            // 如果所有任务都被挂起，则清除当前任务指针
            if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == uxCurrentNumberOfTasks )
            {
                pxCurrentTCB = NULL;
            }
            else
            {
                // 切换任务上下文
                vTaskSwitchContext();
            }
        }
    }
    else
    {
        // 覆盖测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

/**
 * @brief 检查任务是否处于挂起状态。
 *
 * 此函数用于检查给定任务是否已经被挂起。
 *
 * @param xTask 要检查的任务句柄。
 *
 * @return BaseType_t 如果任务被挂起则返回 pdTRUE，否则返回 pdFALSE。
 */
static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )
{
    BaseType_t xReturn = pdFALSE;
    const TCB_t * const pxTCB = ( TCB_t * ) xTask;

    /* 需要在临界区中执行，因为会访问 xPendingReadyList。 */

    /* 检查任务句柄是否有效，并且不应检查当前任务是否被挂起。 */
    configASSERT( xTask );

    /* 要恢复的任务是否在挂起列表中？ */
    if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
    {
        /* 该任务是否已经在中断服务程序中被恢复了？ */
        if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) == pdFALSE )
        {
            /* 任务是在挂起状态还是因为没有超时而被阻塞？ */
            if( listIS_CONTAINED_WITHIN( NULL, &( pxTCB->xEventListItem ) ) != pdFALSE )
            {
                xReturn = pdTRUE;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER(); /* 代码覆盖率标记。 */
    }

    return xReturn;
} /*lint !e818 xTask 不能是指向 const 的指针，因为它是一个类型定义。*/
#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )
//任务恢复函数，将任务从挂起列表中移除，添加进就绪列表
void vTaskResume( TaskHandle_t xTaskToResume )
{
    TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;

    // 断言检查，确保传递的句柄是非空的
    configASSERT( xTaskToResume );

    // 如果任务句柄有效并且不是当前正在执行的任务
    if( ( pxTCB != NULL ) && ( pxTCB != pxCurrentTCB ) )
    {
        taskENTER_CRITICAL();
        {
            // 检查任务是否处于挂起状态
            if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
            {
                // 记录任务恢复的日志信息
                traceTASK_RESUME( pxTCB );

                // 从挂起任务列表中移除该任务
                ( void ) uxListRemove(  &( pxTCB->xStateListItem ) );

                // 将任务添加到就绪队列中
                prvAddTaskToReadyList( pxTCB );

                // 如果恢复的任务优先级等于或高于当前执行的任务优先级，则触发调度
                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                {
                    taskYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // 如果任务不是挂起状态，只是做标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        taskEXIT_CRITICAL();
    }
    else
    {
        // 如果任务句柄无效或就是当前任务，则做标记
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* INCLUDE_vTaskSuspend */

/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )

BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume )
{
    BaseType_t xYieldRequired = pdFALSE;
    TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;
    UBaseType_t uxSavedInterruptStatus;

    // 断言检查，确保传递的任务句柄是非空的
    configASSERT( xTaskToResume );

    // 确保中断优先级是有效的
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 保存当前中断状态，并设置中断屏蔽
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        // 检查任务是否处于挂起状态
        if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
        {
            // 记录任务恢复的日志信息
            traceTASK_RESUME_FROM_ISR( pxTCB );

            // 如果调度器未被挂起
            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                // 检查恢复的任务优先级是否等于或高于当前执行的任务优先级
                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                {
                    // 如果恢复的任务优先级更高，则需要进行调度
                    xYieldRequired = pdTRUE;
                }
                else
                {
                    // 覆盖测试标记
                    mtCOVERAGE_TEST_MARKER();
                }

                // 从挂起任务列表中移除该任务
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                // 将任务添加到就绪队列中
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // 如果调度器被挂起，则将任务添加到待处理的就绪列表中
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }
        }
        else
        {
            // 如果任务不是挂起状态，则做标记
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // 恢复之前的中断状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    // 返回是否需要调度
    return xYieldRequired;
}

#endif /* ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) ) */
/*-----------------------------------------------------------*/

/**
 * @brief 启动任务调度器。
 *
 * 此函数用于启动FreeRTOS内核的调度器，包括创建空闲任务和定时器任务（如果启用了定时器），
 * 并开始调度第一个任务。
 */
void vTaskStartScheduler( void )
{
    BaseType_t xReturn;

    /* 在最低优先级下添加空闲任务。 */
    #if( configSUPPORT_STATIC_ALLOCATION == 1 )
    {
        StaticTask_t *pxIdleTaskTCBBuffer = NULL;
        StackType_t *pxIdleTaskStackBuffer = NULL;
        uint32_t ulIdleTaskStackSize;

        /* 使用用户提供的RAM来创建空闲任务 - 获取RAM地址然后创建空闲任务。 */
        vApplicationGetIdleTaskMemory( &pxIdleTaskTCBBuffer, &pxIdleTaskStackBuffer, &ulIdleTaskStackSize );
        xIdleTaskHandle = xTaskCreateStatic(	prvIdleTask,
                                             "IDLE",
                                             ulIdleTaskStackSize,
                                             ( void * ) NULL,
                                             ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ),
                                             pxIdleTaskStackBuffer,
                                             pxIdleTaskTCBBuffer ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */

        if( xIdleTaskHandle != NULL )
        {
            xReturn = pdPASS;
        }
        else
        {
            xReturn = pdFAIL;
        }
    }
    #else
    {
        /* 使用动态分配的RAM来创建空闲任务。 */
        xReturn = xTaskCreate(	prvIdleTask,
                               "IDLE",
                               configMINIMAL_STACK_SIZE,
                               ( void * ) NULL,
                               ( tskIDLE_PRIORITY | portPRIVILEGE_BIT ),
                               &xIdleTaskHandle ); /*lint !e961 MISRA exception, justified as it is not a redundant explicit cast to all supported compilers. */
    }
    #endif /* configSUPPORT_STATIC_ALLOCATION */

    #if ( configUSE_TIMERS == 1 )
    {
        if( xReturn == pdPASS )
        {
            xReturn = xTimerCreateTimerTask();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* configUSE_TIMERS */

    if( xReturn == pdPASS )
    {
        /* 关闭中断，以确保在调用 xPortStartScheduler() 之前或期间不发生滴答。
        创建的任务堆栈包含一个中断开启的状态字，因此当第一个任务开始运行时中断会自动重新启用。 */
        portDISABLE_INTERRUPTS();

        #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* 将 Newlib 的 _impure_ptr 变量切换到指向第一个运行任务的 _reent 结构。 */
            _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
        }
        #endif /* configUSE_NEWLIB_REENTRANT */

        xNextTaskUnblockTime = portMAX_DELAY;
        xSchedulerRunning = pdTRUE;
        xTickCount = ( TickType_t ) 0U;

        /* 如果定义了 configGENERATE_RUN_TIME_STATS，则必须定义以下宏来配置生成运行时计数器时间基准的定时器/计数器。 */
        portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

        /* 设置定时器滴答是硬件相关的，因此在可移植接口中实现。 */
        if( xPortStartScheduler() != pdFALSE )
        {
            /* 不应该到达这里，因为如果调度器正在运行，函数将不会返回。 */
        }
        else
        {
            /* 只有当某个任务调用 xTaskEndScheduler() 时才会到达这里。 */
        }
    }
    else
    {
        /* 如果内核未能启动，这是因为没有足够的FreeRTOS堆来创建空闲任务或定时器任务。 */
        configASSERT( xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY );
    }

    /* 防止编译器警告，如果 INCLUDE_xTaskGetIdleTaskHandle 被设置为 0，则意味着 xIdleTaskHandle 在其他地方没有被使用。 */
    ( void ) xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

/**
 * @brief 结束任务调度器。
 *
 * 此函数用于停止FreeRTOS内核的调度器，并恢复原始的中断服务程序（如果有必要）。
 */
void vTaskEndScheduler( void )
{
    /* 停止调度器中断，并调用可移植的调度器结束例程以便在必要时可以恢复原始的中断服务程序。
    端口层必须确保中断使能位处于正确的状态。 */
    portDISABLE_INTERRUPTS(); /* 关闭中断，防止在下面的操作中被中断。 */
    xSchedulerRunning = pdFALSE; /* 设置调度器状态为不运行。 */
    vPortEndScheduler(); /* 调用端口特定的结束调度器函数。 */
}
/*----------------------------------------------------------*/

/**
 * @brief 暂停所有任务调度。
 *
 * 此函数用于暂停所有任务的调度，直到对应的 `xTaskResumeAll` 被调用。
 * 在此期间，不允许任务之间切换，但是可以安全地修改任务列表或其他调度相关的数据结构。
 */
void vTaskSuspendAll( void )
{
    /* 增加调度器暂停计数器，这会阻止任何进一步的任务切换。
    当调度器被暂停时，任何尝试导致上下文切换的操作都会被延迟，直到调度器恢复。 */
    ++uxSchedulerSuspended;
}
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

/**
 * @brief 获取预期的空闲时间。
 *
 * 此函数用于计算当前任务（假设是空闲任务）预期的空闲时间。
 * 如果有更高优先级的任务准备好运行或者当前任务不是空闲任务，则预期的空闲时间为0。
 *
 * @return TickType_t 返回预期的空闲时间（滴答数）。
 */
static TickType_t prvGetExpectedIdleTime( void )
{
    TickType_t xReturn;
    UBaseType_t uxHigherPriorityReadyTasks = pdFALSE;

    /* uxHigherPriorityReadyTasks 处理 configUSE_PREEMPTION 为 0 的情况，
    即便空闲任务正在运行，也可能有高于空闲优先级的任务处于就绪状态。 */
    #if( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )
    {
        if( uxTopReadyPriority > tskIDLE_PRIORITY )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #else
    {
        const UBaseType_t uxLeastSignificantBit = ( UBaseType_t ) 0x01;

        /* 当使用端口优化的任务选择时，uxTopReadyPriority 变量作为一个位图。
        如果除了最低有效位外还有其他位被设置，则说明存在高于空闲优先级且处于就绪状态的任务。
        这里处理了使用合作式调度的情况。 */
        if( uxTopReadyPriority > uxLeastSignificantBit )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #endif

    if( pxCurrentTCB->uxPriority > tskIDLE_PRIORITY )
    {
        /* 如果当前任务的优先级高于空闲优先级，则预期的空闲时间为0。 */
        xReturn = 0;
    }
    else if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > 1 )
    {
        /* 如果有其他空闲优先级的任务处于就绪状态，则下一个滴答中断必须被处理。
        这适用于使用时间片调度的情况。 */
        xReturn = 0;
    }
    else if( uxHigherPriorityReadyTasks != pdFALSE )
    {
        /* 存在高于空闲优先级且处于就绪状态的任务。这种情况只能发生在
        configUSE_PREEMPTION 为 0 的情况下。 */
        xReturn = 0;
    }
    else
    {
        /* 如果没有更高优先级的任务准备好运行，那么预期的空闲时间是从现在到下一个任务解阻塞的时间。 */
        xReturn = xNextTaskUnblockTime - xTickCount;
    }

    return xReturn;
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

/**
 * @brief 恢复所有任务调度。
 *
 * 此函数用于恢复所有任务的调度，它与 `vTaskSuspendAll` 函数配对使用。
 * 当所有任务的调度被暂停后，调用此函数可以恢复调度，并处理任何可能发生的上下文切换。
 *
 * @return BaseType_t 如果在恢复调度时已经发生了上下文切换，则返回 pdTRUE，否则返回 pdFALSE。
 */
BaseType_t xTaskResumeAll( void )
{
    TCB_t *pxTCB = NULL;
    BaseType_t xAlreadyYielded = pdFALSE;

    /* 如果 uxSchedulerSuspended 为零，则此函数不匹配之前的 vTaskSuspendAll() 调用。 */
    configASSERT( uxSchedulerSuspended );

    /* 中断可能在调度器被暂停时导致一个任务从事件列表中移除。如果这是真的，那么被移除的任务将被添加到 xPendingReadyList 列表中。
    一旦调度器恢复，就可以安全地将所有待处理的就绪任务从该列表移动到适当的就绪列表。 */
    taskENTER_CRITICAL();
    {
        --uxSchedulerSuspended;

        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
            {
                /* 将任何已准备好的任务从待处理列表移动到适当的就绪列表。 */
                while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
                {
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                    prvAddTaskToReadyList( pxTCB );

                    /* 如果移动的任务优先级高于当前任务，则必须执行上下文切换。 */
                    if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                    {
                        xYieldPending = pdTRUE;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                if( pxTCB != NULL )
                {
                    /* 当调度器被暂停时，如果一个任务被解除阻塞，这可能阻止下次解除阻塞时间被重新计算，
                    因此现在重新计算它。这对于低功耗无滴答实现尤其重要，它可以防止不必要的退出低功耗状态。 */
                    prvResetNextTaskUnblockTime();
                }

                /* 如果在调度器被暂停期间有任何滴答发生，则现在应进行处理。这确保了滴答计数不会滑落，
                并确保任何延迟的任务在正确的时间被恢复。 */
                {
                    UBaseType_t uxPendedCounts = uxPendedTicks; /* 非易失性副本。 */

                    if( uxPendedCounts > ( UBaseType_t ) 0U )
                    {
                        do
                        {
                            if( xTaskIncrementTick() != pdFALSE )
                            {
                                xYieldPending = pdTRUE;
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER();
                            }
                            --uxPendedCounts;
                        } while( uxPendedCounts > ( UBaseType_t ) 0U );

                        uxPendedTicks = 0;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                if( xYieldPending != pdFALSE )
                {
                    #if( configUSE_PREEMPTION != 0 )
                    {
                        xAlreadyYielded = pdTRUE;
                    }
                    #endif
                    taskYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();

    return xAlreadyYielded;
}
/*-----------------------------------------------------------*/

/**
 * @brief 获取当前的滴答计数值。
 *
 * 此函数用于获取当前系统的滴答计数值。如果系统运行在16位处理器上，则需要临界区保护以避免读取到不一致的数据。
 *
 * @return TickType_t 返回当前的滴答计数值。
 */
TickType_t xTaskGetTickCount( void )
{
    TickType_t xTicks;

    /* 如果运行在16位处理器上，则需要临界区保护以确保读取滴答计数时的一致性。 */
    portTICK_TYPE_ENTER_CRITICAL();
    {
        xTicks = xTickCount;
    }
    portTICK_TYPE_EXIT_CRITICAL();

    return xTicks;
}
/*-----------------------------------------------------------*/

/**
 * @brief 从中断服务程序中获取当前的滴答计数值。
 *
 * 此函数用于从中断服务程序中获取当前系统的滴答计数值。为了确保读取的滴答计数值是一致的，需要在中断上下文中禁用中断。
 *
 * @return TickType_t 返回当前的滴答计数值。
 */
TickType_t xTaskGetTickCountFromISR( void )
{
    TickType_t xReturn;
    UBaseType_t uxSavedInterruptStatus;

    /* 支持中断嵌套的RTOS端口具有最大系统调用（或最大API调用）中断优先级的概念。
    高于最大系统调用优先级的中断始终保持启用状态，即使RTOS内核处于临界区时也是如此，但不能调用任何FreeRTOS API函数。
    如果在FreeRTOSConfig.h中定义了configASSERT()，则portASSERT_IF_INTERRUPT_PRIORITY_INVALID()将在高优先级中断中调用FreeRTOS API函数时导致断言失败。
    只有以FromISR结尾的FreeRTOS函数才能从具有等于或低于（逻辑上）最大系统调用优先级的中断中调用。
    FreeRTOS维护了一个单独的中断安全API，以确保中断进入尽可能快和简单。
    更多信息（尽管是Cortex-M特定的）可以在以下链接找到：http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* 保存当前中断状态，并设置中断屏蔽以进入临界区。 */
    uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
    {
        xReturn = xTickCount;
    }
    /* 恢复中断状态，退出临界区。 */
    portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 获取当前系统中的任务数量。
 *
 * 此函数用于返回当前系统中创建的任务总数。
 *
 * @return UBaseType_t 当前系统中的任务数量。
 */
UBaseType_t uxTaskGetNumberOfTasks( void )
{
    return uxCurrentNumberOfTasks;
}

/**
 * @brief 获取指定任务的名字。
 *
 * 此函数用于获取指定任务的名字。任务名字是一个字符串，用于标识任务。
 *
 * @param xTaskToQuery 要查询的任务句柄。
 * @return char * 返回指向任务名字的指针。
 */
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) 
{
    TCB_t *pxTCB;

    /* 从任务句柄中获取TCB指针。 */
    pxTCB = prvGetTCBFromHandle( xTaskToQuery );
    
    /* 断言确保TCB指针有效。 */
    configASSERT( pxTCB );
    
    /* 返回任务的名字。 */
    return &( pxTCB->pcTaskName[ 0 ] );
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/**
 * @brief 在单个列表中按名称查找任务。
 *
 * 此函数用于在一个任务列表中查找具有指定名称的任务。
 *
 * @param pxList 要搜索的任务列表。
 * @param pcNameToQuery 要查询的任务名称。
 * @return TCB_t * 如果找到匹配的任务，则返回指向其TCB的指针；否则返回NULL。
 */
static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] )
{
    TCB_t *pxNextTCB, *pxFirstTCB, *pxReturn = NULL;
    UBaseType_t x;
    char cNextChar;

    /* 此函数是在调度器被暂停的情况下被调用的。 */

    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        /* 获取列表中第一个元素的拥有者（TCB）。 */
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        /* 遍历整个列表。 */
        do
        {
            /* 获取列表中下一个元素的拥有者（TCB）。 */
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

            /* 检查名称中的每个字符以寻找匹配或不匹配的情况。 */
            for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
            {
                cNextChar = pxNextTCB->pcTaskName[ x ];

                if( cNextChar != pcNameToQuery[ x ] )
                {
                    /* 字符不匹配。 */
                    break;
                }
                else if( cNextChar == 0x00 )
                {
                    /* 字符串终止，说明找到了匹配的任务。 */
                    pxReturn = pxNextTCB;
                    break;
                }
                else
                {
                    /* 字符匹配，但还没有达到字符串末尾。 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }

            /* 如果找到了匹配的任务，则跳出循环。 */
            if( pxReturn != NULL )
            {
                break;
            }

        } while( pxNextTCB != pxFirstTCB );
    }
    else
    {
        /* 列表为空。 */
        mtCOVERAGE_TEST_MARKER();
    }

    return pxReturn;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/**
 * @brief 根据任务名称获取任务句柄。
 *
 * 此函数用于根据给定的任务名称，在各个任务列表中查找对应的任务，并返回其句柄。
 *
 * @param pcNameToQuery 要查询的任务名称。
 * @return TaskHandle_t 如果找到任务，则返回指向其TCB的句柄；否则返回NULL。
 */
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery )
{
    UBaseType_t uxQueue = configMAX_PRIORITIES;
    TCB_t *pxTCB;

    /* 任务名称将被截断到 configMAX_TASK_NAME_LEN - 1 字节。 */
    configASSERT( strlen( pcNameToQuery ) < configMAX_TASK_NAME_LEN );

    /* 暂停所有任务调度。 */
    vTaskSuspendAll();
    {
        /* 搜索就绪列表。 */
        do
        {
            uxQueue--;
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) &( pxReadyTasksLists[ uxQueue ] ), pcNameToQuery );

            if( pxTCB != NULL )
            {
                /* 找到了句柄。 */
                break;
            }

        } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY );

        /* 搜索延迟列表。 */
        if( pxTCB == NULL )
        {
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxDelayedTaskList, pcNameToQuery );
        }

        if( pxTCB == NULL )
        {
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) pxOverflowDelayedTaskList, pcNameToQuery );
        }

        #if ( INCLUDE_vTaskSuspend == 1 )
        {
            if( pxTCB == NULL )
            {
                /* 搜索挂起列表。 */
                pxTCB = prvSearchForNameWithinSingleList( &xSuspendedTaskList, pcNameToQuery );
            }
        }
        #endif

        #if( INCLUDE_vTaskDelete == 1 )
        {
            if( pxTCB == NULL )
            {
                /* 搜索删除列表。 */
                pxTCB = prvSearchForNameWithinSingleList( &xTasksWaitingTermination, pcNameToQuery );
            }
        }
        #endif
    }
    /* 恢复所有任务调度。 */
    ( void ) xTaskResumeAll();

    return ( TaskHandle_t ) pxTCB;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/**
 * @brief 获取系统的任务状态。
 *
 * 此函数用于获取系统中所有任务的状态，并将结果存储在一个用户提供的数组中。此外，还可以提供系统总运行时间。
 *
 * @param pxTaskStatusArray 用户提供的数组，用于存储每个任务的状态。
 * @param uxArraySize 提供的数组大小。
 * @param pulTotalRunTime 如果不为NULL，则返回系统总运行时间。
 * @return UBaseType_t 返回实际填写的状态信息条目的数量。
 */
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime )
{
    UBaseType_t uxTask = 0, uxQueue = configMAX_PRIORITIES;

    /* 暂停所有任务调度。 */
    vTaskSuspendAll();
    {
        /* 提供的数组是否足够存放系统中的每个任务？ */
        if( uxArraySize >= uxCurrentNumberOfTasks )
        {
            /* 填充包含每个就绪状态任务信息的 TaskStatus_t 结构。 */
            do
            {
                uxQueue--;
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &( pxReadyTasksLists[ uxQueue ] ), eReady );

            } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY );

            /* 填充包含每个阻塞状态任务信息的 TaskStatus_t 结构。 */
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxDelayedTaskList, eBlocked );
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxOverflowDelayedTaskList, eBlocked );

            #if( INCLUDE_vTaskDelete == 1 )
            {
                /* 填充包含每个已被删除但尚未清理的任务信息的 TaskStatus_t 结构。 */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xTasksWaitingTermination, eDeleted );
            }
            #endif

            #if ( INCLUDE_vTaskSuspend == 1 )
            {
                /* 填充包含每个挂起状态任务信息的 TaskStatus_t 结构。 */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xSuspendedTaskList, eSuspended );
            }
            #endif

            #if ( configGENERATE_RUN_TIME_STATS == 1 )
            {
                if( pulTotalRunTime != NULL )
                {
                    #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                        portALT_GET_RUN_TIME_COUNTER_VALUE( ( *pulTotalRunTime ) );
                    #else
                        *pulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
                    #endif
                }
            }
            #else
            {
                if( pulTotalRunTime != NULL )
                {
                    *pulTotalRunTime = 0;
                }
            }
            #endif
        }
        else
        {
            /* 数组太小，无法容纳所有任务的信息。 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* 恢复所有任务调度。 */
    ( void ) xTaskResumeAll();

    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

/**
 * @brief 获取空闲任务的句柄。
 *
 * 此函数用于返回空闲任务的句柄。如果在调度器启动之前调用此函数，则 `xIdleTaskHandle` 将为 NULL。
 *
 * @return TaskHandle_t 返回空闲任务的句柄。
 */
TaskHandle_t xTaskGetIdleTaskHandle( void )
{
    /* 如果在调度器启动之前调用此函数，则 xIdleTaskHandle 将为 NULL。 */
    configASSERT( ( xIdleTaskHandle != NULL ) );
    return xIdleTaskHandle;
}

#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

/**
 * @brief 跳过指定数量的滴答。
 *
 * 此函数用于在滴答被抑制一段时间之后修正滴答计数值。注意，这并不会为每一个跳过的滴答调用滴答钩子函数。
 *
 * @param xTicksToJump 要跳过的滴答数。
 */
#if ( configUSE_TICKLESS_IDLE != 0 )

void vTaskStepTick( const TickType_t xTicksToJump )
{
    /* 在滴答被抑制一段时间后修正滴答计数值。注意，这并不会为每一个跳过的滴答调用滴答钩子函数。
    确保修正后的滴答计数值不会超过下一个任务解除阻塞的时间。 */
    configASSERT( ( xTickCount + xTicksToJump ) <= xNextTaskUnblockTime );
    xTickCount += xTicksToJump;
    traceINCREASE_TICK_COUNT( xTicksToJump );
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )

/**
 * @brief 终止任务的延迟。
 *
 * 此函数用于终止一个被延迟的任务的等待状态，并将其重新放置到就绪队列中。
 *
 * @param xTask 要终止延迟的任务句柄。
 * @return BaseType_t 如果成功终止延迟，则返回 pdTRUE；否则返回 pdFALSE。
 *                   注意：在这个函数中，返回值始终为 pdFALSE，因为任务的状态仅在任务处于 eBlocked 状态时才改变。
 */
BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
{
    TCB_t *pxTCB = ( TCB_t * ) xTask;
    BaseType_t xReturn = pdFALSE;

    configASSERT( pxTCB );

    vTaskSuspendAll();
    {
        /* 只有当任务实际上处于 Blocked 状态时，才能提前移除任务。 */
        if( eTaskGetState( xTask ) == eBlocked )
        {
            /* 移除任务在 Blocked 列表中的引用。由于调度器被暂停，中断不会影响 xStateListItem。 */
            ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

            /* 任务是否也在等待事件？如果是，也从事件列表中移除它。即使调度器被暂停，中断也可能影响事件列表项，因此使用临界区。 */
            taskENTER_CRITICAL();
            {
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    pxTCB->ucDelayAborted = pdTRUE;
                }
                else
                {
                    /* 未在事件列表中。 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            taskEXIT_CRITICAL();

            /* 将解除阻塞的任务放入适当的就绪列表中。 */
            prvAddTaskToReadyList( pxTCB );

            /* 解除阻塞的任务不会立即引起上下文切换，如果禁用了抢占模式。 */
            #if (  configUSE_PREEMPTION == 1 )
            {
                /* 抢占模式开启，但只有当解除阻塞的任务优先级等于或高于当前正在执行的任务时，才会执行上下文切换。 */
                if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                {
                    /* 挂起要执行的上下文切换，当调度器被恢复时执行。 */
                    xYieldPending = pdTRUE;
                }
                else
                {
                    /* 优先级不够高，不需要上下文切换。 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configUSE_PREEMPTION */
        }
        else
        {
            /* 任务不在 Blocked 状态。 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    xTaskResumeAll();

    return xReturn;
}
#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/

/**
 * @brief 增加任务滴答。
 *
 * 此函数由可移植层在每次滴答中断发生时调用。它会增加滴答计数，并检查新的滴答值是否会解除任何任务的阻塞状态。
 *
 * @return BaseType_t 如果需要进行上下文切换，则返回 pdTRUE；否则返回 pdFALSE。
 */
BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;
    TickType_t xItemValue;
    BaseType_t xSwitchRequired = pdFALSE;

    /* 被可移植层在每次滴答中断发生时调用。增加滴答计数，并检查新的滴答值是否会解除任何任务的阻塞状态。 */
    traceTASK_INCREMENT_TICK( xTickCount );
    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* 小优化：在此块中滴答计数不会改变。 */
        const TickType_t xConstTickCount = xTickCount + 1;

        /* 增加RTOS滴答，并在滴答计数回绕至0时切换延迟和溢出延迟列表。 */
        xTickCount = xConstTickCount;

        if( xConstTickCount == ( TickType_t ) 0U )
        {
            taskSWITCH_DELAYED_LISTS();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        /* 检查这次滴答是否使某个超时到期。任务按其唤醒时间顺序存储在队列中，意味着一旦发现一个任务的阻塞时间未到期，就没有必要继续检查队列中的其他任务。 */
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            for( ;; )
            {
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
                {
                    /* 延迟列表为空。将 xNextTaskUnblockTime 设置为最大值，使得下次通过时不太可能通过测试条件。 */
                    xNextTaskUnblockTime = portMAX_DELAY;
                    break;
                }
                else
                {
                    /* 延迟列表不为空，获取延迟列表头部的项值。这是位于延迟列表头部的任务必须从阻塞状态移除的时间。 */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    if( xConstTickCount < xItemValue )
                    {
                        /* 还不是移除此项的时候，但该项值是位于阻塞列表头部的任务必须移除的时间，因此在 xNextTaskUnblockTime 中记录该项值。 */
                        xNextTaskUnblockTime = xItemValue;
                        break;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 是时候将该项从阻塞状态移除了。 */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    /* 任务是否也在等待事件？如果是，则从事件列表中移除它。 */
                    if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                    {
                        ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* 将解除阻塞的任务放入适当的就绪列表中。 */
                    prvAddTaskToReadyList( pxTCB );

                    /* 如果抢占模式开启，并且解除阻塞的任务优先级等于或高于当前正在执行的任务，则需要上下文切换。 */
                    #if (  configUSE_PREEMPTION == 1 )
                    {
                        if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                        {
                            xSwitchRequired = pdTRUE;
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    #endif /* configUSE_PREEMPTION */
                }
            }
        }

        /* 如果抢占模式开启，并且应用作者没有显式关闭时间片，则相同优先级的任务将共享处理时间。 */
        #if ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) )
        {
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ pxCurrentTCB->uxPriority ] ) ) > ( UBaseType_t ) 1 )
            {
                xSwitchRequired = pdTRUE;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configUSE_TIME_SLICING == 1 ) ) */

        /* 即使调度器被锁定，滴答钩子也会定期调用。 */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            /* 在解开已挂起的滴答计数时防止滴答钩子被调用。 */
            if( uxPendedTicks == ( UBaseType_t ) 0U )
            {
                vApplicationTickHook();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* configUSE_TICK_HOOK */
    }
    else
    {
        ++uxPendedTicks;

        /* 即使调度器被锁定，滴答钩子也会定期调用。 */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            vApplicationTickHook();
        }
        #endif
    }

    /* 如果挂起了上下文切换，则需要进行上下文切换。 */
    #if ( configUSE_PREEMPTION == 1 )
    {
        if( xYieldPending != pdFALSE )
        {
            xSwitchRequired = pdTRUE;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* configUSE_PREEMPTION */

    return xSwitchRequired;
}
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/**
 * @brief 设置任务的应用程序标签（钩子函数）。
 *
 * 此函数用于设置任务的钩子函数，该函数可以在特定的情况下被调用。
 * 如果提供了任务句柄，则设置该任务的钩子函数；如果没有提供，则设置当前任务的钩子函数。
 *
 * @param xTask 要设置钩子函数的任务句柄。如果为 NULL，则设置当前任务的钩子函数。
 * @param pxHookFunction 要设置的钩子函数指针。
 */
void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
{
    TCB_t *xTCB;

    /* 如果 xTask 为 NULL，则设置当前任务的钩子函数。 */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* 在 TCB 中保存钩子函数。需要临界区保护，因为该值可以从中断访问。 */
    taskENTER_CRITICAL();
    {
        xTCB->pxTaskTag = pxHookFunction;
    }
    taskEXIT_CRITICAL();
}
#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/**
 * @brief 获取任务的应用程序标签（钩子函数）。
 *
 * 此函数用于获取指定任务的钩子函数。如果提供了任务句柄，则获取该任务的钩子函数；
 * 如果没有提供，则获取当前任务的钩子函数。
 *
 * @param xTask 要获取钩子函数的任务句柄。如果为 NULL，则获取当前任务的钩子函数。
 * @return TaskHookFunction_t 返回指定任务的钩子函数指针。
 */
TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
{
    TCB_t *xTCB;
    TaskHookFunction_t xReturn;

    /* 如果 xTask 为 NULL，则我们是获取当前任务的钩子函数。 */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* 从中断中也可以访问该值，因此需要临界区保护。 */
    taskENTER_CRITICAL();
    {
        xReturn = xTCB->pxTaskTag;
    }
    taskEXIT_CRITICAL();

    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

#if ( configUSE_APPLICATION_TASK_TAG == 1 )

/**
 * @brief 调用任务的应用程序钩子函数。
 *
 * 此函数用于调用指定任务的钩子函数，并传递参数给该函数。如果提供了任务句柄，
 * 则调用该任务的钩子函数；如果没有提供，则调用当前任务的钩子函数。
 *
 * @param xTask 要调用钩子函数的任务句柄。如果为 NULL，则调用当前任务的钩子函数。
 * @param pvParameter 传递给钩子函数的参数。
 * @return BaseType_t 如果成功调用钩子函数，则返回 pdPASS；否则返回 pdFAIL。
 */
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter )
{
    TCB_t *xTCB;
    BaseType_t xReturn;

    /* 如果 xTask 为 NULL，则我们是调用当前任务的钩子函数。 */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* 如果钩子函数存在，则调用它。 */
    if( xTCB->pxTaskTag != NULL )
    {
        xReturn = xTCB->pxTaskTag( pvParameter );
    }
    else
    {
        /* 钩子函数不存在。 */
        xReturn = pdFAIL;
    }

    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

/**
 * @brief 执行上下文切换。
 *
 * 此函数用于在RTOS中执行上下文切换，选择一个新的最高优先级任务来运行。
 * 如果调度器被暂停，则标记上下文切换为待执行。
 */
void vTaskSwitchContext( void )
{
    if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
    {
        /* 如果调度器当前被暂停，则不允许上下文切换。 */
        xYieldPending = pdTRUE;
    }
    else
    {
        xYieldPending = pdFALSE;
        traceTASK_SWITCHED_OUT();

        #if ( configGENERATE_RUN_TIME_STATS == 1 )
        {
            #ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
                portALT_GET_RUN_TIME_COUNTER_VALUE( ulTotalRunTime );
            #else
                ulTotalRunTime = portGET_RUN_TIME_COUNTER_VALUE();
            #endif

            /* 计算当前任务的运行时间，并累加到任务的总运行时间中。
            从任务开始运行的时间 ulTaskSwitchedInTime 开始计算。
            注意这里没有溢出保护，所以计数值只在定时器溢出前有效。
            对负值的保护是为了避免可疑的运行时统计计数器实现。 */
            if( ulTotalRunTime > ulTaskSwitchedInTime )
            {
                pxCurrentTCB->ulRunTimeCounter += ( ulTotalRunTime - ulTaskSwitchedInTime );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
            ulTaskSwitchedInTime = ulTotalRunTime;
        }
        #endif /* configGENERATE_RUN_TIME_STATS */

        /* 如果配置了检查栈溢出，则检查当前任务的栈是否溢出。 */
        taskCHECK_FOR_STACK_OVERFLOW();

        /* 使用通用C语言或端口优化的汇编代码选择一个新任务来运行。 */
        taskSELECT_HIGHEST_PRIORITY_TASK();
        traceTASK_SWITCHED_IN();

        #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* 将Newlib的 _impure_ptr 指向当前任务专用的 _reent 结构。 */
            _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
        }
        #endif /* configUSE_NEWLIB_REENTRANT */
    }
}
/*-----------------------------------------------------------*/

/*
 * 函数名称：vTaskPlaceOnEventList
 * 功能描述：将当前任务的事件列表项插入到指定的事件列表中，并按优先级排序。
 *          同时，根据给定的等待时间将当前任务添加到延迟执行列表中。
 *
 * 参数说明：
 * @pxEventList: 指向事件列表的指针。该列表用于存放等待特定事件的任务。
 * @xTicksToWait: 任务等待的时间（以滴答数表示）。当事件发生或等待时间到达后，
 *                任务将从延迟执行列表中移除并恢复执行。
 */

void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )
{
    configASSERT( pxEventList ); // 断言 pxEventList 指针非空

    /* 此函数必须在以下情况下调用：
     * - 中断已被禁用
     * - 调度器已被暂停
     * - 当前访问的队列已被锁定
     */

    /* 将当前任务控制块 (TCB) 中的事件列表项插入到指定的事件列表中，
     * 并按优先级顺序排列，确保最高优先级的任务最先被唤醒。
     * 注意：队列在此期间被锁定以防止中断同时访问。*/
    vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* 将当前任务添加到延迟执行列表中，并设置等待的时间。
     * 参数 pdTRUE 表示允许任务因等待事件而被延迟执行。*/
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

/*
 * 函数名称：vTaskPlaceOnUnorderedEventList
 * 功能描述：将当前任务的事件列表项插入到指定的无序事件列表末尾，并设置等待时间。
 *          此函数用于处理事件组的实现。
 *
 * 参数说明：
 * @pxEventList: 指向事件列表的指针。该列表用于存放等待特定事件的任务。
 * @xItemValue: 要存储在事件列表项中的值。
 * @xTicksToWait: 任务等待的时间（以滴答数表示）。当事件发生或等待时间到达后，
 *                任务将从延迟执行列表中移除并恢复执行。
 */

void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )
{
    configASSERT( pxEventList ); // 断言 pxEventList 指针非空

    /* 此函数必须在调度器被暂停的情况下调用。它用于事件组的实现。*/
    configASSERT( uxSchedulerSuspended != 0 );

    /* 存储事件列表项的值。由于中断不会访问处于非阻塞状态的任务的事件列表项，
     * 因此在这里访问事件列表项是安全的。*/
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* 将当前任务控制块 (TCB) 中的事件列表项放置到指定事件列表的末尾。
     * 在这里访问事件列表是安全的，因为这是事件组实现的一部分——
     * 中断不会直接访问事件组（而是通过挂起的功能调用间接访问任务级别）。*/
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* 将当前任务添加到延迟执行列表中，并设置等待时间。
     * 参数 pdTRUE 表示允许任务因等待事件而被延迟执行。*/
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

#if( configUSE_TIMERS == 1 )

/*
 * 函数名称：vTaskPlaceOnEventListRestricted
 * 功能描述：将当前任务的事件列表项插入到指定的事件列表末尾，并根据指定的等待时间和是否无限期等待标志来设置延迟。
 *          此函数仅供内核代码使用，不属于公共API，并且要求在调度器被暂停的情况下调用。
 *
 * 参数说明：
 * @pxEventList: 指向事件列表的指针。该列表用于存放等待特定事件的任务。
 * @xTicksToWait: 任务等待的时间（以滴答数表示）。如果设置了无限期等待，则该值无效。
 * @xWaitIndefinitely: 如果为 pdTRUE，则任务将无限期等待；否则，使用 xTicksToWait 指定的等待时间。
 */

void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
    configASSERT( pxEventList ); // 断言 pxEventList 指针非空

    /* 此函数不应由应用程序代码调用，因此其名称中含有 'Restricted'。
     * 它不是公共API的一部分，而是专为内核代码设计的，并有特殊的调用要求——
     * 必须在调度器被暂停的情况下调用此函数。*/

    /* 将当前任务控制块 (TCB) 中的事件列表项插入到指定的事件列表末尾。
     * 在这种情况下，假定这是唯一一个等待该事件列表的任务，
     * 因此可以使用更快的 vListInsertEnd() 函数而不是 vListInsert。*/
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* 如果任务应该无限期等待，则设置等待时间为一个特殊的值，
     * 该值将在 prvAddCurrentTaskToDelayedList() 函数内部被识别为无限期延迟。*/
    if( xWaitIndefinitely != pdFALSE )
    {
        xTicksToWait = portMAX_DELAY;
    }

    /* 记录任务延迟直到指定的时间点。*/
    traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );

    /* 将当前任务添加到延迟执行列表中，并根据 xWaitIndefinitely 设置延迟标志。*/
    prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

/*
 * 函数名称：xTaskRemoveFromEventList
 * 功能描述：从指定的事件列表中移除最高优先级的任务，并将其从延迟执行列表中移除，
 *          然后将其添加到就绪执行列表中。如果调度器被暂停，则将任务保持待定状态，
 *          直到调度器恢复。
 *
 * 参数说明：
 * @pxEventList: 指向事件列表的指针。该列表用于存放等待特定事件的任务。
 *
 * 返回值：
 * 如果从事件列表中移除的任务优先级高于调用此函数的任务，则返回 pdTRUE；
 * 否则返回 pdFALSE。
 */

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
    TCB_t *pxUnblockedTCB;
    BaseType_t xReturn;

    /* 此函数必须在临界区中调用。也可以在中断服务例程内的临界区内调用。*/

    /* 事件列表按优先级排序，因此可以移除列表中的第一个任务，因为它肯定是最高优先级的。
       移除 TCB 从延迟执行列表，并将其添加到就绪执行列表。

       如果一个队列被锁定，则此函数永远不会被调用——而是修改队列上的锁计数。
       这意味着在此处可以保证对事件列表的独占访问。

       假设已经检查过 pxEventList 不为空。*/
    pxUnblockedTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* 如果调度器未被暂停，则将任务从延迟列表中移除，并添加到就绪列表中。*/
        ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
        prvAddTaskToReadyList( pxUnblockedTCB );
    }
    else
    {
        /* 如果调度器被暂停，则将任务保持待定状态，直到调度器恢复。*/
        vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
    }

    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* 如果从事件列表中移除的任务优先级高于调用此函数的任务，则返回 pdTRUE。
           这允许调用任务知道是否应该立即强制上下文切换。*/
        xReturn = pdTRUE;

        /* 标记即将进行上下文切换，以防用户没有使用 "xHigherPriorityTaskWoken"
           参数来调用中断安全的 FreeRTOS 函数。*/
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    #if( configUSE_TICKLESS_IDLE != 0 )
    {
        /* 如果任务被阻塞在一个内核对象上，则 xNextTaskUnblockTime 可能设置为
           阻塞任务的超时时间。如果任务因其他原因解除阻塞，则通常保持
           xNextTaskUnblockTime 不变，因为当计数等于 xNextTaskUnblockTime 时会自动重置。
           但是，如果使用无节拍空闲模式，则可能更重要的是尽快进入睡眠模式，
           所以在这里重置 xNextTaskUnblockTime 以确保尽早更新。*/
        prvResetNextTaskUnblockTime();
    }
    #endif

    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * 函数名称：xTaskRemoveFromUnorderedEventList
 * 功能描述：从无序事件列表中移除指定的任务，并将其从延迟执行列表中移除，
 *          然后将其添加到就绪执行列表中。此函数用于事件标志的实现。
 *
 * 参数说明：
 * @pxEventListItem: 指向事件列表项的指针。该列表项属于要移除的任务。
 * @xItemValue: 要存储在事件列表项中的新值。
 *
 * 返回值：
 * 如果从事件列表中移除的任务优先级高于调用此函数的任务，则返回 pdTRUE；
 * 否则返回 pdFALSE。
 */

BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )
{
    TCB_t *pxUnblockedTCB;
    BaseType_t xReturn;

    /* 此函数必须在调度器被暂停的情况下调用。它用于事件标志的实现。*/
    configASSERT( uxSchedulerSuspended != pdFALSE );

    /* 在事件列表中存储新的项目值。*/
    listSET_LIST_ITEM_VALUE( pxEventListItem, xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* 从事件标志中移除事件列表。中断不会访问事件标志。*/
    pxUnblockedTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( pxEventListItem );

    /* 将任务从延迟列表中移除，并添加到就绪列表中。调度器被暂停，
       因此中断不会访问就绪列表。*/
    ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
    prvAddTaskToReadyList( pxUnblockedTCB );

    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* 如果从事件列表中移除的任务优先级高于调用此函数的任务，则返回 pdTRUE。
           这允许调用任务知道是否应该立即强制上下文切换。*/
        xReturn = pdTRUE;

        /* 标记即将进行上下文切换，以防用户没有使用 "xHigherPriorityTaskWoken"
           参数来调用中断安全的 FreeRTOS 函数。*/
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * 函数名称：vTaskSetTimeOutState
 * 功能描述：设置指定超时结构的状态信息。此函数记录当前的滴答计数和溢出次数，
 *          以便后续计算超时时间。
 *
 * 参数说明：
 * @pxTimeOut: 指向 TimeOut 结构的指针，用于记录超时状态信息。
 */

void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{
    configASSERT( pxTimeOut ); // 断言 pxTimeOut 指针非空

    /* 记录当前的滴答计数和溢出次数。*/
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/

/*
 * 函数名称：xTaskCheckForTimeOut
 * 功能描述：检查当前任务是否超时，并根据结果调整剩余等待时间。
 *          如果超时，则返回 pdTRUE；如果没有超时，则返回 pdFALSE 并更新剩余等待时间。
 *
 * 参数说明：
 * @pxTimeOut: 指向 TimeOut 结构的指针，用于记录超时状态信息。
 * @pxTicksToWait: 指向一个变量的指针，该变量用于指定等待时间（以滴答数表示）。
 *
 * 返回值：
 * 如果任务已超时，则返回 pdTRUE；否则返回 pdFALSE。
 */

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )
{
    BaseType_t xReturn;

    configASSERT( pxTimeOut ); // 断言 pxTimeOut 指针非空
    configASSERT( pxTicksToWait ); // 断言 pxTicksToWait 指针非空

    taskENTER_CRITICAL();
    {
        /* 在此代码块中，滴答计数不会改变。*/
        const TickType_t xConstTickCount = xTickCount;

        #if( INCLUDE_xTaskAbortDelay == 1 )
            if( pxCurrentTCB->ucDelayAborted != pdFALSE )
            {
                /* 如果延迟被中断，则认为发生了超时（尽管实际不是超时），返回 pdTRUE。*/
                pxCurrentTCB->ucDelayAborted = pdFALSE;
                xReturn = pdTRUE;
            }
            else
        #endif

        #if ( INCLUDE_vTaskSuspend == 1 )
            if( *pxTicksToWait == portMAX_DELAY )
            {
                /* 如果定义了 INCLUDE_vTaskSuspend 并且等待时间被设置为最大值，
                   则任务应无限期等待，因此不会超时。*/
                xReturn = pdFALSE;
            }
            else
        #endif

        if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) )
        {
            /* 滴答计数大于 vTaskSetTimeOut 被调用时的时间，并且从那时起也发生了溢出。
               这表明从 vTaskSetTimeOut 被调用以来经过了一段时间，导致计数器溢出并再次增加。*/
            xReturn = pdTRUE;
        }
        else if( ( ( TickType_t ) ( xConstTickCount - pxTimeOut->xTimeOnEntering ) ) < *pxTicksToWait )
        {
            /* 未真正超时。调整剩余等待时间。*/
            *pxTicksToWait -= ( xConstTickCount - pxTimeOut->xTimeOnEntering );
            vTaskSetTimeOutState( pxTimeOut );
            xReturn = pdFALSE;
        }
        else
        {
            xReturn = pdTRUE;
        }
    }
    taskEXIT_CRITICAL();

    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * 函数名称：vTaskMissedYield
 * 功能描述：标记当前任务错过了一次上下文切换的机会。这通常用于指示系统需要尽快进行一次上下文切换。
 *
 * 参数说明：
 * 无
 */

void vTaskMissedYield( void )
{
    xYieldPending = pdTRUE; // 标记需要进行上下文切换
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )
/*
 * 函数名称：uxTaskGetTaskNumber
 * 功能描述：获取指定任务的编号。如果任务句柄有效，则返回任务的编号；
 *          否则返回 0。
 *
 * 参数说明：
 * @xTask: 指向任务控制块的句柄（TaskHandle_t 类型）。
 *
 * 返回值：
 * 返回任务的编号，如果任务句柄无效，则返回 0。
 */

UBaseType_t uxTaskGetTaskNumber( TaskHandle_t xTask )
{
    UBaseType_t uxReturn;
    TCB_t *pxTCB;

    if( xTask != NULL )
    {
        pxTCB = ( TCB_t * ) xTask;
        uxReturn = pxTCB->uxTaskNumber;
    }
    else
    {
        uxReturn = 0U;
    }

    return uxReturn;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/*
 * 函数名称：vTaskSetTaskNumber
 * 功能描述：设置指定任务的编号。如果任务句柄有效，则设置任务的编号。
 *
 * 参数说明：
 * @xTask: 指向任务控制块的句柄（TaskHandle_t 类型）。
 * @uxHandle: 要设置的任务编号。
 */

void vTaskSetTaskNumber( TaskHandle_t xTask, const UBaseType_t uxHandle )
{
    TCB_t *pxTCB;

    if( xTask != NULL )
    {
        pxTCB = ( TCB_t * ) xTask;
        pxTCB->uxTaskNumber = uxHandle;
    }
}

#endif /* configUSE_TRACE_FACILITY */

/*
 * -----------------------------------------------------------
 * 空闲任务。
 * -----------------------------------------------------------
 *
 * portTASK_FUNCTION() 宏用于允许端口/编译器特定的语言扩展。
 * 此函数等价的原型为：
 *
 * void prvIdleTask( void *pvParameters );
 *
 */

static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    /* 避免未使用的参数警告。 */
    ( void ) pvParameters;

    /** 这是 RTOS 空闲任务 —— 当调度器启动时会自动创建。 **/

    for( ;; )
    {
        /* 检查是否有任何任务自行删除。如果有，则空闲任务负责释放已删除任务的 TCB 和栈空间。 */
        prvCheckTasksWaitingTermination();

        #if ( configUSE_PREEMPTION == 0 )
        {
            /* 如果没有使用抢占式调度，则强制进行任务切换以查看是否有其他任务可用。
               如果使用抢占式调度，则不需要这样做，因为任何可用的任务都会自动获得处理器。 */
            taskYIELD();
        }
        #endif /* configUSE_PREEMPTION */

        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
        {
            /* 使用抢占式调度时，相同优先级的任务将被时间片轮转。如果与空闲优先级共享的任务准备运行，
               则空闲任务应在时间片结束前让出处理器。
               无需在此处使用临界区，因为我们只是读取列表，偶尔的错误值也不会影响。 */
            if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( UBaseType_t ) 1 )
            {
                taskYIELD();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) ) */

        #if ( configUSE_IDLE_HOOK == 1 )
        {
            extern void vApplicationIdleHook( void );

            /* 在空闲任务中调用用户定义的函数。这允许应用设计者在没有额外任务开销的情况下添加后台功能。
               注意：vApplicationIdleHook() 绝对不能调用可能会阻塞的函数。 */
            vApplicationIdleHook();
        }
        #endif /* configUSE_IDLE_HOOK */

        /* 条件编译应该使用不等于 0 的判断，而不是等于 1 的判断。这是为了确保
           在用户定义的低功耗模式实现中需要将 configUSE_TICKLESS_IDLE 设置为不同于 1 的值时，
           仍然能够调用 portSUPPRESS_TICKS_AND_SLEEP()。 */
        #if ( configUSE_TICKLESS_IDLE != 0 )
        {
            TickType_t xExpectedIdleTime;

            /* 每次空闲任务迭代时，不希望挂起然后恢复调度器。因此，先进行一次初步的预计空闲时间测试，
               无需挂起调度器。此时的结果不一定有效。 */
            xExpectedIdleTime = prvGetExpectedIdleTime();

            if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
            {
                vTaskSuspendAll();
                {
                    /* 现在调度器已挂起，可以再次采样预计空闲时间，并且这次可以使用该值。 */
                    configASSERT( xNextTaskUnblockTime >= xTickCount );
                    xExpectedIdleTime = prvGetExpectedIdleTime();

                    if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
                    {
                        traceLOW_POWER_IDLE_BEGIN();
                        portSUPPRESS_TICKS_AND_SLEEP( xExpectedIdleTime );
                        traceLOW_POWER_IDLE_END();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                ( void ) xTaskResumeAll();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* configUSE_TICKLESS_IDLE */
    }
}
/*-----------------------------------------------------------*/

#if( configUSE_TICKLESS_IDLE != 0 )

/*
 * 函数名称：eTaskConfirmSleepModeStatus
 * 功能描述：确认系统是否可以进入低功耗模式，或者是否应中止进入低功耗模式。
 *          根据当前任务的状态返回相应的枚举值。
 *
 * 参数说明：
 * 无
 *
 * 返回值：
 * 返回 eSleepModeStatus 枚举类型中的一个值，表示是否可以安全地进入低功耗模式。
 */

eSleepModeStatus eTaskConfirmSleepModeStatus( void )
{
    /* 除了应用程序任务外，还有一个空闲任务存在。 */
    const UBaseType_t uxNonApplicationTasks = 1;
    eSleepModeStatus eReturn = eStandardSleep;

    if( listCURRENT_LIST_LENGTH( &xPendingReadyList ) != 0 )
    {
        /* 在调度器被挂起期间有一个任务被置为就绪状态。 */
        eReturn = eAbortSleep;
    }
    else if( xYieldPending != pdFALSE )
    {
        /* 在调度器被挂起期间请求了一个上下文切换。 */
        eReturn = eAbortSleep;
    }
    else
    {
        /* 如果所有任务都在挂起列表中（这可能意味着它们具有无限的阻塞时间，而不是实际上被挂起）
           那么可以安全地关闭所有时钟并等待外部中断。 */
        if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == ( uxCurrentNumberOfTasks - uxNonApplicationTasks ) )
        {
            eReturn = eNoTasksWaitingTimeout;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    return eReturn;
}
#endif /* configUSE_TICKLESS_IDLE */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/**
 * @brief 设置任务的线程局部存储指针。
 *
 * 此函数用于设置一个任务的线程局部存储（TLS）指针。TLS 允许每个任务拥有自己的一份数据，
 * 即使多个任务共享同一个全局数据结构。此函数确保 TLS 指针在指定的任务上下文中是有效的。
 *
 * @param xTaskToSet 要设置 TLS 指针的任务句柄。
 * @param xIndex TLS 指针数组中的索引。
 * @param pvValue 要设置的新 TLS 指针值。
 */
void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
{
    TCB_t *pxTCB; // 指向任务控制块（Task Control Block）的指针

    // 检查索引是否在 TLS 指针数组的有效范围内
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        // 从任务句柄获取任务控制块（TCB）
        pxTCB = prvGetTCBFromHandle( xTaskToSet );

        // 设置 TLS 指针
        pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
    }
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/**
 * @brief 获取任务的线程局部存储指针。
 *
 * 此函数用于获取一个任务的线程局部存储（TLS）指针。TLS 允许每个任务拥有自己的一份数据，
 * 即使多个任务共享同一个全局数据结构。此函数返回指定索引处的 TLS 指针值。
 *
 * @param xTaskToQuery 要查询 TLS 指针的任务句柄。
 * @param xIndex TLS 指针数组中的索引。
 * @return 返回指定索引处的 TLS 指针值。
 */
void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
{
    void *pvReturn = NULL; // 默认返回值为 NULL
    TCB_t *pxTCB; // 指向任务控制块（Task Control Block）的指针

    // 检查索引是否在 TLS 指针数组的有效范围内
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        // 从任务句柄获取任务控制块（TCB）
        pxTCB = prvGetTCBFromHandle( xTaskToQuery );

        // 获取 TLS 指针
        pvReturn = pxTCB->pvThreadLocalStoragePointers[ xIndex ];
    }
    else
    {
        // 如果索引超出范围，返回 NULL
        pvReturn = NULL;
    }

    return pvReturn;
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

/**
 * @brief 为任务分配 MPU 区域。
 *
 * 此函数用于为一个任务分配内存保护单元（MPU）区域。MPU 是一种硬件特性，
 * 用于保护内存区域，防止任务之间的内存冲突。此函数允许用户为指定的任务
 * 配置特定的内存区域，从而实现内存保护。
 *
 * @param xTaskToModify 要修改其 MPU 设置的任务句柄。如果为 NULL，则修改当前任务。
 * @param xRegions 指向 MemoryRegion_t 结构体数组的指针，描述了要分配的 MPU 区域。
 */
void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify, const MemoryRegion_t * const xRegions )
{
    TCB_t *pxTCB; // 指向任务控制块（Task Control Block）的指针

    // 如果传入的句柄为 NULL，则修改当前任务的 MPU 设置
    pxTCB = prvGetTCBFromHandle( xTaskToModify );

    // 将指定的 MPU 区域设置存储到任务控制块中
    vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ), xRegions, NULL, 0 );
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

/**
 * @brief 初始化任务列表。
 *
 * 此函数用于初始化任务列表。这些列表用于管理不同优先级的任务，以及延迟任务、
 * 待处理任务、等待终止的任务和挂起的任务。此函数确保所有相关列表在系统启动时
 * 被正确初始化。
 */
static void prvInitialiseTaskLists( void )
{
    UBaseType_t uxPriority; // 用于循环的优先级变量

    // 遍历所有优先级级别
    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {
        // 初始化每个优先级对应的就绪任务列表
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
    }

    // 初始化两个延迟任务列表
    vListInitialise( &xDelayedTaskList1 );
    vListInitialise( &xDelayedTaskList2 );

    // 初始化待处理任务列表
    vListInitialise( &xPendingReadyList );

    // 如果启用了任务删除功能，则初始化等待终止的任务列表
    #if ( INCLUDE_vTaskDelete == 1 )
    {
        vListInitialise( &xTasksWaitingTermination );
    }
    #endif /* INCLUDE_vTaskDelete */

    // 如果启用了任务挂起功能，则初始化挂起任务列表
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        vListInitialise( &xSuspendedTaskList );
    }
    #endif /* INCLUDE_vTaskSuspend */

    // 设置初始的延迟任务列表和溢出延迟任务列表
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*-----------------------------------------------------------*/

/**
 * @brief 检查并清理等待终止的任务列表。
 *
 * 此函数用于检查并清理等待终止的任务列表。该函数由 RTOS 的空闲任务（Idle Task）
 * 调用，确保已删除的任务从系统中完全清除。这有助于维护系统的内存和资源管理。
 *
 * 注意：此函数只能从空闲任务调用。
 */
static void prvCheckTasksWaitingTermination( void )
{
    #if ( INCLUDE_vTaskDelete == 1 )
    {
        BaseType_t xListIsEmpty; // 用于检查列表是否为空的标志

        // ucTasksDeleted 用于防止 vTaskSuspendAll() 在空闲任务中过于频繁地调用
        while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
        {
            // 暂停所有任务调度
            vTaskSuspendAll();

            // 检查等待终止的任务列表是否为空
            xListIsEmpty = listLIST_IS_EMPTY( &xTasksWaitingTermination );

            // 恢复任务调度
            ( void ) xTaskResumeAll();

            if( xListIsEmpty == pdFALSE )
            {
                TCB_t *pxTCB; // 指向任务控制块（Task Control Block）的指针

                // 进入临界区
                taskENTER_CRITICAL();

                {
                    // 获取等待终止任务列表头部的任务控制块
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) );

                    // 从列表中移除该任务控制块
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    // 减少当前任务总数
                    --uxCurrentNumberOfTasks;

                    // 减少等待清理的已删除任务数
                    --uxDeletedTasksWaitingCleanUp;
                }

                // 退出临界区
                taskEXIT_CRITICAL();

                // 清理任务控制块
                prvDeleteTCB( pxTCB );
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    #endif /* INCLUDE_vTaskDelete */
}
/*-----------------------------------------------------------*/

#if( configUSE_TRACE_FACILITY == 1 )

/**
 * @brief 获取任务的信息。
 *
 * 此函数用于获取指定任务的信息，并将这些信息填充到一个 TaskStatus_t 结构体中。
 * 此函数可用于监控和调试目的，可以获取任务的句柄、名称、优先级、栈基址、任务编号、
 * 基优先级（如果支持互斥量）、运行时间计数器（如果支持运行时统计）、当前状态以及栈空间的
 * 最高水位线。
 *
 * @param xTask 要获取信息的任务句柄。如果为 NULL，则获取调用此函数的任务的信息。
 * @param pxTaskStatus 用于存储任务信息的 TaskStatus_t 结构体指针。
 * @param xGetFreeStackSpace 如果为 pdTRUE，则获取栈空间的最高水位线；否则，不获取。
 * @param eState 用于设置任务状态的枚举值。如果为 eInvalid，则获取实际状态。
 */
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState )
{
    TCB_t *pxTCB; // 指向任务控制块（Task Control Block）的指针

    // 如果 xTask 是 NULL，则获取调用此函数的任务的信息
    pxTCB = prvGetTCBFromHandle( xTask );

    // 填充 TaskStatus_t 结构体的基本信息
    pxTaskStatus->xHandle = ( TaskHandle_t ) pxTCB;
    pxTaskStatus->pcTaskName = ( const char * ) &( pxTCB->pcTaskName[ 0 ] );
    pxTaskStatus->uxCurrentPriority = pxTCB->uxPriority;
    pxTaskStatus->pxStackBase = pxTCB->pxStack;
    pxTaskStatus->xTaskNumber = pxTCB->uxTCBNumber;

    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        // 如果任务在挂起列表中，则有可能实际上是被无限期阻塞的
        if( pxTaskStatus->eCurrentState == eSuspended )
        {
            // 暂停所有任务调度
            vTaskSuspendAll();

            {
                // 检查事件列表项是否在某个列表中
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    // 设置为阻塞状态
                    pxTaskStatus->eCurrentState = eBlocked;
                }
            }

            // 恢复任务调度
            xTaskResumeAll();
        }
    }
    #endif /* INCLUDE_vTaskSuspend */

    #if ( configUSE_MUTEXES == 1 )
    {
        // 如果支持互斥量，则获取基优先级
        pxTaskStatus->uxBasePriority = pxTCB->uxBasePriority;
    }
    #else
    {
        // 如果不支持互斥量，则基优先级设为 0
        pxTaskStatus->uxBasePriority = 0;
    }
    #endif

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        // 如果支持运行时统计，则获取运行时间计数器
        pxTaskStatus->ulRunTimeCounter = pxTCB->ulRunTimeCounter;
    }
    #else
    {
        // 如果不支持运行时统计，则运行时间计数器设为 0
        pxTaskStatus->ulRunTimeCounter = 0;
    }
    #endif

    // 如果传递的状态不是 eInvalid，则使用传递的状态
    if( eState != eInvalid )
    {
        pxTaskStatus->eCurrentState = eState;
    }
    else
    {
        // 否则获取实际状态
        pxTaskStatus->eCurrentState = eTaskGetState( xTask );
    }

    // 如果需要获取栈空间的最高水位线
    if( xGetFreeStackSpace != pdFALSE )
    {
        #if ( portSTACK_GROWTH > 0 )
        {
            // 栈向下增长的情况
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxEndOfStack );
        }
        #else
        {
            // 栈向上增长的情况
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxStack );
        }
        #endif
    }
    else
    {
        // 如果不需要获取栈空间的最高水位线，则设为 0
        pxTaskStatus->usStackHighWaterMark = 0;
    }
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/**
 * @brief 从单个任务列表中列出所有任务的信息。
 *
 * 此函数用于从一个任务列表中获取所有任务的信息，并将这些信息填充到一个 TaskStatus_t
 * 结构体数组中。此函数主要用于内部辅助函数，通常由其他函数调用来收集特定状态下的所有任务信息。
 *
 * @param pxTaskStatusArray 用于存储任务信息的 TaskStatus_t 结构体数组指针。
 * @param pxList 要列出的任务所在的列表。
 * @param eState 用于设置任务状态的枚举值。
 * @return 返回已处理的任务数量。
 */
static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState )
{
    volatile TCB_t *pxNextTCB, *pxFirstTCB; // 指向任务控制块（Task Control Block）的指针
    UBaseType_t uxTask = 0; // 用于记录已处理的任务数量

    // 检查列表长度是否大于0
    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        // 获取列表第一个元素的所有者
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        // 遍历列表中的每个任务，并填充 TaskStatus_t 结构体
        do
        {
            // 获取下一个元素的所有者
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

            // 获取任务信息并填充到数组中
            vTaskGetInfo( ( TaskHandle_t ) pxNextTCB, &( pxTaskStatusArray[ uxTask ] ), pdTRUE, eState );

            // 增加已处理的任务数量
            uxTask++;
        } while( pxNextTCB != pxFirstTCB );
    }
    else
    {
        // 代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }

    // 返回已处理的任务数量
    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

/**
 * @brief 检查任务栈中剩余的自由空间。
 *
 * 此函数用于检查任务栈中剩余的自由空间。此函数遍历栈，直到找到第一个不等于
 * tskSTACK_FILL_BYTE 的字节为止，并计算栈中连续等于 tskSTACK_FILL_BYTE 的字节数。
 * 这些字节通常是在栈初始化时用来填充栈的空闲空间的字节。函数返回连续相同字节的数量，
 * 作为栈中剩余自由空间的一个近似度量。
 *
 * @param pucStackByte 指向栈中某字节的指针，通常是栈顶或栈底。
 * @return 返回连续相同字节的数量，作为栈中剩余自由空间的一个近似度量。
 */
static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte )
{
    uint32_t ulCount = 0U; // 用于计数连续相同字节的数量

    // 遍历栈，直到找到第一个不等于 tskSTACK_FILL_BYTE 的字节
    while( *pucStackByte == ( uint8_t ) tskSTACK_FILL_BYTE )
    {
        // 根据栈的增长方向移动指针
        pucStackByte -= portSTACK_GROWTH;

        // 增加计数器
        ulCount++;
    }

    // 将计数值转换为 StackType_t 类型的字节数
    ulCount /= ( uint32_t ) sizeof( StackType_t );

    // 返回连续相同字节的数量，作为栈中剩余自由空间的一个近似度量
    return ( uint16_t ) ulCount;
}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

//获取任务栈的高水位标记，根据任务栈的起始位置，调用内部函数计算栈的高水位标记
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
{
    TCB_t *pxTCB;                           // 定义指向任务控制块的指针
    uint8_t *pucEndOfStack;                 // 定义指向栈顶或栈底的指针
    UBaseType_t uxReturn;                   // 定义返回值，用于存储栈的高水位标记

    // 从任务句柄获取对应的TCB指针
    pxTCB = prvGetTCBFromHandle( xTask );

    // 根据栈增长方向确定栈的起始位置
    #if portSTACK_GROWTH < 0
    {
        // 如果栈向下增长（减小方向），则栈的起始位置是任务的栈基址
        pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
    }
    #else
    {
        // 如果栈向上增长（增加方向），则栈的起始位置是栈的结束位置
        pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
    }
    #endif

    // 调用内部函数计算栈的高水位标记
    uxReturn = ( UBaseType_t ) prvTaskCheckFreeStackSpace( pucEndOfStack );

    // 返回栈的高水位标记
    return uxReturn;
}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )

	/*
 * @brief prvDeleteTCB 删除任务控制块（TCB）及其相关资源。
 *
 * 此函数用于释放与任务控制块（TCB）相关的所有资源，包括栈空间和 TCB 本身。
 *
 * @param pxTCB 指向任务控制块的指针。
 */
static void prvDeleteTCB(TCB_t *pxTCB)
{
    /* 清理 TCB 数据结构。 */
    portCLEAN_UP_TCB(pxTCB);

    #if (configUSE_NEWLIB_REENTRANT == 1)
    {
        /* 如果启用了 Newlib 的可重入支持，则释放 Newlib 的 reent 结构。 */
        _reclaim_reent(&(pxTCB->xNewLib_reent));
    }
    #endif /* configUSE_NEWLIB_REENTRANT */

    #if( (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 0) && (portUSING_MPU_WRAPPERS == 0) )
    {
        /* 如果仅支持动态分配，且不支持静态分配，并且没有使用 MPU 包装器，
         * 则释放栈和 TCB。 */

        /* 释放任务栈。 */
        vPortFree(pxTCB->pxStack);

        /* 释放 TCB 结构。 */
        vPortFree(pxTCB);
    }
    #elif( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE == 1 )
    {
        /* 如果同时支持静态和动态分配，则检查哪些资源是动态分配的，并释放它们。 */

        if(pxTCB->ucStaticallyAllocated == tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB)
        {
            /* 如果栈和 TCB 都是动态分配的，则释放它们。 */

            /* 释放任务栈。 */
            vPortFree(pxTCB->pxStack);

            /* 释放 TCB 结构。 */
            vPortFree(pxTCB);
        }
        else if(pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_ONLY)
        {
            /* 如果只有栈是静态分配的，则只释放 TCB 结构。 */

            /* 释放 TCB 结构。 */
            vPortFree(pxTCB);
        }
        else
        {
            /* 如果栈和 TCB 都是静态分配的，则不需要释放任何资源。 */

            /* 断言确保栈和 TCB 都是静态分配的。 */
            configASSERT(pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB);

            /* 覆盖测试标记。 */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

/**
 * @brief 重置下次任务解阻塞的时间。
 *
 * 此函数用于重置下次任务解阻塞的时间。当延迟任务列表发生变化时，
 * 此函数会检查列表是否为空，并根据情况更新 xNextTaskUnblockTime 变量，
 * 以便在下次检查时知道何时有任务应该从阻塞状态变为可调度状态。
 */
static void prvResetNextTaskUnblockTime( void )
{
    TCB_t *pxTCB; // 指向任务控制块（Task Control Block）的指针

    // 检查延迟任务列表是否为空
    if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
    {
        // 如果延迟任务列表为空，则将下次任务解阻塞时间设置为最大值
        // 这样，在延迟任务列表为空的情况下，几乎不可能满足
        // if( xTickCount >= xNextTaskUnblockTime ) 的条件
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        // 如果延迟任务列表不为空，则获取列表头部的任务控制块
        pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );

        // 获取列表头部任务的解阻塞时间
        xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );
    }
}
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) )

/**
 * @brief 获取当前执行任务的句柄。
 *
 * 此函数用于获取当前执行任务的句柄。由于此函数不在中断上下文中调用，
 * 并且对于任何单一的执行线程来说，当前的任务控制块（TCB）总是相同的，
 * 因此不需要进入临界区。
 *
 * @return 返回当前任务的句柄。
 */
TaskHandle_t xTaskGetCurrentTaskHandle( void )
{
    TaskHandle_t xReturn; // 用于存储当前任务句柄的变量

    // 不需要临界区保护，因为此函数不在中断上下文中调用，
    // 并且当前 TCB 总是对于任何单一执行线程而言是相同的。
    xReturn = pxCurrentTCB;

    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )

/**
 * @brief 获取当前调度器的状态。
 *
 * 此函数用于获取当前调度器的状态。它检查调度器是否已经启动，并根据
 * 调度器的状态返回相应的枚举值。
 *
 * @return 返回当前调度器的状态枚举值。
 */
BaseType_t xTaskGetSchedulerState( void )
{
    BaseType_t xReturn; // 用于存储并返回当前调度器状态的变量

    // 检查调度器是否已经启动
    if( xSchedulerRunning == pdFALSE )
    {
        // 如果调度器尚未启动，则返回 taskSCHEDULER_NOT_STARTED
        xReturn = taskSCHEDULER_NOT_STARTED;
    }
    else
    {
        // 如果调度器已经启动，则进一步检查是否被暂停
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            // 如果调度器正在运行，则返回 taskSCHEDULER_RUNNING
            xReturn = taskSCHEDULER_RUNNING;
        }
        else
        {
            // 如果调度器被暂停，则返回 taskSCHEDULER_SUSPENDED
            xReturn = taskSCHEDULER_SUSPENDED;
        }
    }

    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

/**
 * @brief 实现优先级继承机制。
 *
 * 此函数用于实现在多任务环境中，当一个低优先级的任务持有一个互斥锁而高优先级的任务
 * 试图获取该锁时，低优先级任务继承高优先级任务的优先级，从而避免优先级反转的问题。
 *
 * @param pxMutexHolder 持有互斥锁的任务句柄。
 */
void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
{
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder; // 指向持有互斥锁的任务控制块（TCB）

    // 如果互斥锁持有者不为 NULL，则继续处理
    if( pxMutexHolder != NULL )
    {
        // 如果持有互斥锁的任务优先级低于试图获取互斥锁的任务的优先级
        if( pxTCB->uxPriority < pxCurrentTCB->uxPriority )
        {
            // 调整互斥锁持有者的状态，以反映其新的优先级
            // 只有在列表项值未被用于其他用途时才重置事件列表项值
            if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
            {
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority );
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }

            // 如果被修改的任务处于就绪状态，则需要将其移动到新的列表中
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                // 从当前优先级的就绪任务列表中移除任务
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    // 代码覆盖率测试标记
                    mtCOVERAGE_TEST_MARKER();
                }

                // 继承新优先级前先将任务移动到新的列表中
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // 只继承新优先级
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
            }

            // 记录优先级继承的日志
            traceTASK_PRIORITY_INHERIT( pxTCB, pxCurrentTCB->uxPriority );
        }
        else
        {
            // 代码覆盖率测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // 如果互斥锁持有者为 NULL，则插入代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

/**
 * @brief 实现优先级归还机制。
 *
 * 此函数用于实现优先级归还机制。当一个高优先级任务释放了一个它持有的互斥锁，
 * 而低优先级任务之前为了获取该互斥锁而继承了高优先级，此时该低优先级任务应当
 * 归还其原本的优先级。
 *
 * @param pxMutexHolder 持有互斥锁的任务句柄。
 * @return 如果任务归还了继承的优先级，则返回 pdTRUE；否则返回 pdFALSE。
 */
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
{
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder; // 指向持有互斥锁的任务控制块（TCB）
    BaseType_t xReturn = pdFALSE; // 默认返回值为 pdFALSE

    // 如果互斥锁持有者不为 NULL，则继续处理
    if( pxMutexHolder != NULL )
    {
        // 断言：持有互斥锁的任务必须是当前正在运行的任务
        configASSERT( pxTCB == pxCurrentTCB );

        // 断言：持有互斥锁的任务必须至少持有一个互斥锁
        configASSERT( pxTCB->uxMutexesHeld );

        // 减少持有互斥锁的计数
        ( pxTCB->uxMutexesHeld )--;

        // 检查持有互斥锁的任务是否继承了另一个任务的优先级
        if( pxTCB->uxPriority != pxTCB->uxBasePriority )
        {
            // 只有在没有其他互斥锁被持有时才能归还优先级
            if( pxTCB->uxMutexesHeld == ( UBaseType_t ) 0 )
            {
                // 从当前优先级的就绪任务列表中移除任务
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    // 代码覆盖率测试标记
                    mtCOVERAGE_TEST_MARKER();
                }

                // 归还优先级前先记录日志
                traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );

                // 归还原本的优先级
                pxTCB->uxPriority = pxTCB->uxBasePriority;

                // 重置事件列表项值。当此任务正在运行并且必须运行以释放互斥锁时，
                // 它不能用于任何其他目的。
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxTCB->uxPriority );

                // 将任务添加到新的就绪列表中
                prvAddTaskToReadyList( pxTCB );

                // 返回 pdTRUE 表示需要进行上下文切换
                // 这种情况只在多个互斥锁被持有并且互斥锁的释放顺序不同于获取顺序时发生。
                // 即使没有任务等待第一个互斥锁，当最后一个互斥锁被释放时也应该进行上下文切换。
                xReturn = pdTRUE;
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 代码覆盖率测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // 如果互斥锁持有者为 NULL，则插入代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }

    return xReturn;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

/**
 * @brief 进入临界区。
 *
 * 此函数用于进入一个临界区，在该区域内不允许中断发生，以确保某些关键操作能够原子地完成。
 * 当进入临界区时，中断会被禁用，并且当前任务的临界区嵌套计数会增加。
 */
void vTaskEnterCritical( void )
{
    // 禁用中断
    portDISABLE_INTERRUPTS();

    if( xSchedulerRunning != pdFALSE )
    {
        // 增加当前任务的临界区嵌套计数
        ( pxCurrentTCB->uxCriticalNesting )++;

        // 这不是中断安全版本的进入临界区函数，因此如果它被从中断上下文调用，则断言失败。
        // 只有以 "FromISR" 结尾的 API 函数可以在中断中使用。
        // 只在临界区嵌套计数为 1 时进行断言，以防止递归调用时断言函数本身也使用临界区的情况。
        if( pxCurrentTCB->uxCriticalNesting == 1 )
        {
            portASSERT_IF_IN_ISR();
        }
    }
    else
    {
        // 代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

/**
 * @brief 退出临界区。
 *
 * 此函数用于退出一个临界区，在该区域内不允许中断发生。当退出临界区时，
 * 当前任务的临界区嵌套计数会减少，并且如果这是最后一个嵌套级别，则中断会被重新启用。
 */
void vTaskExitCritical( void )
{
    if( xSchedulerRunning != pdFALSE )
    {
        // 检查当前任务的临界区嵌套计数是否大于0
        if( pxCurrentTCB->uxCriticalNesting > 0U )
        {
            // 减少当前任务的临界区嵌套计数
            ( pxCurrentTCB->uxCriticalNesting )--;

            // 如果这是最后一个嵌套级别，则重新启用中断
            if( pxCurrentTCB->uxCriticalNesting == 0U )
            {
                portENABLE_INTERRUPTS();
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 代码覆盖率测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // 代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

/**
 * @brief 将任务名称写入缓冲区，并在末尾填充空格。
 *
 * 此函数用于将任务名称写入指定的缓冲区，并在字符串末尾填充空格，
 * 以确保打印输出时列对齐。此函数通常用于格式化输出，如在调试或统计信息显示中。
 *
 * @param pcBuffer 指向目标缓冲区的指针。
 * @param pcTaskName 指向任务名称字符串的指针。
 * @return 返回指向缓冲区末尾的新指针。
 */
static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName )
{
    size_t x;

    // 首先复制整个字符串到缓冲区
    strcpy( pcBuffer, pcTaskName );

    // 在字符串末尾填充空格，以确保打印输出时列对齐
    for( x = strlen( pcBuffer ); x < ( size_t )( configMAX_TASK_NAME_LEN - 1 ); x++ )
    {
        pcBuffer[ x ] = ' ';
    }

    // 添加字符串终止符
    pcBuffer[ x ] = 0x00;

    // 返回指向缓冲区末尾的新指针
    return &( pcBuffer[ x ] );
}

#endif /* ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

/**
 * @brief 生成包含所有活动任务状态信息的可读字符串。
 *
 * 此函数用于生成一个包含所有活动任务的状态信息的可读字符串。它首先调用
 * uxTaskGetSystemState 获取系统的状态数据，然后格式化部分输出为人类可读的表格形式，
 * 展示任务名称、状态和栈使用情况等信息。
 *
 * @param pcWriteBuffer 指向用于存放输出字符串的缓冲区。
 */
void vTaskList( char * pcWriteBuffer )
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    char cStatus;

    // 清空写缓冲区，防止已有字符串残留
    *pcWriteBuffer = 0x00;

    // 获取当前活动任务的数量
    uxArraySize = uxCurrentNumberOfTasks;

    // 分配足够大小的数组来保存每个任务的状态信息
    // 注意：如果 configSUPPORT_DYNAMIC_ALLOCATION 被设为 0，则 pvPortMalloc() 将等价于 NULL
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    if( pxTaskStatusArray != NULL )
    {
        // 生成二进制数据
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

        // 将二进制数据转换成人类可读的表格形式
        for( x = 0; x < uxArraySize; x++ )
        {
            // 根据任务当前状态设置字符标志
            switch( pxTaskStatusArray[ x ].eCurrentState )
            {
                case eReady:      cStatus = tskREADY_CHAR; break;
                case eBlocked:    cStatus = tskBLOCKED_CHAR; break;
                case eSuspended:  cStatus = tskSUSPENDED_CHAR; break;
                case eDeleted:    cStatus = tskDELETED_CHAR; break;
                default:          // 防止静态检查错误
                                cStatus = 0x00; break;
            }

            // 将任务名称写入字符串，并在末尾填充空格以便更容易以表格形式打印
            pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

            // 写入其余的字符串信息
            sprintf( pcWriteBuffer, "\t%c\t%u\t%u\t%u\r\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
            pcWriteBuffer += strlen( pcWriteBuffer );
        }

        // 释放分配的数组内存
        // 注意：如果 configSUPPORT_DYNAMIC_ALLOCATION 为 0，则 vPortFree() 将被定义为空
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        // 插入代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}
#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*----------------------------------------------------------*/

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

/**
 * @brief 获取所有任务的运行时间统计数据，并格式化输出到指定的缓冲区。
 * 
 * 此函数用于获取当前系统中所有任务的运行时间统计数据，并将这些统计数据
 * 格式化成一个易读的表格形式，输出到指定的缓冲区中。主要功能包括：
 * 1. 获取任务状态；
 * 2. 计算总运行时间；
 * 3. 格式化输出；
 * 4. 内存管理。
 * 
 * @param pcWriteBuffer 指定的缓冲区，用于存储格式化的统计数据。
 */
void vTaskGetRunTimeStats( char *pcWriteBuffer )
{
    TaskStatus_t *pxTaskStatusArray; // 用于存储任务状态的数组指针
    volatile UBaseType_t uxArraySize, x; // 用于存储数组大小和循环索引的变量
    uint32_t ulTotalTime, ulStatsAsPercentage; // 存储总运行时间和百分比的变量

    // 确保启用了 trace facility
    #if( configUSE_TRACE_FACILITY != 1 )
    {
        #error configUSE_TRACE_FACILITY 必须在 FreeRTOSConfig.h 中设置为 1 以使用 vTaskGetRunTimeStats()。
    }
    #endif

    /*
     * 请注意：
     *
     * 此函数仅用于方便，并由许多演示应用程序使用。不要将其视为调度程序的一部分。
     *
     * vTaskGetRunTimeStats() 调用 uxTaskGetSystemState()，然后将部分 uxTaskGetSystemState() 输出格式化为一个人类可读的表格，该表格显示了每个任务处于 Running 状态的时间（绝对时间和百分比）。
     *
     * vTaskGetRunTimeStats() 依赖于 C 库函数 sprintf()，这可能会增加代码大小、使用大量堆栈，并且在不同平台上提供不同的结果。作为替代方案，许多 FreeRTOS/Demo 子目录中的 printf-stdarg.c 文件提供了 printf() 和 va_args 的小型、第三方、有限功能实现（注意 printf-stdarg.c 不提供完整的 snprintf() 实现！）。
     *
     * 建议生产系统直接调用 uxTaskGetSystemState() 以获取原始统计数据，而不是间接通过 vTaskGetRunTimeStats() 调用。
     */

    // 确保写缓冲区不包含字符串
    *pcWriteBuffer = 0x00;

    // 获取当前任务数量的快照，以防在此函数执行期间发生变化
    uxArraySize = uxCurrentNumberOfTasks;

    // 为每个任务分配一个数组索引。注意！如果 configSUPPORT_DYNAMIC_ALLOCATION 设置为 0，则 pvPortMalloc() 将等同于 NULL。
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    if( pxTaskStatusArray != NULL )
    {
        // 生成二进制数据
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

        // 用于百分比计算
        ulTotalTime /= 100UL;

        // 避免除零错误
        if( ulTotalTime > 0 )
        {
            // 从二进制数据创建人类可读的表格
            for( x = 0; x < uxArraySize; x++ )
            {
                // 任务使用了多少百分比的总运行时间？
                // 这将始终向下取整到最接近的整数。
                // ulTotalRunTimeDiv100 已经除以 100。
                ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;

                // 将任务名称写入字符串，填充空格以便更容易以表格形式打印。
                pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

                if( ulStatsAsPercentage > 0UL )
                {
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        // 使用长整型格式化输出
                        sprintf( pcWriteBuffer, "\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
                    }
                    #else
                    {
                        // sizeof(int) == sizeof(long)，所以可以使用较小的 printf() 库
                        sprintf( pcWriteBuffer, "\t%u\t\t%u%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter, ( unsigned int ) ulStatsAsPercentage );
                    }
                    #endif
                }
                else
                {
                    // 如果百分比为零，则表示任务消耗的时间少于总运行时间的1%。
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        // 使用长整型格式化输出
                        sprintf( pcWriteBuffer, "\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #else
                    {
                        // sizeof(int) == sizeof(long)，所以可以使用较小的 printf() 库
                        sprintf( pcWriteBuffer, "\t%u\t\t<1%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #endif
                }

                // 更新写缓冲区指针
                pcWriteBuffer += strlen( pcWriteBuffer );
            }
        }
        else
        {
            // 代码覆盖率测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        // 释放数组。注意！如果 configSUPPORT_DYNAMIC_ALLOCATION 为 0，则 vPortFree() 将被定义为空。
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        // 代码覆盖率测试标记
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*-----------------------------------------------------------*/

/**
 * @brief 重置当前任务事件列表项的值。
 *
 * 此函数用于重置当前任务事件列表项的值。首先获取当前任务事件列表项的值，
 * 然后将该列表项重置为其默认值，以便它可以再次用于队列和信号量操作。
 *
 * @return 返回原来的事件列表项值。
 */
TickType_t uxTaskResetEventItemValue( void )
{
    TickType_t uxReturn;

    // 获取当前任务事件列表项的值
    uxReturn = listGET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ) );

    // 重置事件列表项到其正常值，以便它可以再次用于队列和信号量操作
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ) );

    return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

/**
 * @brief 增加当前任务持有的互斥锁数量计数。
 *
 * 此函数用于在当前任务持有互斥锁时增加该任务持有的互斥锁数量计数。
 * 此函数通常在互斥锁被成功获取时调用，以跟踪当前任务所持有的互斥锁数量。
 *
 * @return 返回当前任务的控制块指针（TCB）。如果当前没有任务（即调度器还未开始运行），则返回 NULL。
 */
void *pvTaskIncrementMutexHeldCount( void )
{
    // 如果调度器还未开始运行或者尚未创建任务，则 pxCurrentTCB 为 NULL。
    if( pxCurrentTCB != NULL )
    {
        // 增加当前任务持有的互斥锁数量计数
        ( pxCurrentTCB->uxMutexesHeld )++;
    }

    // 返回当前任务的控制块指针（TCB）
    return pxCurrentTCB;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief 从当前任务中取出一个通知值。
 *
 * 此函数用于从当前任务中取出一个通知值。此函数允许当前任务等待直到接收到一个通知，
 * 或者立即返回当前的通知值。如果设置了 xClearCountOnExit 参数，则在返回后会清除通知计数。
 *
 * @param xClearCountOnExit 如果为 pdTRUE，则在返回时清除通知计数值。
 * @param xTicksToWait 等待的时间（以滴答为单位）。如果为 0，则不会阻塞等待。
 * @return 返回当前任务的通知值。
 */
uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
{
    uint32_t ulReturn;

    // 进入临界区
    taskENTER_CRITICAL();
    {
        // 如果通知计数已经是非零，则不需要等待
        if( pxCurrentTCB->ulNotifiedValue == 0UL )
        {
            // 标记当前任务为等待通知状态
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            if( xTicksToWait > ( TickType_t ) 0 )
            {
                // 将当前任务添加到延迟列表中，并设置等待时间
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
                traceTASK_NOTIFY_TAKE_BLOCK();

                // 允许在临界区内进行调度
                portYIELD_WITHIN_API();
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // 代码覆盖率测试标记
            mtCOVERAGE_TEST_MARKER();
        }
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    // 再次进入临界区
    taskENTER_CRITICAL();
    {
        // 记录通知取出的日志
        traceTASK_NOTIFY_TAKE();
        ulReturn = pxCurrentTCB->ulNotifiedValue;

        if( ulReturn != 0UL )
        {
            if( xClearCountOnExit != pdFALSE )
            {
                // 清除通知计数值
                pxCurrentTCB->ulNotifiedValue = 0UL;
            }
            else
            {
                // 如果不清除计数值，则减去已取出的通知数
                pxCurrentTCB->ulNotifiedValue = ulReturn - 1;
            }
        }
        else
        {
            // 代码覆盖率测试标记
            mtCOVERAGE_TEST_MARKER();
        }

        // 设置当前任务为不再等待通知状态
        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    // 再次退出临界区
    taskEXIT_CRITICAL();

    return ulReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/*
 * @brief xTaskNotifyWait 用于等待任务接收通知。
 *
 * 该函数用于等待任务接收到一个通知。如果在指定的时间内接收到通知，则返回 pdTRUE；
 * 否则，如果超时，则返回 pdFALSE。
 *
 * @param ulBitsToClearOnEntry 在进入等待前，清除任务通知值中的这些位。
 * @param ulBitsToClearOnExit 在退出等待前，清除任务通知值中的这些位。
 * @param pulNotificationValue 指针，指向接收通知值的变量。如果为 NULL，则不返回通知值。
 * @param xTicksToWait 等待时间（以 tick 为单位）。如果为 0 或 pdMS_TO_TICKS(0)，则不等待。
 *
 * @return 返回 pdTRUE 表示已接收到通知，pdFALSE 表示超时或没有接收到通知。
 */
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )
{
    BaseType_t xReturn;

    taskENTER_CRITICAL(); // 进入临界区
    {
        /* 只有在没有通知待处理的情况下才阻塞。 */
        if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
        {
            /* 在任务的通知值中清除指定的位。这可以用来将值清零。 */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnEntry;

            /* 标记此任务为等待通知。 */
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* 如果设置了等待时间，则将当前任务添加到延迟任务列表中。 */
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );

                traceTASK_NOTIFY_WAIT_BLOCK(); // 记录任务等待通知的日志

                /* 所有端口都允许在此临界区内进行调度（有些端口会立即调度，有些则等到临界区退出）——但这不是应用程序代码应该做的。 */
                portYIELD_WITHIN_API();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // 测试覆盖率标记
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); // 测试覆盖率标记
        }
    }
    taskEXIT_CRITICAL(); // 退出临界区

    taskENTER_CRITICAL(); // 再次进入临界区
    {
        traceTASK_NOTIFY_WAIT(); // 记录任务等待通知完成的日志

        if( pulNotificationValue != NULL )
        {
            /* 输出当前的通知值，该值可能已经改变。 */
            *pulNotificationValue = pxCurrentTCB->ulNotifiedValue;
        }

        /* 如果 ucNotifyValue 已经设置，则要么任务从未进入阻塞状态（因为已有通知待处理），要么任务因收到通知而解除阻塞。否则，任务因超时而解除阻塞。 */
        if( pxCurrentTCB->ucNotifyState == taskWAITING_NOTIFICATION )
        {
            /* 未收到通知。 */
            xReturn = pdFALSE;
        }
        else
        {
            /* 通知已待处理或等待期间收到了通知。 */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnExit;
            xReturn = pdTRUE;
        }

        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION; // 标记任务不再等待通知
    }
    taskEXIT_CRITICAL(); // 再次退出临界区

    return xReturn; // 返回结果
}
#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )
/*
 * @brief xTaskGenericNotify 用于向指定任务发送通知。
 *
 * 该函数用于设置指定任务的通知值，并将其从等待列表中移除，加入就绪列表，最后检测是否需要切换任务。
 *
 * @param xTaskToNotify 要通知的任务的句柄。
 * @param ulValue 通知的值。
 * @param eAction 通知的动作类型。
 * @param pulPreviousNotificationValue 指针，用于存放之前的通知值。如果为 NULL，则不保存之前的值。
 *
 * @return 返回 pdPASS 表示成功，pdFAIL 表示失败。
 */
BaseType_t xTaskGenericNotify(TaskHandle_t xTaskToNotify,
                               uint32_t ulValue,
                               eNotifyAction eAction,
                               uint32_t *pulPreviousNotificationValue)
{
    TCB_t *pxTCB;                                     // 定义指向任务控制块（TCB）的指针
    BaseType_t xReturn = pdPASS;                      // 返回状态，初始化为成功
    uint8_t ucOriginalNotifyState;                    // 保存任务原始通知状态

    configASSERT(xTaskToNotify);                      // 断言 xTaskToNotify 有效（不为 NULL）
    pxTCB = (TCB_t *) xTaskToNotify;                  // 将任务句柄转换为任务控制块（TCB）

    taskENTER_CRITICAL();                             // 进入临界区，防止在修改任务状态时发生上下文切换
    {
        if (pulPreviousNotificationValue != NULL)     // 检查是否需要存放之前的通知值
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue; // 保存当前的通知值
        }

        ucOriginalNotifyState = pxTCB->ucNotifyState; // 获取当前任务的通知状态

        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED; // 设置任务为接收通知状态

        switch (eAction)                              // 根据通知动作类型处理通知
        {
            case eSetBits:
                pxTCB->ulNotifiedValue |= ulValue;    // 设置指定的位
                break;

            case eIncrement:
                pxTCB->ulNotifiedValue++;             // 递增通知值
                break;

            case eSetValueWithOverwrite:
                pxTCB->ulNotifiedValue = ulValue;     // 直接设置通知值（覆盖）
                break;

            case eSetValueWithoutOverwrite:
                if (ucOriginalNotifyState != taskNOTIFICATION_RECEIVED) // 检查是否可以覆盖
                {
                    pxTCB->ulNotifiedValue = ulValue; // 仅在没有接收到通知的情况下设置值
                }
                else
                {
                    xReturn = pdFAIL;                // 如果已接收到通知，返回失败
                }
                break;

            case eNoAction:
                break;                               // 不执行任何操作
        }

        traceTASK_NOTIFY();                          // 记录任务通知事件（用于调试和监控）

        if (ucOriginalNotifyState == taskWAITING_NOTIFICATION) // 检查任务是否在等待通知
        {
            (void) uxListRemove(&(pxTCB->xStateListItem)); // 从等待列表中移除任务
            prvAddTaskToReadyList(pxTCB);                // 将任务添加到就绪列表

            configASSERT(listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) == NULL); // 断言事件列表为空

            #if (configUSE_TICKLESS_IDLE != 0)
            {
                prvResetNextTaskUnblockTime();         // 重置下一个解锁时间（如果启用了无滴答空闲模式）
            }
            #endif

            // 如果新通知的优先级高于当前任务的优先级，则进行任务切换
            if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
            {
                taskYIELD_IF_USING_PREEMPTION();       // 如果使用抢占，则进行上下文切换
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();              // 执行覆盖率测试标记（表示此分支的执行）
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                 // 执行覆盖率测试标记（表示未在等待通知的状态）
        }
    }
    taskEXIT_CRITICAL();                              // 退出临界区，恢复上下文切换

    return xReturn;                                   // 返回通知的结果（成功或失败）
}


#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief 从中断服务程序中通知一个任务。
 *
 * 此函数用于从中断服务程序（ISR）中通知一个任务。此函数允许在中断上下文中
 * 更新任务的通知值，并根据需要唤醒等待通知的任务。此外，它还可以确定是否有
 * 更高优先级的任务需要运行，并设置相应的标志。
 *
 * @param xTaskToNotify 要通知的任务句柄。
 * @param ulValue 用于更新通知值的数值。
 * @param eAction 指定如何更新通知值的操作类型。
 * @param pulPreviousNotificationValue 存储任务之前的旧通知值的指针。
 * @param pxHigherPriorityTaskWoken 存储是否唤醒了更高优先级任务的布尔标志指针。
 * @return 返回操作的结果，pdPASS 表示成功，pdFAIL 表示失败。
 */
BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )
{
    TCB_t * pxTCB;
    uint8_t ucOriginalNotifyState;
    BaseType_t xReturn = pdPASS;
    UBaseType_t uxSavedInterruptStatus;

    // 断言确保传递的任务句柄有效
    configASSERT( xTaskToNotify );

    // 确保在中断中调用此函数是合法的
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 获取当前任务的 TCB 指针
    pxTCB = ( TCB_t * ) xTaskToNotify;

    // 禁用中断并保存中断状态
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        // 如果提供了存储旧通知值的指针，则保存当前的通知值
        if( pulPreviousNotificationValue != NULL )
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }

        // 保存原始通知状态
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        // 根据提供的操作类型更新通知值
        switch( eAction )
        {
            case eSetBits:
                pxTCB->ulNotifiedValue |= ulValue;
                break;

            case eIncrement:
                ( pxTCB->ulNotifiedValue )++;
                break;

            case eSetValueWithOverwrite:
                pxTCB->ulNotifiedValue = ulValue;
                break;

            case eSetValueWithoutOverwrite:
                if( ucOriginalNotifyState != taskNOTIFICATION_RECEIVED )
                {
                    pxTCB->ulNotifiedValue = ulValue;
                }
                else
                {
                    // 无法写入新值
                    xReturn = pdFAIL;
                }
                break;

            case eNoAction:
                // 任务被通知但不更新通知值
                break;
        }

        // 记录通知操作
        traceTASK_NOTIFY_FROM_ISR();

        // 如果任务正等待接收通知，则将其移出阻塞状态
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            // 确认任务不在任何事件列表上
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                // 从延迟列表中移除任务
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                // 将任务加入就绪列表
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // 如果调度器被暂停，则将任务挂起直到调度器恢复
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            // 如果被通知的任务优先级高于当前执行的任务，则需要切换上下文
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                // 标记更高优先级的任务已被唤醒
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    // 如果用户没有使用 "xHigherPriorityTaskWoken" 参数，则标记上下文切换挂起
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }

    // 恢复中断状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief 从中断服务程序中通知一个任务。
 *
 * 此函数用于从中断服务程序（ISR）中通知一个任务。此函数在中断上下文中
 * 更新任务的通知值，并根据需要唤醒等待通知的任务。如果被通知的任务优先级
 * 高于当前正在执行的任务，则设置相应的标志，指示可能需要进行上下文切换。
 *
 * @param xTaskToNotify 要通知的任务句柄。
 * @param pxHigherPriorityTaskWoken 存储是否唤醒了更高优先级任务的布尔标志指针。
 */
void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )
{
    TCB_t * pxTCB;
    uint8_t ucOriginalNotifyState;
    UBaseType_t uxSavedInterruptStatus;

    // 断言确保传递的任务句柄有效
    configASSERT( xTaskToNotify );

    // 确保在中断中调用此函数是合法的
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // 获取当前任务的 TCB 指针
    pxTCB = ( TCB_t * ) xTaskToNotify;

    // 禁用中断并保存中断状态
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        // 保存原始通知状态
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        // 'Giving' 相当于在计数型信号量中递增计数
        ( pxTCB->ulNotifiedValue )++;

        // 记录通知操作
        traceTASK_NOTIFY_GIVE_FROM_ISR();

        // 如果任务正等待接收通知，则将其移出阻塞状态
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            // 确认任务不在任何事件列表上
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                // 从延迟列表中移除任务
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                // 将任务加入就绪列表
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // 如果调度器被暂停，则将任务挂起直到调度器恢复
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            // 如果被通知的任务优先级高于当前执行的任务，则需要切换上下文
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                // 标记更高优先级的任务已被唤醒
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    // 如果用户没有使用 "xHigherPriorityTaskWoken" 参数，则标记上下文切换挂起
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                // 代码覆盖率测试标记
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }

    // 恢复中断状态
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
}

#endif /* configUSE_TASK_NOTIFICATIONS */

/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief 清除任务的通知状态。
 *
 * 此函数用于清除一个任务的通知状态。如果任务当前处于已接收到通知的状态，
 * 则将其状态重置为未等待通知的状态。此函数返回一个布尔值，表示是否成功
 * 清除了通知状态。
 *
 * @param xTask 要清除通知状态的任务句柄。如果为 NULL，则清除当前任务的通知状态。
 * @return 返回操作的结果，pdPASS 表示成功，pdFAIL 表示失败。
 */
BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )
{
    TCB_t *pxTCB;
    BaseType_t xReturn;

    /* 如果传入的句柄为 NULL，则清除当前任务的通知状态。 */
    pxTCB = prvGetTCBFromHandle( xTask );

    // 进入临界区
    taskENTER_CRITICAL();
    {
        if( pxTCB->ucNotifyState == taskNOTIFICATION_RECEIVED )
        {
            // 将任务的状态重置为未等待通知
            pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
            xReturn = pdPASS;
        }
        else
        {
            // 如果任务不是在等待通知的状态，则无需做任何事情
            xReturn = pdFAIL;
        }
    }
    // 退出临界区
    taskEXIT_CRITICAL();

    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/


/**
 * @brief 将当前任务添加到延迟列表中。
 *
 * 此函数用于将当前任务添加到延迟列表中。此函数用于在任务等待特定事件发生时，
 * 将其从就绪列表中移除并放置在一个基于时间排序的延迟列表中。如果任务将无限期
 * 地等待，则将其放置在挂起任务列表中。
 *
 * @param xTicksToWait 任务等待的滴答数。
 * @param xCanBlockIndefinitely 任务是否可以无限期地等待。
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
    TickType_t xTimeToWake;
    const TickType_t xConstTickCount = xTickCount;

    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        /* 在进入延迟列表之前，确保 ucDelayAborted 标记被重置为 pdFALSE，
           以便在任务离开 Blocked 状态时能够检测到该标记被设置为 pdTRUE。 */
        pxCurrentTCB->ucDelayAborted = pdFALSE;
    }
    #endif

    /* 将任务从就绪列表中移除，因为同一个列表项同时用于就绪列表和延迟列表。 */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        /* 当前任务必须在某个就绪列表中，因此可以直接调用端口重置宏。 */
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }
    else
    {
        /* 代码覆盖率测试标记 */
        mtCOVERAGE_TEST_MARKER();
    }

    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
        {
            /* 如果任务将无限期地等待，则将其添加到挂起任务列表中，而不是延迟列表，
               以确保它不会因时间事件而被唤醒。 */
            vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* 计算任务应该被唤醒的时间（如果事件未发生）。这可能会溢出，但内核会正确处理。 */
            xTimeToWake = xConstTickCount + xTicksToWait;

            /* 将列表项插入按唤醒时间排序的位置。 */
            listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

            if( xTimeToWake < xConstTickCount )
            {
                /* 唤醒时间发生了溢出，将此项目插入溢出列表。 */
                vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* 唤醒时间没有溢出，使用当前的延迟列表。 */
                vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

                /* 如果任务被放置在延迟任务列表的头部，则需要更新 xNextTaskUnblockTime。 */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }
                else
                {
                    /* 代码覆盖率测试标记 */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
    }
    #else /* INCLUDE_vTaskSuspend */
    {
        /* 计算任务应该被唤醒的时间（如果事件未发生）。这可能会溢出，但内核会正确处理。 */
        xTimeToWake = xConstTickCount + xTicksToWait;

        /* 将列表项插入按唤醒时间排序的位置。 */
        listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

        if( xTimeToWake < xConstTickCount )
        {
            /* 唤醒时间发生了溢出，将此项目插入溢出列表。 */
            vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* 唤醒时间没有溢出，使用当前的延迟列表。 */
            vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

            /* 如果任务被放置在延迟任务列表的头部，则需要更新 xNextTaskUnblockTime。 */
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
            else
            {
                /* 代码覆盖率测试标记 */
                mtCOVERAGE_TEST_MARKER();
            }
        }

        /* 避免在 INCLUDE_vTaskSuspend 不为 1 时产生编译器警告。 */
        ( void ) xCanBlockIndefinitely;
    }
    #endif /* INCLUDE_vTaskSuspend */
}


#ifdef FREERTOS_MODULE_TEST
	#include "tasks_test_access_functions.h"
#endif

