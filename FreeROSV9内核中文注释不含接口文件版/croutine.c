#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

/* 如果未使用协程，则移除整个文件。 */
#if( configUSE_CO_ROUTINES != 0 )


/*
 * 一些支持内核的调试器要求被查看的数据是全局的，而不是文件作用域的。
 */
#ifdef portREMOVE_STATIC_QUALIFIER
    #define static
#endif



/* 就绪和阻塞协程的列表. -------------------- */
static List_t pxReadyCoRoutineLists[ configMAX_CO_ROUTINE_PRIORITIES ];	/*< 优先级就绪协程列表. */
static List_t xDelayedCoRoutineList1;									/*< 延迟协程列表. */
static List_t xDelayedCoRoutineList2;									/*< 延迟协程列表（使用两个列表 - 一个用于超出当前滴答计数的延迟协程. */
static List_t * pxDelayedCoRoutineList;									/*< 指向当前正在使用的延迟协程列表. */
static List_t * pxOverflowDelayedCoRoutineList;							/*< 指向当前正在使用的延迟协程列表，用于保存那些超过当前滴答计数的协程. */
static List_t xPendingReadyCoRoutineList;								/*< 包含因外部事件而准备好的协程. 这些协程不能直接加入就绪列表，因为就绪列表不能被中断访问. */


/* 其他文件私有变量. ----------------------------------------------- */
CRCB_t * pxCurrentCoRoutine = NULL;              /*< 指向当前运行的协程. */
static UBaseType_t uxTopCoRoutineReadyPriority = 0;  /*< 当前就绪状态的最高优先级协程. */
static TickType_t xCoRoutineTickCount = 0;       /*< 协程滴答计数. */
static TickType_t xLastTickCount = 0;            /*< 上一次滴答计数. */
static TickType_t xPassedTicks = 0;              /*< 已经过的滴答数. */


/* 协程创建时的初始状态. */
#define corINITIAL_STATE ( 0 )


/*
 * 将由 pxCRCB 表示的协程放入相应优先级的就绪队列中
 * 它将被插入到列表的末尾。
 *
 * 此宏访问协程就绪列表，因此不应在 ISR（中断服务例程）中使用。
 */
#define prvAddCoRoutineToReadyQueue( pxCRCB )																		\
{																													\
	if( pxCRCB->uxPriority > uxTopCoRoutineReadyPriority )															\
	{																												\
		uxTopCoRoutineReadyPriority = pxCRCB->uxPriority;															\
	}																												\
	vListInsertEnd( ( List_t * ) &( pxReadyCoRoutineLists[ pxCRCB->uxPriority ] ), &( pxCRCB->xGenericListItem ) );	\
}


/*
 * 用于初始化调度器使用的所有列表的实用程序。 
 * 这将在第一个协程创建时自动调用。
 */
static void prvInitialiseCoRoutineLists( void );


/*
 * 由中断准备好的协程不能直接放入就绪列表（因为没有互斥保护）。
 * 因此，它们被放置在待就绪列表中，以便后续可以通过协程调度器移动到就绪列表。
 */
static void prvCheckPendingReadyList( void );


/*
 * 宏函数，用于检查当前延迟的协程列表，以查看是否有需要唤醒的协程。
 *
 * 协程根据其唤醒时间存储在队列中 - 这意味着一旦找到一个其定时器尚未到期的协程，
 * 我们就无需进一步检查列表中的其他项。
 */
static void prvCheckDelayedList( void );


/*-----------------------------------------------------------*/

/**
 * @brief 创建一个新的协程并初始化其控制块。
 *
 * 此函数分配内存以存储协程控制块（CRCB），并根据传入的参数设置协程的 
 * 初始状态、优先级和执行函数。协程将添加到适当的就绪列表，准备进行调度。
 *
 * @param pxCoRoutineCode 指向协程执行函数的指针。
 * @param uxPriority 协程的优先级，范围为 0 到 configMAX_CO_ROUTINE_PRIORITIES-1。
 * @param uxIndex 用于区分多个使用相同协程功能的协程的索引值。
 *
 * @return 如果成功创建协程，则返回 pdPASS；如果内存分配失败，则返回 
 * errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY。
 */
