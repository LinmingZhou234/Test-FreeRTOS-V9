/* 标准库包含。 */
#include <stdlib.h>

/* 定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE 防止 task.h 重新定义
所有 API 函数以使用 MPU 包装器。这仅应在 task.h 从应用程序文件中包含时发生。 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS 包含。 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "event_groups.h"

/* 抑制 lint 工具的 e961 和 e750 警告，因为根据 MISRA C 标准，这些警告在此处不适用。
MPU 端口需要在上面的头文件中定义 MPU_WRAPPERS_INCLUDED_FROM_API_FILE，
而不是在此文件中，以便生成正确的特权与非特权链接和放置。 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* 以下位字段在任务的事件列表项值中传达控制信息。重要的是它们不能与
taskEVENT_LIST_ITEM_VALUE_IN_USE 定义冲突。 */
#if configUSE_16_BIT_TICKS == 1
    /* 定义清除退出时的事件标志 */
    #define eventCLEAR_EVENTS_ON_EXIT_BIT  0x0100U
    /* 定义由于设置了位而解除阻塞的标志 */
    #define eventUNBLOCKED_DUE_TO_BIT_SET  0x0200U
    /* 定义等待所有位都被设置的标志 */
    #define eventWAIT_FOR_ALL_BITS         0x0400U
    /* 定义事件位控制字节掩码 */
    #define eventEVENT_BITS_CONTROL_BYTES  0xff00U
#else
    /* 定义清除退出时的事件标志 */
    #define eventCLEAR_EVENTS_ON_EXIT_BIT  0x01000000UL
    /* 定义由于设置了位而解除阻塞的标志 */
    #define eventUNBLOCKED_DUE_TO_BIT_SET  0x02000000UL
    /* 定义等待所有位都被设置的标志 */
    #define eventWAIT_FOR_ALL_BITS         0x04000000UL
    /* 定义事件位控制字节掩码 */
    #define eventEVENT_BITS_CONTROL_BYTES  0xff000000UL
#endif

/* 事件组定义结构体 */
typedef struct xEventGroupDefinition
{
    EventBits_t uxEventBits;          /*< 当前设置的事件位 */
    List_t xTasksWaitingForBits;      /*< 等待特定位被设置的任务列表 */

    #if( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxEventGroupNumber; /*< 用于跟踪的事件组编号 */
    #endif

    #if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated;  /*< 如果事件组是静态分配的，则设置为 pdTRUE，以确保不会尝试释放内存 */
    #endif
} EventGroup_t;

/*-----------------------------------------------------------*/

/*
 * 测试 uxCurrentEventBits 中设置的位，以查看是否满足等待条件。
 * 等待条件由 xWaitForAllBits 定义。如果 xWaitForAllBits 为 pdTRUE，则等待条件是在 uxBitsToWaitFor 中设置的所有位也都在 uxCurrentEventBits 中设置时满足。
 * 如果 xWaitForAllBits 为 pdFALSE，则等待条件是在 uxBitsToWaitFor 中设置的任何位也在 uxCurrentEventBits 中设置时满足。
 */
