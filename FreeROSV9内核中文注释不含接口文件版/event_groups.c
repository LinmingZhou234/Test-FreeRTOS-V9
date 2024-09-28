/* ��׼������� */
#include <stdlib.h>

/* ���� MPU_WRAPPERS_INCLUDED_FROM_API_FILE ��ֹ task.h ���¶���
���� API ������ʹ�� MPU ��װ�������Ӧ�� task.h ��Ӧ�ó����ļ��а���ʱ������ */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS ������ */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "event_groups.h"

/* ���� lint ���ߵ� e961 �� e750 ���棬��Ϊ���� MISRA C ��׼����Щ�����ڴ˴������á�
MPU �˿���Ҫ�������ͷ�ļ��ж��� MPU_WRAPPERS_INCLUDED_FROM_API_FILE��
�������ڴ��ļ��У��Ա�������ȷ����Ȩ�����Ȩ���Ӻͷ��á� */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* ����λ�ֶ���������¼��б���ֵ�д��������Ϣ����Ҫ�������ǲ�����
taskEVENT_LIST_ITEM_VALUE_IN_USE �����ͻ�� */
#if configUSE_16_BIT_TICKS == 1
    /* ��������˳�ʱ���¼���־ */
    #define eventCLEAR_EVENTS_ON_EXIT_BIT  0x0100U
    /* ��������������λ����������ı�־ */
    #define eventUNBLOCKED_DUE_TO_BIT_SET  0x0200U
    /* ����ȴ�����λ�������õı�־ */
    #define eventWAIT_FOR_ALL_BITS         0x0400U
    /* �����¼�λ�����ֽ����� */
    #define eventEVENT_BITS_CONTROL_BYTES  0xff00U
#else
    /* ��������˳�ʱ���¼���־ */
    #define eventCLEAR_EVENTS_ON_EXIT_BIT  0x01000000UL
    /* ��������������λ����������ı�־ */
    #define eventUNBLOCKED_DUE_TO_BIT_SET  0x02000000UL
    /* ����ȴ�����λ�������õı�־ */
    #define eventWAIT_FOR_ALL_BITS         0x04000000UL
    /* �����¼�λ�����ֽ����� */
    #define eventEVENT_BITS_CONTROL_BYTES  0xff000000UL
#endif

/* �¼��鶨��ṹ�� */
typedef struct xEventGroupDefinition
{
    EventBits_t uxEventBits;          /*< ��ǰ���õ��¼�λ */
    List_t xTasksWaitingForBits;      /*< �ȴ��ض�λ�����õ������б� */

    #if( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxEventGroupNumber; /*< ���ڸ��ٵ��¼����� */
    #endif

    #if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated;  /*< ����¼����Ǿ�̬����ģ�������Ϊ pdTRUE����ȷ�����᳢���ͷ��ڴ� */
    #endif
} EventGroup_t;

/*-----------------------------------------------------------*/

/*
 * ���� uxCurrentEventBits �����õ�λ���Բ鿴�Ƿ�����ȴ�������
 * �ȴ������� xWaitForAllBits ���塣��� xWaitForAllBits Ϊ pdTRUE����ȴ��������� uxBitsToWaitFor �����õ�����λҲ���� uxCurrentEventBits ������ʱ���㡣
 * ��� xWaitForAllBits Ϊ pdFALSE����ȴ��������� uxBitsToWaitFor �����õ��κ�λҲ�� uxCurrentEventBits ������ʱ���㡣
 */