BaseType_t xCoRoutineCreate( crCOROUTINE_CODE pxCoRoutineCode, UBaseType_t uxPriority, UBaseType_t uxIndex )
{
    BaseType_t xReturn;
    CRCB_t *pxCoRoutine;

    /* 分配存储协程控制块的内存. */
    pxCoRoutine = ( CRCB_t * ) pvPortMalloc( sizeof( CRCB_t ) );
    if( pxCoRoutine )
    {
        /* 如果 pxCurrentCoRoutine 为 NULL，那么这是第一个协程的创建，
        需要初始化协程数据结构. */
        if( pxCurrentCoRoutine == NULL )
        {
            pxCurrentCoRoutine = pxCoRoutine;
            prvInitialiseCoRoutineLists(); // 初始化协程列表
        }

        /* 检查优先级是否在限制范围内. */
        if( uxPriority >= configMAX_CO_ROUTINE_PRIORITIES )
        {
            uxPriority = configMAX_CO_ROUTINE_PRIORITIES - 1; // 限制优先级到最大值
        }

        /* 根据函数参数填充协程控制块. */
        pxCoRoutine->uxState = corINITIAL_STATE; // 设置初始状态
        pxCoRoutine->uxPriority = uxPriority; // 设置优先级
        pxCoRoutine->uxIndex = uxIndex; // 设置索引
        pxCoRoutine->pxCoRoutineFunction = pxCoRoutineCode; // 设置协程函数

        /* 初始化协程控制块的其他参数. */
        vListInitialiseItem( &( pxCoRoutine->xGenericListItem ) ); // 初始化通用列表项
        vListInitialiseItem( &( pxCoRoutine->xEventListItem ) ); // 初始化事件列表项

        /* 将协程控制块设置为 ListItem_t 的所有者链接.
        这样可以从列表中的通用项返回到包含的 CRCB. */
        listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xGenericListItem ), pxCoRoutine );
        listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xEventListItem ), pxCoRoutine );

        /* 事件列表始终按照优先级顺序排列. */
        listSET_LIST_ITEM_VALUE( &( pxCoRoutine->xEventListItem ), ( ( TickType_t ) configMAX_CO_ROUTINE_PRIORITIES - ( TickType_t ) uxPriority ) );

        /* 协程初始化完成后，可以以正确的优先级将其添加到就绪列表. */
        prvAddCoRoutineToReadyQueue( pxCoRoutine );

        xReturn = pdPASS; // 返回成功
    }
    else
    {
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; // 分配内存失败
    }

    return xReturn; // 返回结果
}

/*-----------------------------------------------------------*/

/**
 * @brief 将当前协程添加到延迟列表，以便在指定的延迟之后唤醒。
 *
 * 此函数首先将当前协程从就绪列表中移除，然后将其放入延迟列表，
 * 并根据计算出的唤醒时间进行排序。若唤醒时间溢出，则放入溢出列表。
 * 该函数还可选择将协程添加到事件列表中。
 *
 * @param xTicksToDelay 指定延迟的滴答数，表示协程在此后应被唤醒。
 * @param pxEventList 指向事件列表的指针，如果不需要添加到事件列表则传入 NULL。
 *
 * @note 此函数只能在协程上下文中调用，不应在中断中调用。
 */