static BaseType_t prvTestWaitCondition(const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

/**
 * @brief 静态创建一个事件组
 *
 * 此函数用于静态创建一个事件组对象。如果配置允许静态分配，则使用提供的静态缓冲区来初始化事件组。
 *
 * @param[in] pxEventGroupBuffer 指向用于静态分配的事件组缓冲区的指针。
 *
 * @return 返回新创建的事件组的句柄。如果创建失败，则返回 NULL。
 */
EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t *pxEventGroupBuffer)
{
    EventGroup_t *pxEventBits;

    /* 必须提供一个 StaticEventGroup_t 对象。 */
    configASSERT(pxEventGroupBuffer);

    /* 用户已经提供了一个静态分配的事件组 - 使用它。 */
    pxEventBits = (EventGroup_t *)pxEventGroupBuffer; /*lint !e740 EventGroup_t 和 StaticEventGroup_t 保证具有相同的大小和对齐要求 - 由 configASSERT() 检查。 */

    if (pxEventBits != NULL)
    {
        /* 初始化事件位为 0。 */
        pxEventBits->uxEventBits = 0;
        /* 初始化等待位的任务列表。 */
        vListInitialise(&(pxEventBits->xTasksWaitingForBits));

        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* 允许同时使用静态和动态分配，因此在删除事件组时注意该事件组是静态创建的。 */
            pxEventBits->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* 记录事件组创建。 */
        traceEVENT_GROUP_CREATE(pxEventBits);
    }
    else
    {
        /* 记录事件组创建失败。 */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    return (EventGroupHandle_t)pxEventBits;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

/**
 * @brief 创建一个事件组
 *
 * 此函数用于动态分配内存并创建一个新的事件组。事件组用于在任务之间进行同步和通信。
 *
 * @return EventGroupHandle_t 返回事件组的句柄。如果创建失败，则返回 NULL。
 */
EventGroupHandle_t xEventGroupCreate( void )
{
    EventGroup_t *pxEventBits;

    /* 申请事件组的内存 */
    pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) );

    if( pxEventBits != NULL )
    {
        /* 初始化事件位 */
        pxEventBits->uxEventBits = 0;
        /* 初始化等待事件位的任务列表 */
        vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* 支持静态分配，记录此事件组是否为静态分配 */
            pxEventBits->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* 记录事件组创建的跟踪信息 */
        traceEVENT_GROUP_CREATE( pxEventBits );
    }
    else
    {
        /* 记录事件组创建失败的跟踪信息 */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    /* 返回事件组的句柄 */
    return ( EventGroupHandle_t ) pxEventBits;
}


#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/**
 * @brief 同步事件组中的事件
 *
 * 此函数用于设置和等待事件组中的事件位。如果在指定的时间内所有等待的位都被设置，则任务会立即恢复执行；
 * 否则，任务将进入阻塞状态直到事件位被设置或超时。
 *
 * @param[in] xEventGroup 事件组句柄。
 * @param[in] uxBitsToSet 需要设置的事件位。
 * @param[in] uxBitsToWaitFor 需要等待的事件位。
 * @param[in] xTicksToWait 等待的时间（滴答数）。
 *
 * @return 返回当前设置的事件位。
 */
EventBits_t xEventGroupSync(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait)
{
    EventBits_t uxOriginalBitValue, uxReturn;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    BaseType_t xAlreadyYielded;
    BaseType_t xTimeoutOccurred = pdFALSE;

    // 断言确保等待的位没有控制位，并且至少有一个位需要等待。
    configASSERT((uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES) == 0);
    configASSERT(uxBitsToWaitFor != 0);

    #if (INCLUDE_xTaskGetSchedulerState == 1) || (configUSE_TIMERS == 1)
    {
        // 断言确保调度器未暂停，除非等待时间为 0。
        configASSERT(!( (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED) && (xTicksToWait != 0) ));
    }
    #endif

    // 暂停所有任务调度。
    vTaskSuspendAll();
    {
        uxOriginalBitValue = pxEventBits->uxEventBits;

        // 设置事件位。
        (void)xEventGroupSetBits(xEventGroup, uxBitsToSet);

        if (((uxOriginalBitValue | uxBitsToSet) & uxBitsToWaitFor) == uxBitsToWaitFor)
        {
            // 所需的位均已设置 - 不需要阻塞。
            uxReturn = (uxOriginalBitValue | uxBitsToSet);

            // 清除等待的位（除非这是唯一的任务）。
            pxEventBits->uxEventBits &= ~uxBitsToWaitFor;

            xTicksToWait = 0;
        }
        else
        {
            if (xTicksToWait != (TickType_t)0)
            {
                // 记录同步阻塞。
                traceEVENT_GROUP_SYNC_BLOCK(xEventGroup, uxBitsToSet, uxBitsToWaitFor);

                // 存储等待的位，并进入阻塞状态。
                vTaskPlaceOnUnorderedEventList(&(pxEventBits->xTasksWaitingForBits), (uxBitsToWaitFor | eventCLEAR_EVENTS_ON_EXIT_BIT | eventWAIT_FOR_ALL_BITS), xTicksToWait);

                // 初始化返回值。
                uxReturn = 0;
            }
            else
            {
                // 所需的位未设置，且没有指定等待时间 - 直接返回当前事件位。
                uxReturn = pxEventBits->uxEventBits;
            }
        }
    }

    // 恢复任务调度。
    xAlreadyYielded = xTaskResumeAll();

    if (xTicksToWait != (TickType_t)0)
    {
        if (xAlreadyYielded == pdFALSE)
        {
            portYIELD_WITHIN_API();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        // 获取并清除设置的位。
        uxReturn = uxTaskResetEventItemValue();

        if ((uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET) == (EventBits_t)0)
        {
            // 任务超时，返回当前事件位。
            taskENTER_CRITICAL();
            {
                uxReturn = pxEventBits->uxEventBits;

                // 如果位在解阻塞后被设置，则需要清除位。
                if ((uxReturn & uxBitsToWaitFor) == uxBitsToWaitFor)
                {
                    pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            taskEXIT_CRITICAL();

            xTimeoutOccurred = pdTRUE;
        }
        else
        {
            // 位被设置，任务解阻塞。
        }

        // 清除控制位。
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }

    // 记录同步结束。
    traceEVENT_GROUP_SYNC_END(xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred);

    return uxReturn;
}
/*-----------------------------------------------------------*/

/*
 * @brief xEventGroupWaitBits 等待事件组中的指定位被设置。
 *
 * 此函数用于等待事件组中的指定位被设置，并在满足条件时返回。
 *
 * @param xEventGroup 事件组句柄。
 * @param uxBitsToWaitFor 要等待的事件位。
 * @param xClearOnExit 如果设置为 pdTRUE，则在返回之前清除等待的位。
 * @param xWaitForAllBits 如果设置为 pdTRUE，则等待所有指定的位都被设置；否则，只要有一个位被设置就返回。
 * @param xTicksToWait 等待的时间（以滴答为单位）。
 *
 * @return 返回当前事件组中的事件位。如果等待超时，则返回当前事件位的值。
 */
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToWaitFor,
    const BaseType_t xClearOnExit,
    const BaseType_t xWaitForAllBits,
    TickType_t xTicksToWait )
{
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup; // 获取事件组的地址
    EventBits_t uxReturn, uxControlBits = 0;                 // 初始化返回值和控制位
    BaseType_t xWaitConditionMet, xAlreadyYielded;           // 初始化等待条件和是否已经切换标志
    BaseType_t xTimeoutOccurred = pdFALSE;                   // 初始化超时标志

    /* 检查用户没有尝试等待内核使用的位，并且至少请求了一个位。 */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
    configASSERT( uxBitsToWaitFor != 0 );

    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* 如果调度器被暂停并且等待时间不为零，则断言失败。 */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* 暂停所有任务调度。 */
    vTaskSuspendAll();
    {
        const EventBits_t uxCurrentEventBits = pxEventBits->uxEventBits; // 获取当前事件位

        /* 检查等待条件是否已经满足。 */
        xWaitConditionMet = prvTestWaitCondition( uxCurrentEventBits, uxBitsToWaitFor, xWaitForAllBits );

        if( xWaitConditionMet != pdFALSE )
        {
            /* 等待条件已经满足，无需阻塞。 */
            uxReturn = uxCurrentEventBits;
            xTicksToWait = (TickType_t)0;

            /* 如果请求清除位，则清除等待的位。 */
            if( xClearOnExit != pdFALSE )
            {
                pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else if( xTicksToWait == (TickType_t)0 )
        {
            /* 等待条件没有满足，并且没有指定阻塞时间，直接返回当前值。 */
            uxReturn = uxCurrentEventBits;
        }
        else
        {
            /* 任务将阻塞以等待所需的位被设置。 */

            /* 设置控制位，记录此调用的行为。 */
            if( xClearOnExit != pdFALSE )
            {
                uxControlBits |= eventCLEAR_EVENTS_ON_EXIT_BIT;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            if( xWaitForAllBits != pdFALSE )
            {
                uxControlBits |= eventWAIT_FOR_ALL_BITS;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            /* 存储等待位，并进入阻塞状态。 */
            vTaskPlaceOnUnorderedEventList( &(pxEventBits->xTasksWaitingForBits), (uxBitsToWaitFor | uxControlBits), xTicksToWait );

            /* 保留返回值，防止编译器警告。 */
            uxReturn = 0;

            /* 记录等待阻塞。 */
            traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor );
        }
    }

    /* 恢复任务调度。 */
    xAlreadyYielded = xTaskResumeAll();

    if( xTicksToWait != (TickType_t)0 )
    {
        if( xAlreadyYielded == pdFALSE )
        {
            portYIELD_WITHIN_API(); // 强制进行调度
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        /* 获取设置的位。 */
        uxReturn = uxTaskResetEventItemValue();

        if( (uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET) == (EventBits_t)0 )
        {
            /* 任务超时，返回当前事件位的值。 */
            taskENTER_CRITICAL();
            {
                uxReturn = pxEventBits->uxEventBits;

                /* 如果等待条件满足，则清除等待的位。 */
                if( prvTestWaitCondition( uxReturn, uxBitsToWaitFor, xWaitForAllBits ) != pdFALSE )
                {
                    if( xClearOnExit != pdFALSE )
                    {
                        pxEventBits->uxEventBits &= ~uxBitsToWaitFor;
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
            taskEXIT_CRITICAL();

            /* 标记超时发生。 */
            xTimeoutOccurred = pdFALSE;
        }
        else
        {
            /* 位已被设置，任务解除阻塞。 */
        }

        /* 清除控制位。 */
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }

    /* 记录等待结束。 */
    traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred );

    return uxReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 清除事件组中的指定事件位
 *
 * 此函数用于清除事件组中指定的事件位。函数返回清除位之前的事件组值。
 *
 * @param[in] xEventGroup 事件组句柄。
 * @param[in] uxBitsToClear 需要清除的事件位。
 *
 * @return 返回清除位之前的事件组值。
 */
EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    EventBits_t uxReturn;

    /* 检查用户是否试图清除内核自身使用的位。 */
    configASSERT(xEventGroup); // 确保事件组句柄非空。
    configASSERT((uxBitsToClear & eventEVENT_BITS_CONTROL_BYTES) == 0); // 确保清除的位中不包含控制位。

    taskENTER_CRITICAL(); // 进入临界区，确保操作的原子性。
    {
        traceEVENT_GROUP_CLEAR_BITS(xEventGroup, uxBitsToClear); // 记录清除位的操作。

        /* 在清除位之前返回事件组的值。 */
        uxReturn = pxEventBits->uxEventBits;

        /* 清除指定的位。 */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    taskEXIT_CRITICAL(); // 退出临界区。

    return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

/**
 * @brief 从中断服务程序清除事件组中的指定事件位
 *
 * 此函数用于从中断服务程序清除事件组中指定的事件位。函数通过调用一个回调函数来异步地清除事件位。
 * 
 * @param[in] xEventGroup 事件组句柄。
 * @param[in] uxBitsToClear 需要清除的事件位。
 *
 * @return 返回值表示是否成功安排了清除操作。
 *         pdPASS 表示成功安排了清除操作。
 *         pdFAIL 表示未能安排清除操作。
 */
BaseType_t xEventGroupClearBitsFromISR(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    BaseType_t xReturn;

    // 记录从中断服务程序清除位的操作。
    traceEVENT_GROUP_CLEAR_BITS_FROM_ISR(xEventGroup, uxBitsToClear);

    // 通过中断安全的方式安排异步清除事件位。
    // 参数：回调函数、事件组句柄、需要清除的位、回调上下文（这里为NULL）。
    xReturn = xTimerPendFunctionCallFromISR(vEventGroupClearBitsCallback,
                                            (void *)xEventGroup,
                                            (uint32_t)uxBitsToClear,
                                            NULL);

    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

/**
 * @brief 从中断服务程序获取事件组中的事件位
 *
 * 此函数用于从中断服务程序获取事件组中的当前事件位值。
 *
 * @param[in] xEventGroup 事件组句柄。
 *
 * @return 返回事件组中的当前事件位值。
 */
EventBits_t xEventGroupGetBitsFromISR(EventGroupHandle_t xEventGroup)
{
    UBaseType_t uxSavedInterruptStatus;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    EventBits_t uxReturn;

    // 保存当前中断状态，并禁用中断。
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        // 获取事件组中的当前事件位值。
        uxReturn = pxEventBits->uxEventBits;
    }
    // 恢复中断状态。
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    return uxReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief 设置事件组中的事件位
 *
 * 此函数用于设置事件组中的指定事件位，并检查是否有等待这些位的任务可以被解除阻塞。
 *
 * @param[in] xEventGroup 事件组句柄。
 * @param[in] uxBitsToSet 需要设置的事件位。
 *
 * @return 返回事件组中的当前事件位值。
 */
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet)
{
    ListItem_t *pxListItem, *pxNext;
    ListItem_t const *pxListEnd;
    List_t *pxList;
    EventBits_t uxBitsToClear = 0, uxBitsWaitedFor, uxControlBits;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    BaseType_t xMatchFound = pdFALSE;

    /* 检查用户是否试图设置内核自身使用的位。 */
    configASSERT(xEventGroup); // 确保事件组句柄非空。
    configASSERT((uxBitsToSet & eventEVENT_BITS_CONTROL_BYTES) == 0); // 确保设置的位中不包含控制位。

    pxList = &(pxEventBits->xTasksWaitingForBits);
    pxListEnd = listGET_END_MARKER(pxList); /*lint !e826 !e740 使用 mini list 结构作为列表结束标记以节省 RAM。这是有效的。*/
    vTaskSuspendAll(); // 暂停所有任务调度。
    {
        traceEVENT_GROUP_SET_BITS(xEventGroup, uxBitsToSet); // 记录设置位的操作。

        pxListItem = listGET_HEAD_ENTRY(pxList);

        /* 设置指定的位。 */
        pxEventBits->uxEventBits |= uxBitsToSet;

        /* 检查新的位值是否应该解除任何任务的阻塞。 */
        while (pxListItem != pxListEnd)
        {
            pxNext = listGET_NEXT(pxListItem);
            uxBitsWaitedFor = listGET_LIST_ITEM_VALUE(pxListItem);
            xMatchFound = pdFALSE;

            /* 分离等待的位和控制位。 */
            uxControlBits = uxBitsWaitedFor & eventEVENT_BITS_CONTROL_BYTES;
            uxBitsWaitedFor &= ~eventEVENT_BITS_CONTROL_BYTES;

            if ((uxControlBits & eventWAIT_FOR_ALL_BITS) == (EventBits_t)0)
            {
                /* 只寻找单个位被设置。 */
                if ((uxBitsWaitedFor & pxEventBits->uxEventBits) != (EventBits_t)0)
                {
                    xMatchFound = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else if ((uxBitsWaitedFor & pxEventBits->uxEventBits) == uxBitsWaitedFor)
            {
                /* 所有位都已设置。 */
                xMatchFound = pdTRUE;
            }
            else
            {
                /* 需要所有位都设置，但不是所有的位都已设置。 */
            }

            if (xMatchFound != pdFALSE)
            {
                /* 位匹配。在退出时是否应清除这些位？ */
                if ((uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT) != (EventBits_t)0)
                {
                    uxBitsToClear |= uxBitsWaitedFor;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                /* 在从事件列表移除任务之前，将实际的事件标志值存储在任务的事件列表项中，
                   并设置 eventUNBLOCKED_DUE_TO_BIT_SET 标志，以便任务知道是因为其所需的位匹配而被解除阻塞，而不是因为超时。 */
                (void)xTaskRemoveFromUnorderedEventList(pxListItem, pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET);
            }

            /* 移动到下一个列表项。注意 pxListItem->pxNext 在这里不使用，
               因为列表项可能已被从事件列表移除并插入到就绪/等待读取列表中。 */
            pxListItem = pxNext;
        }

        /* 清除在控制字中设置了 eventCLEAR_EVENTS_ON_EXIT_BIT 时匹配的任何位。 */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    (void)xTaskResumeAll(); // 恢复任务调度。

    return pxEventBits->uxEventBits; // 返回事件组中的当前事件位值。
}
/*-----------------------------------------------------------*/

/**
 * @brief 删除事件组
 *
 * 此函数用于删除一个事件组，并释放相关联的内存。
 * 如果事件组中还有等待中的任务，则会解除这些任务的阻塞。
 *
 * @param[in] xEventGroup 要删除的事件组句柄。
 */
void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    const List_t *pxTasksWaitingForBits = &(pxEventBits->xTasksWaitingForBits);

    vTaskSuspendAll(); // 暂停所有任务调度。
    {
        traceEVENT_GROUP_DELETE(xEventGroup); // 记录删除事件组的操作。

        while (listCURRENT_LIST_LENGTH(pxTasksWaitingForBits) > (UBaseType_t)0)
        {
            /* 解除任务的阻塞，返回 0 表示事件列表正在被删除，因此不能有任何位被设置。 */
            configASSERT(pxTasksWaitingForBits->xListEnd.pxNext != (ListItem_t *)&(pxTasksWaitingForBits->xListEnd));
            (void)xTaskRemoveFromUnorderedEventList(pxTasksWaitingForBits->xListEnd.pxNext, eventUNBLOCKED_DUE_TO_BIT_SET);
        }

        #if (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 0)
        {
            /* 事件组只能是动态分配的 - 释放内存。 */
            vPortFree(pxEventBits);
        }
        #elif (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 1)
        {
            /* 事件组可能是静态或动态分配的，所以在尝试释放内存前进行检查。 */
            if (pxEventBits->ucStaticallyAllocated == (uint8_t)pdFALSE)
            {
                vPortFree(pxEventBits);
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }
    (void)xTaskResumeAll(); // 恢复任务调度。
}
/*-----------------------------------------------------------*/

/**
 * @brief 内部使用 - 执行一个从中断挂起的“设置位”命令
 *
 * 此函数用于内部处理从中断服务程序挂起的“设置位”命令。
 *
 * @param[in] pvEventGroup 事件组句柄。
 * @param[in] ulBitsToSet 需要设置的事件位。
 */
void vEventGroupSetBitsCallback(void *pvEventGroup, const uint32_t ulBitsToSet)
{
    // 将事件位设置操作委托给 xEventGroupSetBits 函数。
    (void)xEventGroupSetBits(pvEventGroup, (EventBits_t)ulBitsToSet);
}
/*-----------------------------------------------------------*/

/**
 * @brief 内部使用 - 执行一个从中断挂起的“清除位”命令
 *
 * 此函数用于内部处理从中断服务程序挂起的“清除位”命令。
 *
 * @param[in] pvEventGroup 事件组句柄。
 * @param[in] ulBitsToClear 需要清除的事件位。
 */
void vEventGroupClearBitsCallback(void *pvEventGroup, const uint32_t ulBitsToClear)
{
    // 调用 xEventGroupClearBits 函数来清除事件组中的指定事件位。
    (void)xEventGroupClearBits(pvEventGroup, (EventBits_t)ulBitsToClear);
}
/*-----------------------------------------------------------*/

/**
 * @brief 静态内部函数 - 测试等待条件
 *
 * 此函数用于测试事件组中的当前事件位是否满足等待条件。
 * 根据 `xWaitForAllBits` 参数的不同，可以等待一个或多个特定的位被设置。
 *
 * @param[in] uxCurrentEventBits 当前事件组中的事件位。
 * @param[in] uxBitsToWaitFor 需要等待的事件位。
 * @param[in] xWaitForAllBits 指定是否需要等待所有位被设置。
 *
 * @return 返回值表示等待条件是否满足。
 *         pdTRUE 表示条件满足。
 *         pdFALSE 表示条件不满足。
 */
static BaseType_t prvTestWaitCondition(const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits)
{
    BaseType_t xWaitConditionMet = pdFALSE;

    if (xWaitForAllBits == pdFALSE)
    {
        /* 任务只需要等待 uxBitsToWaitFor 中的一个位被设置。
           当前已有位被设置吗？ */
        if ((uxCurrentEventBits & uxBitsToWaitFor) != (EventBits_t)0)
        {
            xWaitConditionMet = pdTRUE;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        /* 任务需要等待 uxBitsToWaitFor 中的所有位被设置。
           所有位都已经设置了吗？ */
        if ((uxCurrentEventBits & uxBitsToWaitFor) == uxBitsToWaitFor)
        {
            xWaitConditionMet = pdTRUE;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    return xWaitConditionMet;
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

/**
 * @brief 从中断服务程序设置事件组中的事件位
 *
 * 此函数用于从中断服务程序设置事件组中的指定事件位。
 * 该函数通过调用一个回调函数来异步地设置事件位。
 *
 * @param[in] xEventGroup 事件组句柄。
 * @param[in] uxBitsToSet 需要设置的事件位。
 * @param[out] pxHigherPriorityTaskWoken 指向一个变量，该变量用来指示是否有更高优先级的任务被唤醒。
 *
 * @return 返回值表示是否成功安排了设置操作。
 *         pdPASS 表示成功安排了设置操作。
 *         pdFAIL 表示未能安排设置操作。
 */
BaseType_t xEventGroupSetBitsFromISR(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t xReturn;

    // 记录从中断服务程序设置位的操作。
    traceEVENT_GROUP_SET_BITS_FROM_ISR(xEventGroup, uxBitsToSet);

    // 通过中断安全的方式安排异步设置事件位。
    // 参数：回调函数、事件组句柄、需要设置的位、更高优先级任务被唤醒的指示变量。
    xReturn = xTimerPendFunctionCallFromISR(vEventGroupSetBitsCallback,
                                            (void *)xEventGroup,
                                            (uint32_t)uxBitsToSet,
                                            pxHigherPriorityTaskWoken);

    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief 获取事件组的编号
 *
 * 此函数用于获取指定事件组的编号。
 *
 * @param[in] xEventGroup 事件组句柄。
 *
 * @return 返回事件组的编号。
 *         如果事件组句柄为 NULL，则返回 0。
 */
UBaseType_t uxEventGroupGetNumber(void *xEventGroup)
{
    UBaseType_t xReturn;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;

    if (xEventGroup == NULL)
    {
        // 如果事件组句柄为 NULL，则返回 0。
        xReturn = 0;
    }
    else
    {
        // 获取事件组的编号。
        xReturn = pxEventBits->uxEventGroupNumber;
    }

    return xReturn;
}

#endif