static BaseType_t prvTestWaitCondition(const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

/**
 * @brief ��̬����һ���¼���
 *
 * �˺������ھ�̬����һ���¼�����������������̬���䣬��ʹ���ṩ�ľ�̬����������ʼ���¼��顣
 *
 * @param[in] pxEventGroupBuffer ָ�����ھ�̬������¼��黺������ָ�롣
 *
 * @return �����´������¼���ľ�����������ʧ�ܣ��򷵻� NULL��
 */
EventGroupHandle_t xEventGroupCreateStatic(StaticEventGroup_t *pxEventGroupBuffer)
{
    EventGroup_t *pxEventBits;

    /* �����ṩһ�� StaticEventGroup_t ���� */
    configASSERT(pxEventGroupBuffer);

    /* �û��Ѿ��ṩ��һ����̬������¼��� - ʹ������ */
    pxEventBits = (EventGroup_t *)pxEventGroupBuffer; /*lint !e740 EventGroup_t �� StaticEventGroup_t ��֤������ͬ�Ĵ�С�Ͷ���Ҫ�� - �� configASSERT() ��顣 */

    if (pxEventBits != NULL)
    {
        /* ��ʼ���¼�λΪ 0�� */
        pxEventBits->uxEventBits = 0;
        /* ��ʼ���ȴ�λ�������б� */
        vListInitialise(&(pxEventBits->xTasksWaitingForBits));

        #if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
        {
            /* ����ͬʱʹ�þ�̬�Ͷ�̬���䣬�����ɾ���¼���ʱע����¼����Ǿ�̬�����ġ� */
            pxEventBits->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* ��¼�¼��鴴���� */
        traceEVENT_GROUP_CREATE(pxEventBits);
    }
    else
    {
        /* ��¼�¼��鴴��ʧ�ܡ� */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    return (EventGroupHandle_t)pxEventBits;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

/**
 * @brief ����һ���¼���
 *
 * �˺������ڶ�̬�����ڴ沢����һ���µ��¼��顣�¼�������������֮�����ͬ����ͨ�š�
 *
 * @return EventGroupHandle_t �����¼���ľ�����������ʧ�ܣ��򷵻� NULL��
 */
EventGroupHandle_t xEventGroupCreate( void )
{
    EventGroup_t *pxEventBits;

    /* �����¼�����ڴ� */
    pxEventBits = ( EventGroup_t * ) pvPortMalloc( sizeof( EventGroup_t ) );

    if( pxEventBits != NULL )
    {
        /* ��ʼ���¼�λ */
        pxEventBits->uxEventBits = 0;
        /* ��ʼ���ȴ��¼�λ�������б� */
        vListInitialise( &( pxEventBits->xTasksWaitingForBits ) );

        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            /* ֧�־�̬���䣬��¼���¼����Ƿ�Ϊ��̬���� */
            pxEventBits->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        /* ��¼�¼��鴴���ĸ�����Ϣ */
        traceEVENT_GROUP_CREATE( pxEventBits );
    }
    else
    {
        /* ��¼�¼��鴴��ʧ�ܵĸ�����Ϣ */
        traceEVENT_GROUP_CREATE_FAILED();
    }

    /* �����¼���ľ�� */
    return ( EventGroupHandle_t ) pxEventBits;
}


#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/**
 * @brief ͬ���¼����е��¼�
 *
 * �˺����������ú͵ȴ��¼����е��¼�λ�������ָ����ʱ�������еȴ���λ�������ã�������������ָ�ִ�У�
 * �������񽫽�������״ֱ̬���¼�λ�����û�ʱ��
 *
 * @param[in] xEventGroup �¼�������
 * @param[in] uxBitsToSet ��Ҫ���õ��¼�λ��
 * @param[in] uxBitsToWaitFor ��Ҫ�ȴ����¼�λ��
 * @param[in] xTicksToWait �ȴ���ʱ�䣨�δ�������
 *
 * @return ���ص�ǰ���õ��¼�λ��
 */
EventBits_t xEventGroupSync(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, const EventBits_t uxBitsToWaitFor, TickType_t xTicksToWait)
{
    EventBits_t uxOriginalBitValue, uxReturn;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    BaseType_t xAlreadyYielded;
    BaseType_t xTimeoutOccurred = pdFALSE;

    // ����ȷ���ȴ���λû�п���λ������������һ��λ��Ҫ�ȴ���
    configASSERT((uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES) == 0);
    configASSERT(uxBitsToWaitFor != 0);

    #if (INCLUDE_xTaskGetSchedulerState == 1) || (configUSE_TIMERS == 1)
    {
        // ����ȷ��������δ��ͣ�����ǵȴ�ʱ��Ϊ 0��
        configASSERT(!( (xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED) && (xTicksToWait != 0) ));
    }
    #endif

    // ��ͣ����������ȡ�
    vTaskSuspendAll();
    {
        uxOriginalBitValue = pxEventBits->uxEventBits;

        // �����¼�λ��
        (void)xEventGroupSetBits(xEventGroup, uxBitsToSet);

        if (((uxOriginalBitValue | uxBitsToSet) & uxBitsToWaitFor) == uxBitsToWaitFor)
        {
            // �����λ�������� - ����Ҫ������
            uxReturn = (uxOriginalBitValue | uxBitsToSet);

            // ����ȴ���λ����������Ψһ�����񣩡�
            pxEventBits->uxEventBits &= ~uxBitsToWaitFor;

            xTicksToWait = 0;
        }
        else
        {
            if (xTicksToWait != (TickType_t)0)
            {
                // ��¼ͬ��������
                traceEVENT_GROUP_SYNC_BLOCK(xEventGroup, uxBitsToSet, uxBitsToWaitFor);

                // �洢�ȴ���λ������������״̬��
                vTaskPlaceOnUnorderedEventList(&(pxEventBits->xTasksWaitingForBits), (uxBitsToWaitFor | eventCLEAR_EVENTS_ON_EXIT_BIT | eventWAIT_FOR_ALL_BITS), xTicksToWait);

                // ��ʼ������ֵ��
                uxReturn = 0;
            }
            else
            {
                // �����λδ���ã���û��ָ���ȴ�ʱ�� - ֱ�ӷ��ص�ǰ�¼�λ��
                uxReturn = pxEventBits->uxEventBits;
            }
        }
    }

    // �ָ�������ȡ�
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

        // ��ȡ��������õ�λ��
        uxReturn = uxTaskResetEventItemValue();

        if ((uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET) == (EventBits_t)0)
        {
            // ����ʱ�����ص�ǰ�¼�λ��
            taskENTER_CRITICAL();
            {
                uxReturn = pxEventBits->uxEventBits;

                // ���λ�ڽ����������ã�����Ҫ���λ��
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
            // λ�����ã������������
        }

        // �������λ��
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }

    // ��¼ͬ��������
    traceEVENT_GROUP_SYNC_END(xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred);

    return uxReturn;
}
/*-----------------------------------------------------------*/

/*
 * @brief xEventGroupWaitBits �ȴ��¼����е�ָ��λ�����á�
 *
 * �˺������ڵȴ��¼����е�ָ��λ�����ã�������������ʱ���ء�
 *
 * @param xEventGroup �¼�������
 * @param uxBitsToWaitFor Ҫ�ȴ����¼�λ��
 * @param xClearOnExit �������Ϊ pdTRUE�����ڷ���֮ǰ����ȴ���λ��
 * @param xWaitForAllBits �������Ϊ pdTRUE����ȴ�����ָ����λ�������ã�����ֻҪ��һ��λ�����þͷ��ء�
 * @param xTicksToWait �ȴ���ʱ�䣨�Եδ�Ϊ��λ����
 *
 * @return ���ص�ǰ�¼����е��¼�λ������ȴ���ʱ���򷵻ص�ǰ�¼�λ��ֵ��
 */
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToWaitFor,
    const BaseType_t xClearOnExit,
    const BaseType_t xWaitForAllBits,
    TickType_t xTicksToWait )
{
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup; // ��ȡ�¼���ĵ�ַ
    EventBits_t uxReturn, uxControlBits = 0;                 // ��ʼ������ֵ�Ϳ���λ
    BaseType_t xWaitConditionMet, xAlreadyYielded;           // ��ʼ���ȴ��������Ƿ��Ѿ��л���־
    BaseType_t xTimeoutOccurred = pdFALSE;                   // ��ʼ����ʱ��־

    /* ����û�û�г��Եȴ��ں�ʹ�õ�λ����������������һ��λ�� */
    configASSERT( xEventGroup );
    configASSERT( ( uxBitsToWaitFor & eventEVENT_BITS_CONTROL_BYTES ) == 0 );
    configASSERT( uxBitsToWaitFor != 0 );

    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        /* �������������ͣ���ҵȴ�ʱ�䲻Ϊ�㣬�����ʧ�ܡ� */
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    /* ��ͣ����������ȡ� */
    vTaskSuspendAll();
    {
        const EventBits_t uxCurrentEventBits = pxEventBits->uxEventBits; // ��ȡ��ǰ�¼�λ

        /* ���ȴ������Ƿ��Ѿ����㡣 */
        xWaitConditionMet = prvTestWaitCondition( uxCurrentEventBits, uxBitsToWaitFor, xWaitForAllBits );

        if( xWaitConditionMet != pdFALSE )
        {
            /* �ȴ������Ѿ����㣬���������� */
            uxReturn = uxCurrentEventBits;
            xTicksToWait = (TickType_t)0;

            /* ����������λ��������ȴ���λ�� */
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
            /* �ȴ�����û�����㣬����û��ָ������ʱ�䣬ֱ�ӷ��ص�ǰֵ�� */
            uxReturn = uxCurrentEventBits;
        }
        else
        {
            /* ���������Եȴ������λ�����á� */

            /* ���ÿ���λ����¼�˵��õ���Ϊ�� */
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

            /* �洢�ȴ�λ������������״̬�� */
            vTaskPlaceOnUnorderedEventList( &(pxEventBits->xTasksWaitingForBits), (uxBitsToWaitFor | uxControlBits), xTicksToWait );

            /* ��������ֵ����ֹ���������档 */
            uxReturn = 0;

            /* ��¼�ȴ������� */
            traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor );
        }
    }

    /* �ָ�������ȡ� */
    xAlreadyYielded = xTaskResumeAll();

    if( xTicksToWait != (TickType_t)0 )
    {
        if( xAlreadyYielded == pdFALSE )
        {
            portYIELD_WITHIN_API(); // ǿ�ƽ��е���
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        /* ��ȡ���õ�λ�� */
        uxReturn = uxTaskResetEventItemValue();

        if( (uxReturn & eventUNBLOCKED_DUE_TO_BIT_SET) == (EventBits_t)0 )
        {
            /* ����ʱ�����ص�ǰ�¼�λ��ֵ�� */
            taskENTER_CRITICAL();
            {
                uxReturn = pxEventBits->uxEventBits;

                /* ����ȴ��������㣬������ȴ���λ�� */
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

            /* ��ǳ�ʱ������ */
            xTimeoutOccurred = pdFALSE;
        }
        else
        {
            /* λ�ѱ����ã������������� */
        }

        /* �������λ�� */
        uxReturn &= ~eventEVENT_BITS_CONTROL_BYTES;
    }

    /* ��¼�ȴ������� */
    traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred );

    return uxReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ����¼����е�ָ���¼�λ
 *
 * �˺�����������¼�����ָ�����¼�λ�������������λ֮ǰ���¼���ֵ��
 *
 * @param[in] xEventGroup �¼�������
 * @param[in] uxBitsToClear ��Ҫ������¼�λ��
 *
 * @return �������λ֮ǰ���¼���ֵ��
 */
EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    EventBits_t uxReturn;

    /* ����û��Ƿ���ͼ����ں�����ʹ�õ�λ�� */
    configASSERT(xEventGroup); // ȷ���¼������ǿա�
    configASSERT((uxBitsToClear & eventEVENT_BITS_CONTROL_BYTES) == 0); // ȷ�������λ�в���������λ��

    taskENTER_CRITICAL(); // �����ٽ�����ȷ��������ԭ���ԡ�
    {
        traceEVENT_GROUP_CLEAR_BITS(xEventGroup, uxBitsToClear); // ��¼���λ�Ĳ�����

        /* �����λ֮ǰ�����¼����ֵ�� */
        uxReturn = pxEventBits->uxEventBits;

        /* ���ָ����λ�� */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    taskEXIT_CRITICAL(); // �˳��ٽ�����

    return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 1 ) )

/**
 * @brief ���жϷ����������¼����е�ָ���¼�λ
 *
 * �˺������ڴ��жϷ����������¼�����ָ�����¼�λ������ͨ������һ���ص��������첽������¼�λ��
 * 
 * @param[in] xEventGroup �¼�������
 * @param[in] uxBitsToClear ��Ҫ������¼�λ��
 *
 * @return ����ֵ��ʾ�Ƿ�ɹ����������������
 *         pdPASS ��ʾ�ɹ����������������
 *         pdFAIL ��ʾδ�ܰ������������
 */
BaseType_t xEventGroupClearBitsFromISR(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    BaseType_t xReturn;

    // ��¼���жϷ���������λ�Ĳ�����
    traceEVENT_GROUP_CLEAR_BITS_FROM_ISR(xEventGroup, uxBitsToClear);

    // ͨ���жϰ�ȫ�ķ�ʽ�����첽����¼�λ��
    // �������ص��������¼���������Ҫ�����λ���ص������ģ�����ΪNULL����
    xReturn = xTimerPendFunctionCallFromISR(vEventGroupClearBitsCallback,
                                            (void *)xEventGroup,
                                            (uint32_t)uxBitsToClear,
                                            NULL);

    return xReturn;
}

#endif
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ�������ȡ�¼����е��¼�λ
 *
 * �˺������ڴ��жϷ�������ȡ�¼����еĵ�ǰ�¼�λֵ��
 *
 * @param[in] xEventGroup �¼�������
 *
 * @return �����¼����еĵ�ǰ�¼�λֵ��
 */
EventBits_t xEventGroupGetBitsFromISR(EventGroupHandle_t xEventGroup)
{
    UBaseType_t uxSavedInterruptStatus;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    EventBits_t uxReturn;

    // ���浱ǰ�ж�״̬���������жϡ�
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        // ��ȡ�¼����еĵ�ǰ�¼�λֵ��
        uxReturn = pxEventBits->uxEventBits;
    }
    // �ָ��ж�״̬��
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    return uxReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief �����¼����е��¼�λ
 *
 * �˺������������¼����е�ָ���¼�λ��������Ƿ��еȴ���Щλ��������Ա����������
 *
 * @param[in] xEventGroup �¼�������
 * @param[in] uxBitsToSet ��Ҫ���õ��¼�λ��
 *
 * @return �����¼����еĵ�ǰ�¼�λֵ��
 */
EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet)
{
    ListItem_t *pxListItem, *pxNext;
    ListItem_t const *pxListEnd;
    List_t *pxList;
    EventBits_t uxBitsToClear = 0, uxBitsWaitedFor, uxControlBits;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    BaseType_t xMatchFound = pdFALSE;

    /* ����û��Ƿ���ͼ�����ں�����ʹ�õ�λ�� */
    configASSERT(xEventGroup); // ȷ���¼������ǿա�
    configASSERT((uxBitsToSet & eventEVENT_BITS_CONTROL_BYTES) == 0); // ȷ�����õ�λ�в���������λ��

    pxList = &(pxEventBits->xTasksWaitingForBits);
    pxListEnd = listGET_END_MARKER(pxList); /*lint !e826 !e740 ʹ�� mini list �ṹ��Ϊ�б��������Խ�ʡ RAM��������Ч�ġ�*/
    vTaskSuspendAll(); // ��ͣ����������ȡ�
    {
        traceEVENT_GROUP_SET_BITS(xEventGroup, uxBitsToSet); // ��¼����λ�Ĳ�����

        pxListItem = listGET_HEAD_ENTRY(pxList);

        /* ����ָ����λ�� */
        pxEventBits->uxEventBits |= uxBitsToSet;

        /* ����µ�λֵ�Ƿ�Ӧ�ý���κ������������ */
        while (pxListItem != pxListEnd)
        {
            pxNext = listGET_NEXT(pxListItem);
            uxBitsWaitedFor = listGET_LIST_ITEM_VALUE(pxListItem);
            xMatchFound = pdFALSE;

            /* ����ȴ���λ�Ϳ���λ�� */
            uxControlBits = uxBitsWaitedFor & eventEVENT_BITS_CONTROL_BYTES;
            uxBitsWaitedFor &= ~eventEVENT_BITS_CONTROL_BYTES;

            if ((uxControlBits & eventWAIT_FOR_ALL_BITS) == (EventBits_t)0)
            {
                /* ֻѰ�ҵ���λ�����á� */
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
                /* ����λ�������á� */
                xMatchFound = pdTRUE;
            }
            else
            {
                /* ��Ҫ����λ�����ã����������е�λ�������á� */
            }

            if (xMatchFound != pdFALSE)
            {
                /* λƥ�䡣���˳�ʱ�Ƿ�Ӧ�����Щλ�� */
                if ((uxControlBits & eventCLEAR_EVENTS_ON_EXIT_BIT) != (EventBits_t)0)
                {
                    uxBitsToClear |= uxBitsWaitedFor;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                /* �ڴ��¼��б��Ƴ�����֮ǰ����ʵ�ʵ��¼���־ֵ�洢��������¼��б����У�
                   ������ eventUNBLOCKED_DUE_TO_BIT_SET ��־���Ա�����֪������Ϊ�������λƥ����������������������Ϊ��ʱ�� */
                (void)xTaskRemoveFromUnorderedEventList(pxListItem, pxEventBits->uxEventBits | eventUNBLOCKED_DUE_TO_BIT_SET);
            }

            /* �ƶ�����һ���б��ע�� pxListItem->pxNext �����ﲻʹ�ã�
               ��Ϊ�б�������ѱ����¼��б��Ƴ������뵽����/�ȴ���ȡ�б��С� */
            pxListItem = pxNext;
        }

        /* ����ڿ������������� eventCLEAR_EVENTS_ON_EXIT_BIT ʱƥ����κ�λ�� */
        pxEventBits->uxEventBits &= ~uxBitsToClear;
    }
    (void)xTaskResumeAll(); // �ָ�������ȡ�

    return pxEventBits->uxEventBits; // �����¼����еĵ�ǰ�¼�λֵ��
}
/*-----------------------------------------------------------*/

/**
 * @brief ɾ���¼���
 *
 * �˺�������ɾ��һ���¼��飬���ͷ���������ڴ档
 * ����¼����л��еȴ��е������������Щ�����������
 *
 * @param[in] xEventGroup Ҫɾ�����¼�������
 */
void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;
    const List_t *pxTasksWaitingForBits = &(pxEventBits->xTasksWaitingForBits);

    vTaskSuspendAll(); // ��ͣ����������ȡ�
    {
        traceEVENT_GROUP_DELETE(xEventGroup); // ��¼ɾ���¼���Ĳ�����

        while (listCURRENT_LIST_LENGTH(pxTasksWaitingForBits) > (UBaseType_t)0)
        {
            /* ������������������ 0 ��ʾ�¼��б����ڱ�ɾ������˲������κ�λ�����á� */
            configASSERT(pxTasksWaitingForBits->xListEnd.pxNext != (ListItem_t *)&(pxTasksWaitingForBits->xListEnd));
            (void)xTaskRemoveFromUnorderedEventList(pxTasksWaitingForBits->xListEnd.pxNext, eventUNBLOCKED_DUE_TO_BIT_SET);
        }

        #if (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 0)
        {
            /* �¼���ֻ���Ƕ�̬����� - �ͷ��ڴ档 */
            vPortFree(pxEventBits);
        }
        #elif (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 1)
        {
            /* �¼�������Ǿ�̬��̬����ģ������ڳ����ͷ��ڴ�ǰ���м�顣 */
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
    (void)xTaskResumeAll(); // �ָ�������ȡ�
}
/*-----------------------------------------------------------*/

/**
 * @brief �ڲ�ʹ�� - ִ��һ�����жϹ���ġ�����λ������
 *
 * �˺��������ڲ�������жϷ���������ġ�����λ�����
 *
 * @param[in] pvEventGroup �¼�������
 * @param[in] ulBitsToSet ��Ҫ���õ��¼�λ��
 */
void vEventGroupSetBitsCallback(void *pvEventGroup, const uint32_t ulBitsToSet)
{
    // ���¼�λ���ò���ί�и� xEventGroupSetBits ������
    (void)xEventGroupSetBits(pvEventGroup, (EventBits_t)ulBitsToSet);
}
/*-----------------------------------------------------------*/

/**
 * @brief �ڲ�ʹ�� - ִ��һ�����жϹ���ġ����λ������
 *
 * �˺��������ڲ�������жϷ���������ġ����λ�����
 *
 * @param[in] pvEventGroup �¼�������
 * @param[in] ulBitsToClear ��Ҫ������¼�λ��
 */
void vEventGroupClearBitsCallback(void *pvEventGroup, const uint32_t ulBitsToClear)
{
    // ���� xEventGroupClearBits ����������¼����е�ָ���¼�λ��
    (void)xEventGroupClearBits(pvEventGroup, (EventBits_t)ulBitsToClear);
}
/*-----------------------------------------------------------*/

/**
 * @brief ��̬�ڲ����� - ���Եȴ�����
 *
 * �˺������ڲ����¼����еĵ�ǰ�¼�λ�Ƿ�����ȴ�������
 * ���� `xWaitForAllBits` �����Ĳ�ͬ�����Եȴ�һ�������ض���λ�����á�
 *
 * @param[in] uxCurrentEventBits ��ǰ�¼����е��¼�λ��
 * @param[in] uxBitsToWaitFor ��Ҫ�ȴ����¼�λ��
 * @param[in] xWaitForAllBits ָ���Ƿ���Ҫ�ȴ�����λ�����á�
 *
 * @return ����ֵ��ʾ�ȴ������Ƿ����㡣
 *         pdTRUE ��ʾ�������㡣
 *         pdFALSE ��ʾ���������㡣
 */
static BaseType_t prvTestWaitCondition(const EventBits_t uxCurrentEventBits, const EventBits_t uxBitsToWaitFor, const BaseType_t xWaitForAllBits)
{
    BaseType_t xWaitConditionMet = pdFALSE;

    if (xWaitForAllBits == pdFALSE)
    {
        /* ����ֻ��Ҫ�ȴ� uxBitsToWaitFor �е�һ��λ�����á�
           ��ǰ����λ�������� */
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
        /* ������Ҫ�ȴ� uxBitsToWaitFor �е�����λ�����á�
           ����λ���Ѿ��������� */
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
 * @brief ���жϷ�����������¼����е��¼�λ
 *
 * �˺������ڴ��жϷ�����������¼����е�ָ���¼�λ��
 * �ú���ͨ������һ���ص��������첽�������¼�λ��
 *
 * @param[in] xEventGroup �¼�������
 * @param[in] uxBitsToSet ��Ҫ���õ��¼�λ��
 * @param[out] pxHigherPriorityTaskWoken ָ��һ���������ñ�������ָʾ�Ƿ��и������ȼ������񱻻��ѡ�
 *
 * @return ����ֵ��ʾ�Ƿ�ɹ����������ò�����
 *         pdPASS ��ʾ�ɹ����������ò�����
 *         pdFAIL ��ʾδ�ܰ������ò�����
 */
BaseType_t xEventGroupSetBitsFromISR(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet, BaseType_t *pxHigherPriorityTaskWoken)
{
    BaseType_t xReturn;

    // ��¼���жϷ����������λ�Ĳ�����
    traceEVENT_GROUP_SET_BITS_FROM_ISR(xEventGroup, uxBitsToSet);

    // ͨ���жϰ�ȫ�ķ�ʽ�����첽�����¼�λ��
    // �������ص��������¼���������Ҫ���õ�λ���������ȼ����񱻻��ѵ�ָʾ������
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
 * @brief ��ȡ�¼���ı��
 *
 * �˺������ڻ�ȡָ���¼���ı�š�
 *
 * @param[in] xEventGroup �¼�������
 *
 * @return �����¼���ı�š�
 *         ����¼�����Ϊ NULL���򷵻� 0��
 */
UBaseType_t uxEventGroupGetNumber(void *xEventGroup)
{
    UBaseType_t xReturn;
    EventGroup_t *pxEventBits = (EventGroup_t *)xEventGroup;

    if (xEventGroup == NULL)
    {
        // ����¼�����Ϊ NULL���򷵻� 0��
        xReturn = 0;
    }
    else
    {
        // ��ȡ�¼���ı�š�
        xReturn = pxEventBits->uxEventGroupNumber;
    }

    return xReturn;
}

#endif