void vCoRoutineAddToDelayedList( TickType_t xTicksToDelay, List_t *pxEventList )
{
    TickType_t xTimeToWake;

    /* 计算唤醒时间 - 这可能会溢出，但这并不是问题. */
    xTimeToWake = xCoRoutineTickCount + xTicksToDelay;

    /* 在将自己添加到阻塞列表之前，必须先从就绪列表中移除自己，
     * 因为两个列表使用的是相同的列表项. */
    ( void ) uxListRemove( ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );

    /* 列表项将在唤醒时间顺序中插入. */
    listSET_LIST_ITEM_VALUE( &( pxCurrentCoRoutine->xGenericListItem ), xTimeToWake );

    if( xTimeToWake < xCoRoutineTickCount )
    {
        /* 唤醒时间已经溢出.
         * 将该项放入溢出列表. */
        vListInsert( ( List_t * ) pxOverflowDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
    }
    else
    {
        /* 唤醒时间没有溢出，因此我们可以使用当前的阻塞列表. */
        vListInsert( ( List_t * ) pxDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
    }

    if( pxEventList )
    {
        /* 还要将协程添加到事件列表.
         * 如果这样做，则必须在禁用中断的情况下调用此函数. */
        vListInsert( pxEventList, &( pxCurrentCoRoutine->xEventListItem ) );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief 检查待就绪列表，查看是否有任何协程需要移动到就绪列表中。
 *
 * 该函数用于处理由中断服务例程（ISR）准备好的协程。
 * 因为 ISR 不能直接访问就绪列表，所以这些协调首先在待就绪列表中。
 */
static void prvCheckPendingReadyList( void )
{
    /* 检查是否有任何协程等待被移动到就绪列表中。
       这些协程是由 ISR 准备好的，ISR 自身不能访问就绪列表. */
    while( listLIST_IS_EMPTY( &xPendingReadyCoRoutineList ) == pdFALSE )
    {
        CRCB_t *pxUnblockedCRCB;

        /* 待就绪列表可以被 ISR 访问. */
        portDISABLE_INTERRUPTS(); // 禁用中断
        {
            /* 获取待就绪列表头部的协程控制块（CRCB）. */
            pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( (&xPendingReadyCoRoutineList) );
            ( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) ); // 移除对应的事件列表项
        }
        portENABLE_INTERRUPTS(); // 恢复中断

        /* 从待就绪列表中移除协程. */
        ( void ) uxListRemove( &( pxUnblockedCRCB->xGenericListItem ) );

        /* 将该协程添加到就绪列表. */
        prvAddCoRoutineToReadyQueue( pxUnblockedCRCB );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief 检查延迟列表，查看是否有协程超时。
 *
 * 此函数负责处理延迟的协程，将那些超时的协程移动到就绪列表。
 * 它还会管理滴答计数，当滴答计数溢出时，会交换延迟列表。
 */
static void prvCheckDelayedList( void )
{
    CRCB_t *pxCRCB;

    // 计算自上次检查以来经过的滴答数
    xPassedTicks = xTaskGetTickCount() - xLastTickCount;
    
    while( xPassedTicks ) 
    {
        xCoRoutineTickCount++; // 增加当前滴答计数
        xPassedTicks--; // 减少经过的滴答数
        
        // 如果滴答计数溢出，我们需要交换延迟列表
        if( xCoRoutineTickCount == 0 ) 
        {
            List_t * pxTemp;

            // 滴答计数溢出，因此需要交换延迟列表
            // 如果 pxDelayedCoRoutineList 中还有任何项目，则有错误
            pxTemp = pxDelayedCoRoutineList;
            pxDelayedCoRoutineList = pxOverflowDelayedCoRoutineList;
            pxOverflowDelayedCoRoutineList = pxTemp;
        }

        // 查看是否有超时
        while( listLIST_IS_EMPTY( pxDelayedCoRoutineList ) == pdFALSE ) 
        {
            // 获取延迟列表头部的协程控制块
            pxCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedCoRoutineList );

            // 检查是否超时
            if( xCoRoutineTickCount < listGET_LIST_ITEM_VALUE( &( pxCRCB->xGenericListItem ) ) ) 
            {
                // 超时尚未到期，退出循环
                break;
            }

            portDISABLE_INTERRUPTS(); // 禁用中断
            {
                // 事件可能在关键区间之前发生
                // 如果是这种情况，通用列表项将已移至待就绪列表
                ( void ) uxListRemove( &( pxCRCB->xGenericListItem ) ); // 从延迟列表中移除

                // 检查协程是否也在等待事件
                if( pxCRCB->xEventListItem.pvContainer ) 
                {
                    ( void ) uxListRemove( &( pxCRCB->xEventListItem ) ); // 从事件列表中移除
                }
            }
            portENABLE_INTERRUPTS(); // 恢复中断

            // 将协程添加到就绪列表
            prvAddCoRoutineToReadyQueue( pxCRCB );
        }
    }

    // 更新上一次检查的滴答计数
    xLastTickCount = xCoRoutineTickCount;
}

/*-----------------------------------------------------------*/

/**
 * @brief 调度当前准备就绪的协程并调用其执行函数。
 *
 * 此函数检查是否有由事件准备的协程需要移动到就绪列表，并
 * 检查是否有延迟的协程超时。然后根据优先级找到最高优先级的
 * 就绪协程，并调用其对应的执行函数。
 */
void vCoRoutineSchedule( void )
{
    /* 检查是否有由事件准备好的协程需要移动到就绪列表中. */
    prvCheckPendingReadyList();

    /* 检查是否有延迟的协程已超时. */
    prvCheckDelayedList();

    /* 找到包含就绪协程的最高优先级队列. */
    while( listLIST_IS_EMPTY( &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) ) )
    {
        if( uxTopCoRoutineReadyPriority == 0 )
        {
            /* 如果没有更多的协程可供检查，返回. */
            return;
        }
        --uxTopCoRoutineReadyPriority; // 降低优先级以检查下一个优先级
    }

    /* listGET_OWNER_OF_NEXT_ENTRY 遍历列表，
       确保相同优先级的协程能公平共享处理器时间. */
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentCoRoutine, &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) );

    /* 调用协程的执行函数. */
    ( pxCurrentCoRoutine->pxCoRoutineFunction )( pxCurrentCoRoutine, pxCurrentCoRoutine->uxIndex );

    return;
}

/*-----------------------------------------------------------*/

/**
 * @brief 初始化协程管理所需的各种列表。
 *
 * 此函数负责为协程调度器创建和初始化必要的列表，包括
 * 就绪协程列表和延迟协程列表。它运行时将所有优先级的
 * 就绪列表和延迟列表进行初始化，以确保协程管理的正确性。
 */
static void prvInitialiseCoRoutineLists( void )
{
    UBaseType_t uxPriority;

    // 初始化所有优先级的就绪协程列表
    for( uxPriority = 0; uxPriority < configMAX_CO_ROUTINE_PRIORITIES; uxPriority++ )
    {
        vListInitialise( ( List_t * ) &( pxReadyCoRoutineLists[ uxPriority ] ) ); // 初始化特定优先级的就绪列表
    }

    // 初始化延迟协程列表
    vListInitialise( ( List_t * ) &xDelayedCoRoutineList1 ); // 初始化第一个延迟列表
    vListInitialise( ( List_t * ) &xDelayedCoRoutineList2 ); // 初始化第二个延迟列表
    vListInitialise( ( List_t * ) &xPendingReadyCoRoutineList ); // 初始化待就绪列表

    /* 初始时使用 list1 作为 pxDelayedCoRoutineList，
       使用 list2 作为 pxOverflowDelayedCoRoutineList. */
    pxDelayedCoRoutineList = &xDelayedCoRoutineList1; // 指向当前延迟列表1
    pxOverflowDelayedCoRoutineList = &xDelayedCoRoutineList2; // 指向当前延迟列表2
}

/*-----------------------------------------------------------*/

/**
 * @brief 从事件列表中移除协程，并将其添加到待就绪列表。
 *
 * 此函数用于处理从事件列表移除的协程并将其放入待就绪列表。
 * 该函数通常在中断服务例程中调用，只能访问事件列表和待就绪列表。
 *
 * @param pxEventList 指向事件列表的指针，协程从中移除。
 * @return 如果被移除的协程的优先级高于或等于当前协程的优先级，则返回 pdTRUE；否则返回 pdFALSE。
 */
BaseType_t xCoRoutineRemoveFromEventList( const List_t *pxEventList )
{
    CRCB_t *pxUnblockedCRCB;
    BaseType_t xReturn;

    /* 该函数在中断内部被调用，只能访问事件列表和待就绪列表。
       本函数假设已检查 pxEventList 不为空. */
    pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList ); // 获取事件列表头部的协程控制块
    ( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) ); // 从事件列表中移除该协程的事件列表项
    vListInsertEnd( ( List_t * ) &( xPendingReadyCoRoutineList ), &( pxUnblockedCRCB->xEventListItem ) ); // 将其放入待就绪列表

    /* 检查优先级并返回结果. */
    if( pxUnblockedCRCB->uxPriority >= pxCurrentCoRoutine->uxPriority )
    {
        xReturn = pdTRUE; // 返回 pdTRUE 表示优先级较高
    }
    else
    {
        xReturn = pdFALSE; // 返回 pdFALSE 表示优先级较低
    }

    return xReturn; // 返回优先级比较的结果
}


#endif /* configUSE_CO_ROUTINES == 0 */

