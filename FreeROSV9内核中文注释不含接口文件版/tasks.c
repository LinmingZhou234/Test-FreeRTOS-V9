/* ��׼��ͷ�ļ������� */
#include <stdlib.h>
#include <string.h>

/* ���� MPU_WRAPPERS_INCLUDED_FROM_API_FILE ��ֹ task.h ���¶������� API ������ʹ�� MPU ��װ����
��Ӧ��ֻ�� task.h ��Ӧ�ó����ļ��а���ʱ������ */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* FreeRTOS ͷ�ļ������� */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "StackMacros.h"

/* Lint e961 �� e750 ��ȡ������Ϊ MISRA �쳣��ԭ���� MPU �˿�Ҫ���������ͷ�ļ��ж��� MPU_WRAPPERS_INCLUDED_FROM_API_FILE��
������������ļ��ж��壬�Ա�������ȷ����Ȩ�����Ȩ���Ӻͷ��á� */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750. */

/* ���� configUSE_STATS_FORMATTING_FUNCTIONS Ϊ 2 �԰���ͳ�Ƹ�ʽ�������������ڴ˴����� stdio.h�� */
#if ( configUSE_STATS_FORMATTING_FUNCTIONS == 1 )
	/* ������ļ��ĵײ���������ѡ�ĺ��������������� uxTaskGetSystemState() �������ɵ�ԭʼ��������������ɶ����ı���
	ע�⣬��ʽ������ֻ��Ϊ�˷����ṩ�����Ҳ�����Ϊ���ں˵�һ���֡� */
	#include <stdio.h>
#endif /* configUSE_STATS_FORMATTING_FUNCTIONS == 1 */

#if( configUSE_PREEMPTION == 0 )
	/* ���ʹ��Э������������Ӧ�ý�����Ϊ������һ���������ȼ��������ִ���������л��� */
	#define taskYIELD_IF_USING_PREEMPTION()
#else
	#define taskYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/* ����Ϊ TCB �е� ucNotifyState ��Ա����ֵ�� */
#define taskNOT_WAITING_NOTIFICATION	( ( uint8_t ) 0 )
#define taskWAITING_NOTIFICATION		( ( uint8_t ) 1 )
#define taskNOTIFICATION_RECEIVED		( ( uint8_t ) 2 )

/*
 * �����񴴽�ʱ�����������ջ��ֵ���ⴿ����Ϊ�˼������ĸ�ˮλ��ǡ�
 */
#define tskSTACK_FILL_BYTE	( 0xa5U )

/* 
 * ��ʱ FreeRTOSConfig.h ���ý�����ʹ�ö�̬����� RAM ������������������£����κ�����ɾ��ʱ����֪��Ҫ�ͷ������ջ��TCB��
 * ��ʱ FreeRTOSConfig.h ���ý�����ʹ�þ�̬����� RAM ������������������£����κ�����ɾ��ʱ����֪����Ҫ�ͷ������ջ��TCB��
 * ��ʱ FreeRTOSConfig.h ��������ʹ�þ�̬��̬����� RAM ������������������£�TCB��һ����Ա���ڼ�¼ջ��/��TCB�Ǿ�̬���Ƕ�̬����ģ�
 * ��˵�����ɾ��ʱ���ٴ��ͷŶ�̬����� RAM�����Ҳ������ͷž�̬����� RAM��
 * tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE ֻ���ڿ���ʹ�þ�̬��̬����� RAM ��������ʱ��Ϊ�档
 * ע�⣬��� portUSING_MPU_WRAPPERS Ϊ 1������Դ������о�̬�����ջ�Ͷ�̬�����TCB���ܱ�������
 */
#define tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE ( ( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) || ( portUSING_MPU_WRAPPERS == 1 ) )
#define tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB 		( ( uint8_t ) 0 )	/* ��̬�����ջ��TCB */
#define tskSTATICALLY_ALLOCATED_STACK_ONLY 			( ( uint8_t ) 1 )	/* ��̬�����ջ */
#define tskSTATICALLY_ALLOCATED_STACK_AND_TCB		( ( uint8_t ) 2 )	/* ��̬�����ջ��TCB */

/*
 * �� vListTasks ʹ�õĺ꣬����ָʾ����������״̬��
 */
#define tskBLOCKED_CHAR		( 'B' )	/* ����״̬ */
#define tskREADY_CHAR		( 'R' )	/* ����״̬ */
#define tskDELETED_CHAR		( 'D' )	/* ��ɾ��״̬ */
#define tskSUSPENDED_CHAR	( 'S' )	/* ����״̬ */

/*
 * ĳЩ�ں˸�֪�ĵ�����Ҫ���������Ҫ���ʵ�������ȫ�ֵģ��������ļ�������ġ�
 */
#ifdef portREMOVE_STATIC_QUALIFIER
	#define static	/* ȡ����̬�޶��� */
#endif

#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 0 )

	/* ��� configUSE_PORT_OPTIMISED_TASK_SELECTION Ϊ 0��������ѡ������ͨ�õķ�ʽ���еģ����ַ�ʽ�����κ��ض���΢�������ܹ������Ż��� */

/* uxTopReadyPriority ����������ȼ�����״̬��������ȼ��� */
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
    /* ���Ұ������������������ȼ����С� */ \
    while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopPriority ] ) ) ) \
    { \
        configASSERT( uxTopPriority ); \
        --uxTopPriority; \
    } \
 \
    /* listGET_OWNER_OF_NEXT_ENTRY ͨ���б�������ʹ����ͬ���ȼ��������ܹ�ƽ�ȵط�������ʱ�䡣 */ \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
    uxTopReadyPriority = uxTopPriority; \
} /* taskSELECT_HIGHEST_PRIORITY_TASK */

/*-----------------------------------------------------------*/

/* ����ʹ���ض��ܹ��Ż�������ѡ��ʱ������� taskRESET_READY_PRIORITY() �� portRESET_READY_PRIORITY()�� */
#define taskRESET_READY_PRIORITY( uxPriority )
#define portRESET_READY_PRIORITY( uxPriority, uxTopReadyPriority )

#else /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/* ��� configUSE_PORT_OPTIMISED_TASK_SELECTION Ϊ 1��������ѡ�������ض�΢�������ܹ��Ż��ķ�ʽ���еġ� */

/* �ṩ������ض��ܹ��Ż��İ汾�����ö˿ڶ���ĺꡣ */
#define taskRECORD_READY_PRIORITY( uxPriority ) portRECORD_READY_PRIORITY( uxPriority, uxTopReadyPriority )

/*-----------------------------------------------------------*/

#define taskSELECT_HIGHEST_PRIORITY_TASK() \
{ \
    UBaseType_t uxTopPriority; \
 \
    /* ���Ұ������������������ȼ��б� */ \
    portGET_HIGHEST_PRIORITY( uxTopPriority, uxTopReadyPriority ); \
    configASSERT( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ uxTopPriority ] ) ) > 0 ); \
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentTCB, &( pxReadyTasksLists[ uxTopPriority ] ) ); \
} /* taskSELECT_HIGHEST_PRIORITY_TASK() */

/*-----------------------------------------------------------*/

/* �ṩ������ض��ܹ��Ż��İ汾�����ڱ����õ� TCB ���Ӿ����б�����ʱ���á�����������ӳٻ�����б����ã�������������ھ����б��С� */
#define taskRESET_READY_PRIORITY( uxPriority ) \
{ \
    if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ ( uxPriority ) ] ) ) == ( UBaseType_t ) 0 ) \
    { \
        portRESET_READY_PRIORITY( ( uxPriority ), ( uxTopReadyPriority ) ); \
    } \
}

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

/*-----------------------------------------------------------*/

/* �����������ʱ��pxDelayedTaskList �� pxOverflowDelayedTaskList �ᱻ������ */
#define taskSWITCH_DELAYED_LISTS() \
{ \
    List_t *pxTemp; \
 \
    /* �л��б�ʱ���ӳ������б�Ӧ���ǿյġ� */ \
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
 * �� pxTCB ���������������ʵ��ľ����б��С��������뵽�б��ĩβ��
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
 * �����������һ������Ϊ NULL �� TaskHandle_t ���������� NULL ��ʾӦʹ�õ�ǰ����ִ�е�����ľ�������������
 * �˺�����Ǽ������Ƿ�Ϊ NULL��������ָ���ʵ� TCB ��ָ�롣
 */
#define prvGetTCBFromHandle( pxHandle ) ( ( ( pxHandle ) == NULL ) ? ( TCB_t * ) pxCurrentTCB : ( TCB_t * ) ( pxHandle ) )

/* 
 * �¼��б����ֵͨ�����ڱ���������������ȼ��������������䰴�������ȼ�˳��洢����
 * Ȼ������ʱ���ᱻ����������Ŀ�ġ���Ҫ���ǣ�����������������Ŀ��ʱ����ֵ��Ӧ�����������ȼ��ı仯�������¡�
 * �����λ�������ڸ�֪��������ֵ��Ӧ���ı䡪������������£����ø�ֵ��ģ��������ȷ���������ͷ�ʱ�ָ���ԭʼֵ��
 */
#if( configUSE_16_BIT_TICKS == 1 )
    /* ����ʹ��16λticks����� */
    #define taskEVENT_LIST_ITEM_VALUE_IN_USE 0x8000U
#else
    /* ����ʹ��32λticks����� */
    #define taskEVENT_LIST_ITEM_VALUE_IN_USE 0x80000000UL
#endif /* configUSE_16_BIT_TICKS == 1 */

/*
 * ������ƿ飨TCB����ÿ�����񶼻����һ��������ƿ飨TCB�������ڴ洢�����״̬��Ϣ��
 * ����ָ�����������ģ�����ʱ�����������Ĵ���ֵ����ָ�롣
 */
typedef struct tskTaskControlBlock
{
    volatile StackType_t *pxTopOfStack; /*< ָ������ջ����λ�á��������һ����������ջ��������� TCB �ṹ�ĵ�һ����Ա�� */

    #if (portUSING_MPU_WRAPPERS == 1)
        xMPU_SETTINGS xMPUSettings;     /*< ��ʹ�� MPU ��ƽ̨�ϣ������� MPU ���á������� TCB �ṹ�ĵڶ�����Ա�� */
    #endif

    ListItem_t xStateListItem;          /*< ����״̬�б������Ӹ��б������õ�״̬�б�����������״̬�����������������𣩡� */
    ListItem_t xEventListItem;          /*< ���ڴ��¼��б����������� */

    UBaseType_t uxPriority;             /*< ��������ȼ���0 ��������ȼ��� */

    StackType_t *pxStack;               /*< ָ������ջ����ʼλ�á� */

    char pcTaskName[configMAX_TASK_NAME_LEN]; /*< �����񴴽�ʱ����������������ơ������ڵ��ԡ� */
    /* lint !e971 �����ַ����͵��ַ�ʹ��δ�޶��� char ���͡� */

    #if (portSTACK_GROWTH > 0)
        StackType_t *pxEndOfStack;      /*< ָ��ܹ���ջ���������ĩ��λ�á�����ջ���������ļܹ���ָ��ջ�ס� */
    #endif

    #if (portCRITICAL_NESTING_IN_TCB == 1)
        UBaseType_t uxCriticalNesting;  /*< ���ڲ��ڶ˿ڲ�ά�����������ƽ̨������ؼ���Ƕ����ȡ� */
    #endif

    #if (configUSE_TRACE_FACILITY == 1)
        UBaseType_t uxTCBNumber;        /*< ÿ�δ��� TCB ʱ���������֡����������ȷ�������ʱ��ɾ�������´����� */
        UBaseType_t uxTaskNumber;       /*< ר���ڵ��������ٴ�������֡� */
    #endif

    #if (configUSE_MUTEXES == 1)
        UBaseType_t uxBasePriority;     /*< ��������������ȼ����������ȼ��̳л��ơ� */
        UBaseType_t uxMutexesHeld;      /*< ������еĻ����������� */
    #endif

    #if (configUSE_APPLICATION_TASK_TAG == 1)
        TaskHookFunction_t pxTaskTag;   /*< Ӧ�ó��������ǩ�� */
    #endif

    #if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)
        void *pvThreadLocalStoragePointers[configNUM_THREAD_LOCAL_STORAGE_POINTERS];
    #endif

    #if (configGENERATE_RUN_TIME_STATS == 1)
        uint32_t ulRunTimeCounter;      /*< �洢����������״̬��ʱ������ */
    #endif

    #if (configUSE_NEWLIB_REENTRANT == 1)
        /* ����һ���ض�������� Newlib reent �ṹ��
         * ע�⣺Ӧ�û�Ҫ������� Newlib ֧�֣��� FreeRTOS ά�����Լ���δʹ�á�
         * FreeRTOS �������ɴ˵��µ� Newlib �������û�������Ϥ Newlib ���ṩ��Ҫ�Ĵ��ʵ�֡�
         * ���棺Ŀǰ Newlib ���ʵ����һ��ϵͳ��Χ�ڵ� malloc()�������ṩ���� */
        struct _reent xNewLib_reent;
    #endif

    #if (configUSE_TASK_NOTIFICATIONS == 1)
        volatile uint32_t ulNotifiedValue; /*< ֵ֪ͨ�� */
        volatile uint8_t ucNotifyState;    /*< ֪ͨ״̬�� */
    #endif

    /* �μ� tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE �����Ϸ���ע�͡� */
    #if (tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0)
        uint8_t ucStaticallyAllocated;    /*< ��������Ǿ�̬����ģ�������Ϊ pdTRUE����ȷ�����᳢���ͷ��ڴ档 */
    #endif

    #if (INCLUDE_xTaskAbortDelay == 1)
        uint8_t ucDelayAborted;           /*< �ӳ��Ƿ�ȡ���� */
    #endif

} tskTCB;

/* 
 * �����ɵ� tskTCB ���ƣ������� typedef Ϊ�µ� TCB_t ���ƣ��Ա�ʹ�ýϾɵ��ں˸�֪��������
 */
typedef tskTCB TCB_t;

/*lint -e956 ͨ���Ծ�̬�������ֶ������ͼ��ȷ������Щ��������Ϊ volatile�� */

PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;

/* ���ھ���������������б� ---------------------------- */
PRIVILEGED_DATA static List_t pxReadyTasksLists[ configMAX_PRIORITIES ]; /*< �����ȼ�����ľ��������б� */
PRIVILEGED_DATA static List_t xDelayedTaskList1;                        /*< �ӳ������б� */
PRIVILEGED_DATA static List_t xDelayedTaskList2;                        /*< �ӳ������б�ʹ�������б���һ�������ѳ�����ǰ����ֵ���ӳ٣��� */
PRIVILEGED_DATA static List_t * volatile pxDelayedTaskList;             /*< ָ��ǰʹ�õ��ӳ������б� */
PRIVILEGED_DATA static List_t * volatile pxOverflowDelayedTaskList;     /*< ָ�����ڱ����ѳ�����ǰ����ֵ��������ӳ������б� */
PRIVILEGED_DATA static List_t xPendingReadyList;                        /*< ��������������ʱ��Ϊ����״̬���������ǽ��ڵ������ָ�ʱ���Ƶ������б��С� */

#if( INCLUDE_vTaskDelete == 1 )

    PRIVILEGED_DATA static List_t xTasksWaitingTermination;              /*< �ѱ�ɾ�������ڴ���δ�ͷŵ����� */
    PRIVILEGED_DATA static volatile UBaseType_t uxDeletedTasksWaitingCleanUp = ( UBaseType_t ) 0U;

#endif

#if ( INCLUDE_vTaskSuspend == 1 )

    PRIVILEGED_DATA static List_t xSuspendedTaskList;                   /*< ��ǰ������������б� */

#endif

/* �����ļ�˽�еı����� ------------------------------------ */
//PRIVILEGED_DATA static volatile UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;      /*< ��ǰϵͳ������������ */

//Ϊ��ʹ��keil��debug���ܲ鿴cpu�������ʣ���main.c������ʵ������������ʱ��Ĳ鿴����vTaskGetRunTime����Ҫ��main.c������uxCurrentNumberOfTasks�����Զ�����������¶���
UBaseType_t uxCurrentNumberOfTasks = ( UBaseType_t ) 0U;      /*< ��ǰϵͳ������������ */


PRIVILEGED_DATA static volatile TickType_t xTickCount = ( TickType_t ) 0U;                    /*< ��ǰϵͳ��ʱ��̶ȼ���ֵ�� */
PRIVILEGED_DATA static volatile UBaseType_t uxTopReadyPriority = tskIDLE_PRIORITY;            /*< ��ǰ������ȼ�������������ȼ��� */
PRIVILEGED_DATA static volatile BaseType_t xSchedulerRunning = pdFALSE;                       /*< �������Ƿ��������еı�־�� */
PRIVILEGED_DATA static volatile UBaseType_t uxPendedTicks = ( UBaseType_t ) 0U;              /*< δ�����tick������ */
PRIVILEGED_DATA static volatile BaseType_t xYieldPending = pdFALSE;                           /*< �Ƿ��д�����������л����� */
PRIVILEGED_DATA static volatile BaseType_t xNumOfOverflows = ( BaseType_t ) 0;               /*< ��������������� */
PRIVILEGED_DATA static UBaseType_t uxTaskNumber = ( UBaseType_t ) 0U;                        /*< �����š� */
PRIVILEGED_DATA static volatile TickType_t xNextTaskUnblockTime = ( TickType_t ) 0U;         /*< ��һ�������������ʱ�䡣�ڵ���������ǰ��ʼ��Ϊ portMAX_DELAY�� */
PRIVILEGED_DATA static TaskHandle_t xIdleTaskHandle = NULL;                                  /*< �����������ľ�������������ڵ���������ʱ�Զ������� */




/* �������л��ڵ���������ʱ�����𡣴��⣬
�ж��ڵ���������ʱ���ܲ���TCB��xStateListItem���κ�xStateListItem�������õ��б�
����ж���Ҫ�ڵ���������ʱ����������������������¼��б����ƶ���xPendingReadyList�У�
�Ա��ں��ڵ������������ʱ������Ӵ���������б��ƶ���ʵ�ʵľ����б�
����������б���ֻ�����ٽ����з��ʡ� */
PRIVILEGED_DATA static volatile UBaseType_t uxSchedulerSuspended = ( UBaseType_t ) pdFALSE;

#if ( configGENERATE_RUN_TIME_STATS == 1 )

    PRIVILEGED_DATA static uint32_t ulTaskSwitchedInTime = 0UL;   /*< �����ϴ������л�����ʱ��ʱ��/��������ֵ�� */
    PRIVILEGED_DATA static uint32_t ulTotalRunTime = 0UL;         /*< ������ִ��ʱ�䣬������ʱ������ʱ�Ӷ��塣 */

#endif

/*lint +e956 */

/*-----------------------------------------------------------*/

/* �ص�����ԭ�������� --------------------------------------*/
#if(  configCHECK_FOR_STACK_OVERFLOW > 0 )
    extern void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );
#endif

#if( configUSE_TICK_HOOK > 0 )
    extern void vApplicationTickHook( void );
#endif

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
    extern void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
#endif

/* �ļ�˽�к��������� --------------------------------*/

/**
 * ���������������ж��� xTask ���õ������Ƿ�ǰ���ڹ���״̬��
 * ��� xTask ���õ������ڹ���״̬���򷵻� pdTRUE�����򷵻� pdFALSE��
 */
#if ( INCLUDE_vTaskSuspend == 1 )
	static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask ) PRIVILEGED_FUNCTION;
#endif /* INCLUDE_vTaskSuspend */

/*
 * ��ʼ�������ɵ�����ʹ�õ��б�����ڴ�����һ������ʱ�Զ����á�
 */
static void prvInitialiseTaskLists( void ) PRIVILEGED_FUNCTION;

/*
 * �������񣬺�������������һ������һ������ֹ����ѭ����
 * ����������ڴ�����һ���û�����ʱ�Զ���������������б�
 *
 * ʹ�� portTASK_FUNCTION_PROTO() ����Ϊ�������ض��ڶ˿�/��������������չ��
 * �ú����ĵ�Чԭ��Ϊ��
 *
 * void prvIdleTask( void *pvParameters );
 */
static portTASK_FUNCTION_PROTO( prvIdleTask, pvParameters );

/*
 * �����ͷ��ɵ���������������ڴ棬����TCB��ָ���ջ�ռ䡣
 *
 * �ⲻ�����������������ڴ棨�����������Ӧ�ô�����ͨ������ pvPortMalloc ������ڴ棩��
 */
#if ( INCLUDE_vTaskDelete == 1 )

	static void prvDeleteTCB( TCB_t *pxTCB ) PRIVILEGED_FUNCTION;

#endif

/*
 * ���ɿ�������ʹ�á�����Ƿ������񱻷����ڵȴ�ɾ���������б��С�
 * ����У�����������ɾ����TCB��
 */
static void prvCheckTasksWaitingTermination( void ) PRIVILEGED_FUNCTION;

/*
 * ��ǰִ�е����񼴽���������״̬����������ӵ���ǰ���ӳ������б������ӳ������б�
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely ) PRIVILEGED_FUNCTION;

/*
 * ����Ϣ���һ�� TaskStatus_t �ṹ�壬��Щ��Ϣ������ pxList �б��е�ÿ������pxList ������һ�������б��ӳ��б������б�ȣ���
 *
 * �˺�����������ʹ�ã���Ӧ������Ӧ�ó�������е��á�
 */
#if ( configUSE_TRACE_FACILITY == 1 )

	static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState ) PRIVILEGED_FUNCTION;

#endif

/*
 * �� pxList ��������Ϊ pcNameToQuery ������ - ����ҵ��򷵻ظ�����ľ�������û���ҵ��򷵻� NULL��
 */
#if ( INCLUDE_xTaskGetHandle == 1 )

	static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] ) PRIVILEGED_FUNCTION;

#endif

/*
 * ��������ʱ�������ջ�����Ϊһ����֪��ֵ���˺���ͨ��ȷ��ջ����Ȼ���ֳ�ʼԤ��ֵ�Ĳ�����ȷ������ջ�ġ���ˮλ��ǡ���
 */
#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

	static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte ) PRIVILEGED_FUNCTION;

#endif

/*
 * �����ں��´ν����������״̬��������״̬�����ʱ�䣨�Եδ�Ϊ��λ����
 *
 * ����������Ӧ��ʹ�ò����� 0 �������������ǵ��� 1 ��������
 * ����Ϊ��ȷ����ʹ�û�����ĵ͹���ģʽʵ��Ҫ�� configUSE_TICKLESS_IDLE ����Ϊ����ֵʱ��Ҳ�ܵ��� portSUPPRESS_TICKS_AND_SLEEP()��
 */
#if ( configUSE_TICKLESS_IDLE != 0 )

	static TickType_t prvGetExpectedIdleTime( void ) PRIVILEGED_FUNCTION;

#endif

/*
 * �� xNextTaskUnblockTime ����Ϊ��һ������״̬�����˳�����״̬��ʱ�䡣
 */
static void prvResetNextTaskUnblockTime( void );

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

	/*
	 * �����ڴ�ӡ������Ϣ������ɶ����ʱ�����������ƺ����ո�ĸ���������
	 */
	static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName ) PRIVILEGED_FUNCTION;

#endif
/**
 * @brief ����´���������� TCB �ṹ�塣
 *
 * �ú����ڷ����� Task_t �ṹ��֮�������Ǿ�̬���Ƕ�̬�����ڳ�ʼ���ṹ���Ա��
 *
 * @param pxTaskCode �������ں���ָ�롣
 * @param pcName ��������ƣ����ڵ��Ի�ͳ�ơ�
 * @param ulStackDepth ����ջ����ȣ����ֽ���Ϊ��λ��
 * @param pvParameters ���ݸ��������Ĳ�����
 * @param uxPriority ��������ȼ���
 * @param pxCreatedTask ָ����������ָ�룬���ڷ��ش���������ľ����
 * @param pxNewTCB ָ���´�����������ƿ� (TCB) ��ָ�롣
 * @param xRegions �ڴ�������Ϣ�����Ϊ NULL ��ʹ�á�
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
 * �ڴ�������ʼ��������֮�����ڽ��������ڵ������Ŀ���֮�¡�
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB ) PRIVILEGED_FUNCTION;

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

/**
 * @brief ��̬����һ������
 *
 * �˺������ڴ���һ������ʹ�þ�̬������ڴ����洢������ƿ� (TCB) ��ջ��
 *
 * @param pxTaskCode ָ��������ں�����ָ�롣
 * @param pcName ��������ƣ����ڵ��Ի�ͳ�ơ�
 * @param ulStackDepth ����ջ����ȣ����ֽ���Ϊ��λ��
 * @param pvParameters ���ݸ��������Ĳ�����
 * @param uxPriority ��������ȼ���
 * @param puxStackBuffer ��̬�����ջ��������
 * @param pxTaskBuffer ��̬�����������ƿ� (TCB) ��������
 *
 * @return TaskHandle_t ����������ľ�����������ʧ���򷵻� NULL��
 */
TaskHandle_t xTaskCreateStatic(
    TaskFunction_t pxTaskCode,          /**< ָ��������ں�����ָ�롣 */
    const char * const pcName,          /**< ��������ƣ����ڵ��Ի�ͳ�ơ� */
    const uint32_t ulStackDepth,        /**< ����ջ����ȣ����ֽ���Ϊ��λ�� */
    void * const pvParameters,          /**< ���ݸ��������Ĳ����� */
    UBaseType_t uxPriority,             /**< ��������ȼ��� */
    StackType_t * const puxStackBuffer, /**< ��̬�����ջ�������� */
    StaticTask_t * const pxTaskBuffer   /**< ��̬�����������ƿ� (TCB) �������� */
)
{
    TCB_t *pxNewTCB;
    TaskHandle_t xReturn;

    /* 
     * ���Լ��ջ�����������񻺳����Ƿ�Ϊ�ա�
     * ����κ�һ��Ϊ NULL������������������
     */
    configASSERT( puxStackBuffer != NULL );
    configASSERT( pxTaskBuffer != NULL );

    if( ( pxTaskBuffer != NULL ) && ( puxStackBuffer != NULL ) )
    {
        /* 
         * ʹ�ô�����ڴ���Ϊ����� TCB ��ջ��
         * ע�����������ת���ǰ�ȫ�ģ���Ϊ TCB �� StaticTask_t �������ͬ�Ķ��뷽ʽ�����Ҵ�С�ɶ��Լ�顣
         */
        pxNewTCB = ( TCB_t * ) pxTaskBuffer; /*lint !e740 ��Ѱ��������ת���ǿ��Խ��ܵģ���Ϊ�ṹ�������ͬ�Ķ��뷽ʽ�����Ҵ�С�ɶ��Լ�顣 */
        pxNewTCB->pxStack = ( StackType_t * ) puxStackBuffer;

        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            /* 
             * ������Ծ�̬��̬��������˼�¼�������Ǿ�̬�����ģ��Է������Ժ�ɾ����
             * ��� TCB Ϊ��̬����ģ��Ա���ɾ������ʱ��ȷ�ͷ���Դ��
             */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_AND_TCB;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        /* 
         * ��ʼ�������񣬲����������洢�� xReturn �С�
         * ������� prvInitialiseNewTask ����� TCB �ĳ�ʼ����
         */
        prvInitialiseNewTask( pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, &xReturn, pxNewTCB, NULL );

        /* 
         * ����������ӵ������б�
         * ���� prvAddNewTaskToReadyList ��������뵽�������ľ������С�
         */
        prvAddNewTaskToReadyList( pxNewTCB );
    }
    else
    {
        /* ���������������򷵻� NULL�� */
        xReturn = NULL;
    }

    /* ���ش���������ľ���� */
    return xReturn;
}

#endif /* SUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( portUSING_MPU_WRAPPERS == 1 )

/**
 * @brief ���޴���һ������
 *
 * �˺����������������´���һ������ʹ���ṩ��ջ��������������ṹ�塣
 *
 * @param pxTaskDefinition ָ�������������Ϣ�Ľṹ���ָ�롣
 * @param pxCreatedTask ָ����������ָ�룬���ڷ��ش���������ľ����
 *
 * @return BaseType_t ���ش���״̬���ɹ����� pdPASS�����򷵻� errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY��
 */
BaseType_t xTaskCreateRestricted( const TaskParameters_t * const pxTaskDefinition, TaskHandle_t *pxCreatedTask )
{
    TCB_t *pxNewTCB;
    BaseType_t xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;

    /* 
     * ���Լ��ջ�������Ƿ�Ϊ�ա�
     * ���ջ������Ϊ NULL������������������
     */
    configASSERT( pxTaskDefinition->puxStackBuffer );

    if( pxTaskDefinition->puxStackBuffer != NULL )
    {
        /* 
         * ���� TCB �Ŀռ䡣
         * �ڴ���Դȡ���ڶ˿� malloc ������ʵ���Լ��Ƿ�ʹ�þ�̬���䡣
         */
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );

        if( pxNewTCB != NULL )
        {
            /* 
             * ��ջλ�ô洢�� TCB �С�
             * ��һ��������ջ����ʼ��ַ��ֵ�� TCB ��ջָ�롣
             */
            pxNewTCB->pxStack = pxTaskDefinition->puxStackBuffer;

            /* 
             * ��¼������о�̬�����ջ���Է��Ժ�ɾ������
             * ���� TCB ��ǣ�����ջ�Ǿ�̬����ġ�
             */
            pxNewTCB->ucStaticallyAllocated = tskSTATICALLY_ALLOCATED_STACK_ONLY;

            /* 
             * ��ʼ�������񣬲����������洢�� pxCreatedTask �С�
             * ���� prvInitialiseNewTask ����ʼ�� TCB �����������������ԡ�
             */
            prvInitialiseNewTask( pxTaskDefinition->pvTaskCode,
                                  pxTaskDefinition->pcName,
                                  ( uint32_t ) pxTaskDefinition->usStackDepth,
                                  pxTaskDefinition->pvParameters,
                                  pxTaskDefinition->uxPriority,
                                  pxCreatedTask, pxNewTCB,
                                  pxTaskDefinition->xRegions );

            /* 
             * ����������ӵ������б�
             * ���� prvAddNewTaskToReadyList ��������뵽�������ľ������С�
             */
            prvAddNewTaskToReadyList( pxNewTCB );
            xReturn = pdPASS;
        }
    }

    /* ���ش���״̬�� */
    return xReturn;
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,                // ������ں���ָ��
                        const char * const pcName,              // �������ƣ����ڵ���
                        const uint16_t usStackDepth,           // ����ջ�����
                        void * const pvParameters,              // ���ݸ�����Ĳ���
                        UBaseType_t uxPriority,                // �������ȼ�
                        TaskHandle_t * const pxCreatedTask)    // ������������մ�����������
{
    TCB_t *pxNewTCB;                                     // ����һ��ָ��������ƿ飨TCB����ָ��
    BaseType_t xReturn;                                  // ��������ķ���ֵ

    #if( portSTACK_GROWTH > 0 )                          // ���ջ��������
    {
        pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) );  // ��̬����һ��������ƿ�
        if( pxNewTCB != NULL )                           // �������Ƿ�ɹ�
        {
            pxNewTCB->pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ); // ��������ջ�ռ�

            if( pxNewTCB->pxStack == NULL )             // ���ջ�ռ�����Ƿ�ɹ�
            {
                vPortFree( pxNewTCB );                  // ���ջ����ʧ�ܣ��ͷ�֮ǰ����� TCB
                pxNewTCB = NULL;                        // �� TCB ָ����Ϊ NULL
            }
        }
    }
    #else                                                  // ����ջ����������
    {
        StackType_t *pxStack;                            // ����ָ��ջ�ռ��ָ��
        pxStack = ( StackType_t * ) pvPortMalloc( ( ( ( size_t ) usStackDepth ) * sizeof( StackType_t ) ) ); // ��������ջ�ռ�

        if( pxStack != NULL )                            // ���ջ�ռ�����Ƿ�ɹ�
        {
            pxNewTCB = ( TCB_t * ) pvPortMalloc( sizeof( TCB_t ) ); // ��̬����һ��������ƿ�

            if( pxNewTCB != NULL )                       // ��� TCb �����Ƿ�ɹ�
            {
                pxNewTCB->pxStack = pxStack;          // �������ջָ�븳ֵ�� TCB
            }
            else
            {
                vPortFree( pxStack );                  // ��� TCB ����ʧ�ܣ��ͷ�֮ǰ�����ջ
            }
        }
        else
        {
            pxNewTCB = NULL;                            // ���ջ�ռ����ʧ�ܣ����� TCB ָ��Ϊ NULL
        }
    }
    #endif 

    if( pxNewTCB != NULL )                               // ��� TCB �Ƿ�ɹ�����
    {
        #if( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE != 0 )
        {
            pxNewTCB->ucStaticallyAllocated = tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB; // ���Ϊ��̬����� TCB
        }
        #endif

        // ��ʼ�������񣬲�׼�����ڲ�״̬
        prvInitialiseNewTask(pxTaskCode, pcName, ( uint32_t ) usStackDepth, pvParameters, uxPriority, pxCreatedTask, pxNewTCB, NULL );
        
        // ����������ӵ����ȶ�����
        prvAddNewTaskToReadyList(pxNewTCB);
        xReturn = pdPASS;                               // ���񴴽��ɹ������سɹ���־
    }
    else
    {
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; // ����ʧ�ܣ������ڴ治�����
    }

    return xReturn;                                     // �������񴴽��Ľ��
}


#endif /* configSUPPORT_DYNAMIC_ALLOCATION */
/*-----------------------------------------------------------*/

/**
 * @brief ��ʼ���´���������� TCB �ṹ�塣
 *
 * �ú����ڷ����� Task_t �ṹ��֮�������Ǿ�̬���Ƕ�̬�����ڳ�ʼ���ṹ���Ա��
 *
 * @param pxTaskCode �������ں���ָ�롣
 * @param pcName ��������ƣ����ڵ��Ի�ͳ�ơ�
 * @param ulStackDepth ����ջ����ȣ����ֽ���Ϊ��λ��
 * @param pvParameters ���ݸ��������Ĳ�����
 * @param uxPriority ��������ȼ���
 * @param pxCreatedTask ָ����������ָ�룬���ڷ��ش���������ľ����
 * @param pxNewTCB ָ���´�����������ƿ� (TCB) ��ָ�롣
 * @param xRegions �ڴ�������Ϣ�����Ϊ NULL ��ʹ�á�
 */
static void prvInitialiseNewTask(
    TaskFunction_t pxTaskCode,          /**< �������ں���ָ�롣 */
    const char * const pcName,          /**< ��������ƣ����ڵ��Ի�ͳ�ơ� */
    const uint32_t ulStackDepth,        /**< ����ջ����ȣ����ֽ���Ϊ��λ�� */
    void * const pvParameters,          /**< ���ݸ��������Ĳ����� */
    UBaseType_t uxPriority,             /**< ��������ȼ��� */
    TaskHandle_t * const pxCreatedTask, /**< ָ����������ָ�룬���ڷ��ش���������ľ���� */
    TCB_t *pxNewTCB,                    /**< ָ���´�����������ƿ� (TCB) ��ָ�롣 */
    const MemoryRegion_t * const xRegions /**< �ڴ�������Ϣ�����Ϊ NULL ��ʹ�á� */
) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    StackType_t *pxTopOfStack;
    UBaseType_t x;

    #if( portUSING_MPU_WRAPPERS == 1 )
    {
        /* Ӧ����������Ȩģʽ�´����� */
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

    /* �������� memset() �������Ҫ�Ļ��� */
    #if( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )
    {
        /* ����ֵ֪���ջ��Э�����ԡ� */
        ( void ) memset( pxNewTCB->pxStack, ( int ) tskSTACK_FILL_BYTE, ( size_t ) ulStackDepth * sizeof( StackType_t ) );
    }
    #endif /* ( ( configCHECK_FOR_STACK_OVERFLOW > 1 ) || ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) ) */

    /* ����ջ����ַ����ȡ����ջ�ǴӸߵ�ַ���͵�ַ�������� 80x86���������෴��
    portSTACK_GROWTH ����ʹ��������ŷ��϶˿ڵ�Ҫ�� */
    #if( portSTACK_GROWTH < 0 )
    {
        pxTopOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
        pxTopOfStack = ( StackType_t * ) ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack ) & ( ~( ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) ) ); /*lint !e923 MISRA exception. ����ָ�������֮���ת���ǲ���ʵ�ġ���С����ͨ�� portPOINTER_SIZE_TYPE ���ͽ���� */

        /* �������ջ����ַ�Ƿ������ȷ�� */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxTopOfStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );
    }
    #else /* portSTACK_GROWTH */
    {
        pxTopOfStack = pxNewTCB->pxStack;

        /* ���ջ�������Ķ����Ƿ���ȷ�� */
        configASSERT( ( ( ( portPOINTER_SIZE_TYPE ) pxNewTCB->pxStack & ( portPOINTER_SIZE_TYPE ) portBYTE_ALIGNMENT_MASK ) == 0UL ) );

        /* �������ջ��飬����Ҫջ�ռ����һ�ˡ� */
        pxNewTCB->pxEndOfStack = pxNewTCB->pxStack + ( ulStackDepth - ( uint32_t ) 1 );
    }
    #endif /* portSTACK_GROWTH */

    /* �洢���������� TCB �С� */
    for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
    {
        pxNewTCB->pcTaskName[ x ] = pcName[ x ];

        /* ����ַ�������С�� configMAX_TASK_NAME_LEN �ַ�����Ҫ�������� configMAX_TASK_NAME_LEN��
        �Է��ַ���ĩβ���ڴ治�ɷ��ʣ����䲻���ܣ��� */
        if( pcName[ x ] == 0x00 )
        {
            break;
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    /* ȷ���ַ�����ֹ���Է��ַ������ȴ��ڵ��� configMAX_TASK_NAME_LEN�� */
    pxNewTCB->pcTaskName[ configMAX_TASK_NAME_LEN - 1 ] = '\0';

    /* ����һ����������������ȷ�������������ֵ������ȥ����Ȩλ��������ڣ��� */
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

    /* ���� pxNewTCB ��Ϊ�� ListItem_t ���ݵ����ӡ�����Ϊ�������ܴ�ͨ�����лص������� TCB�� */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* �¼��б����ǰ����ȼ�˳�����С� */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxPriority ); /*lint !e961 MISRA exception ����ĳЩ�˿ڣ�ת��ֻ�Ƕ���ġ� */
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
        /* ����������������δ���õĲ����� */
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
        /* ��ʼ��������� Newlib ����ṹ�� */
        _REENT_INIT_PTR( ( &( pxNewTCB->xNewLib_reent ) ) );
    }
    #endif

    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        pxNewTCB->ucDelayAborted = pdFALSE;
    }
    #endif

    /* ��ʼ�� TCB ջ��ʹ�俴�����������Ѿ������У��������ȳ����жϡ�
    ���ص�ַ����Ϊ�������Ŀ�ʼ����һ��ջ�Ѿ���ʼ����ջ�������ͻ���¡� */
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
        /* ��������ʽ���ݾ������������ڸ��Ĵ�������������ȼ���ɾ������������ȡ�*/
        *pxCreatedTask = ( TaskHandle_t ) pxNewTCB;
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief ����������ӵ������б�
 *
 * �˺�������һ���´�����������ӵ�ϵͳ�ľ����б������ݵ�ǰ����״̬�����л���������
 *
 * @param pxNewTCB ָ���´�����������ƿ� (TCB) ��ָ�롣
 */
static void prvAddNewTaskToReadyList( TCB_t *pxNewTCB )
{
    /* ȷ���ڸ��������б�ʱ�жϲ�����������б� */
    taskENTER_CRITICAL();
    {
        uxCurrentNumberOfTasks++; /* ���ӵ�ǰ���������� */

        if( pxCurrentTCB == NULL )
        {
            /* ���û���������񣬻��������������񶼴��ڹ���״̬���򽫴�������Ϊ��ǰ���� */
            pxCurrentTCB = pxNewTCB;

            if( uxCurrentNumberOfTasks == ( UBaseType_t ) 1 )
            {
                /* ������Ǵ����ĵ�һ��������ִ������ĳ�����ʼ����
                ����˵���ʧ�ܣ����ǽ�����ʧ�ܵ��޷��ָ��� */
                prvInitialiseTaskLists();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
            }
        }
        else
        {
            /* �����������δ���У��򽫴�������Ϊ��ǰ����ǰ������������Ϊֹ������������ȼ����� */
            if( xSchedulerRunning == pdFALSE )
            {
                if( pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority )
                {
                    pxCurrentTCB = pxNewTCB;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
                }
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
            }
        }

        uxTaskNumber++; /* ��������������� */

        #if ( configUSE_TRACE_FACILITY == 1 )
        {
            /* Ϊ׷��Ŀ�ģ��� TCB �����һ���������� */
            pxNewTCB->uxTCBNumber = uxTaskNumber;
        }
        #endif /* configUSE_TRACE_FACILITY */

        traceTASK_CREATE( pxNewTCB ); /* ��¼���񴴽�����־�� */

        prvAddTaskToReadyList( pxNewTCB ); /* ��������ӵ������б� */

        portSETUP_TCB( pxNewTCB ); /* Ϊ�ض��˿����� TCB�� */
    }
    taskEXIT_CRITICAL(); /* �˳��ٽ����� */

    if( xSchedulerRunning != pdFALSE )
    {
        /* �������������ȵ�ǰ������и��ߵ����ȼ�����Ӧ�������������� */
        if( pxCurrentTCB->uxPriority < pxNewTCB->uxPriority )
        {
            taskYIELD_IF_USING_PREEMPTION(); /* ���ʹ����ռʽ���ȣ����л����������ȼ������� */
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
    }
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )
/*
 * @brief vTaskDelete ����ɾ��ָ��������
 *
 * �ú�������ɾ��ָ�������񣬰���������Ӿ����б���¼��б���ɾ�������ͷ���ص��ڴ档
 *
 * @param xTaskToDelete Ҫɾ��������ľ����
 */
void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    TCB_t *pxTCB;                                         // ����һ��ָ��������ƿ飨TCB����ָ��

    taskENTER_CRITICAL();                                 // �����ٽ���������������Դ
    {
        pxTCB = prvGetTCBFromHandle(xTaskToDelete);       // ����������ȡ������ƿ飨TCB��

        // �Ӿ����б����Ƴ�����������Ƴ����б�Ϊ�գ����������ȼ�
        if (uxListRemove(&(pxTCB->xStateListItem)) == (UBaseType_t) 0)
        {
            taskRESET_READY_PRIORITY(pxTCB->uxPriority);  // ������������ȼ�
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                     // ִ�и����ʲ��Ա�ǣ��ǹؼ������������ڵ��ԣ�
        }

        // ���������¼��б�Ϊ�գ�����¼��б����Ƴ�������
        if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL)
        {
            (void) uxListRemove(&(pxTCB->xEventListItem)); // ���¼��б����Ƴ�����
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                     // ִ�и����ʲ��Ա�ǣ��ǹؼ������������ڵ��ԣ�
        }

        uxTaskNumber++;                                   // ������������������

        if (pxTCB == pxCurrentTCB)                        // ����Ƿ�ɾ�����ǵ�ǰ�������е�����
        {
            // ����ǰ������ӵ��ȴ�����������б���
            vListInsertEnd(&xTasksWaitingTermination, &(pxTCB->xStateListItem));

            ++uxDeletedTasksWaitingCleanUp;               // ���ӵȴ��������������
            
            portPRE_TASK_DELETE_HOOK(pxTCB, &xYieldPending); // ִ������ɾ�����ӣ�����������Դ
        }
        else
        {
            --uxCurrentNumberOfTasks;                     // �ǵ�ǰ���񣬼��ٵ�ǰ��������������
            prvDeleteTCB(pxTCB);                         // ɾ��������ƿ飨TCB�����ͷ���Դ

            prvResetNextTaskUnblockTime();               // ������һ���������ʱ��
        }

        traceTASK_DELETE(pxTCB);                         // ��¼����ɾ���¼��������ã�
    }
    taskEXIT_CRITICAL();                                 // �˳��ٽ������ָ�������Դ�ķ���

    if (xSchedulerRunning != pdFALSE)                    // ���������Ƿ���������
    {
        if (pxTCB == pxCurrentTCB)                       // ���ɾ�����ǵ�ǰ���е�����
        {
            configASSERT(uxSchedulerSuspended == 0);     // ���Ե�����δ��ͣ
            portYIELD_WITHIN_API();                      // �����������л�
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                    // ִ�и����ʲ��Ա�ǣ��ǹؼ������������ڵ��ԣ�
        }
    }
}


#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelayUntil == 1 )

	/*
 * @brief vTaskDelayUntil ʹ��ǰ�����ӳٵ�ָ����ʱ���Ż��ѡ�
 *
 * �˺�������ʹ��ǰ�����ӳٵ�ָ����ʱ���Ż��ѣ�ȷ�����񰴹̶���ʱ����ִ�С�
 *
 * @param pxPreviousWakeTime ָ��һ��������ָ�룬�ñ����洢�������ϴλ��ѵ�ʱ��㡣
 *                           ���ֵ��������Ϊ�´�Ԥ�ƻ��ѵ�ʱ��㡣
 * @param xTimeIncrement ��ʾ�����´λ���ǰ��Ҫ�ȴ��ĵδ�����
 */
void vTaskDelayUntil( TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement )
{
    TickType_t xTimeToWake;                  // �´λ��ѵ�ʱ���
    BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE; // ����Ƿ�Ӧ���ӳ�

    // ����ȷ�� pxPreviousWakeTime ��Ϊ��
    configASSERT( pxPreviousWakeTime );
    // ����ȷ����ʱʱ������0
    configASSERT( ( xTimeIncrement > 0U ) );
    // ����ȷ��������δ����ͣ
    configASSERT( uxSchedulerSuspended == 0 );

    // ��ͣ�����������
    vTaskSuspendAll();
    {
        // ���浱ǰ�ĵδ����ֵ����ֹ�ڴ˿��ڷ����ı�
        const TickType_t xConstTickCount = xTickCount;

        // ����������Ҫ���ѵ�ʱ���
        xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

        // ���δ�����Ƿ����
        if( xConstTickCount < *pxPreviousWakeTime )
        {
            // ����δ���������������Ҫ��һ���ж�
            if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xConstTickCount ) )
            {
                // �������ʱ��Ҳ��������һ���ʱ����ڵ�ǰ�δ����������Ҫ�ӳ�
                xShouldDelay = pdTRUE;
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ����δ����δ���������Ҫ��һ���ж�
            if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xConstTickCount ) )
            {
                // �������ʱ���������ʱ����ڵ�ǰ�δ����������Ҫ�ӳ�
                xShouldDelay = pdTRUE;
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }

        // ���»���ʱ�䣬Ϊ�´ε�����׼��
        *pxPreviousWakeTime = xTimeToWake;

        // �����Ҫ�ӳ٣��򽫵�ǰ������ӵ��ӳ��б�
        if( xShouldDelay != pdFALSE )
        {
            // ��¼�ӳ���Ϣ�����ڸ��٣�
            traceTASK_DELAY_UNTIL( xTimeToWake );

            // ����ʵ����Ҫ�ӳٵĵδ���������ʱ���ȥ��ǰ�δ������
            prvAddCurrentTaskToDelayedList( xTimeToWake - xConstTickCount, pdFALSE );
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // �ָ��������
    xAlreadyYielded = xTaskResumeAll();

    // ��� xTaskResumeAll �Ѿ������˵��ȣ�����Ҫ�ٽ��е���
    if( xAlreadyYielded == pdFALSE )
    {
        // ǿ�ƽ��е��ȣ������ǰ�����ѽ��Լ���Ϊ˯��״̬��
        portYIELD_WITHIN_API();
    }
    else
    {
        // ���ǲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}
#endif /* INCLUDE_vTaskDelayUntil */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelay == 1 )
//�ܵ���˵�����������������ǽ�������ӵ������б�ָ������ʱ��
void vTaskDelay(const TickType_t xTicksToDelay)             // �������壬������ǰ����ָ���� Tick ��
{
    BaseType_t xAlreadyYielded = pdFALSE;                   // ��־������ָʾ�Ƿ��Ѿ������������л�

    if (xTicksToDelay > (TickType_t) 0U)                     // ����ӳ�ʱ���Ƿ���� 0
    {
        configASSERT(uxSchedulerSuspended == 0);            // ���Ե�����δ��ͣ

        vTaskSuspendAll();                                   // ��ͣ��������׼�������ӳٲ���
        {
            traceTASK_DELAY();                               // ��¼���������¼������ڵ��Ի��أ�

            prvAddCurrentTaskToDelayedList(xTicksToDelay, pdFALSE); // ����ǰ������ӵ��ӳ��б�ָ���ӳ�ʱ��
        }
        xAlreadyYielded = xTaskResumeAll();                 // �ָ���������������Ƿ���Ҫ�л�����
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();                            // ִ�и����ʲ��Ա�ǣ���ʾ�˷�֧��ִ�У�
    }

    if (xAlreadyYielded == pdFALSE)                          // ����Ƿ���Ҫ�л�����
    {
        portYIELD_WITHIN_API();                              // ���û�з��������л�����ǿ�ƽ����������л�
    }
    else
    {
        mtCOVERAGE_TEST_MARKER();                            // ִ�и����ʲ��Ա�ǣ���ʾ�˷�֧��ִ�У�
    }
}


#endif /* INCLUDE_vTaskDelay */
/*-----------------------------------------------------------*/

#if( ( INCLUDE_eTaskGetState == 1 ) || ( configUSE_TRACE_FACILITY == 1 ) )

/*
 * @brief eTaskGetState ���ڲ�ѯָ�������״̬��
 *
 * �ú���ͨ����ѯ�������ڵ��б����ж������״̬����������Ӧ��ö��ֵ��
 *
 * @param xTask Ҫ��ѯ״̬������ľ����
 *
 * @return ���������״̬��ö������Ϊ eTaskState��
 */
eTaskState eTaskGetState( TaskHandle_t xTask )
{
    eTaskState eReturn;                      // ���巵�ص�����״̬
    List_t *pxStateList;                     // ����һ��ָ���б��ָ��
    const TCB_t * const pxTCB = ( TCB_t * ) xTask;  // ��ȡ�����TCBָ��

    // ���Լ�飬ȷ���������Ƿǿյ�
    configASSERT( pxTCB );

    // �����ǰ������ָ��ľ����������е�����
    if( pxTCB == pxCurrentTCB )
    {
        eReturn = eRunning;                  // ����״̬Ϊ��������
    }
    else
    {
        // �����ٽ�������ֹ�жϷ�����ȷ��������ԭ����
        taskENTER_CRITICAL();
        {
            // ��ȡ����״̬�б������
            pxStateList = ( List_t * ) listLIST_ITEM_CONTAINER( &( pxTCB->xStateListItem ) );
        }
        // �˳��ٽ���
        taskEXIT_CRITICAL();

        // ��������Ƿ����ӳ������б���
        if( ( pxStateList == pxDelayedTaskList ) || ( pxStateList == pxOverflowDelayedTaskList ) )
        {
            eReturn = eBlocked;              // ����״̬Ϊ������
        }

        #if ( INCLUDE_vTaskSuspend == 1 )
            // ��������ڹ��������б���
            else if( pxStateList == &xSuspendedTaskList )
            {
                // ��������Ƿ����¼��б���
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL )
                {
                    eReturn = eSuspended;    // ����״̬Ϊ������
                }
                else
                {
                    eReturn = eBlocked;      // ������¼��б��������״̬Ϊ������
                }
            }
        #endif

        #if ( INCLUDE_vTaskDelete == 1 )
            // ��������ڵȴ���ֹ�������б��У������б�Ϊ��
            else if( ( pxStateList == &xTasksWaitingTermination ) || ( pxStateList == NULL ) )
            {
                eReturn = eDeleted;          // ����״̬Ϊ�ѱ�ɾ��
            }
        #endif

        else 
        {
            eReturn = eReady;                // Ĭ������״̬Ϊ����
        }
    }

    // ���������״̬
    return eReturn;
}
#endif /* INCLUDE_eTaskGetState */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

/*
 * @brief uxTaskPriorityGet ���ڻ�ȡָ����������ȼ���
 *
 * �ú������ڻ�ȡ�� `xTask` ָ������������ȼ���
 *
 * @param xTask Ҫ��ȡ���ȼ�������ľ����
 *
 * @return ������������ȼ���
 */
UBaseType_t uxTaskPriorityGet( TaskHandle_t xTask )
{
    TCB_t *pxTCB;                                       // ����ָ��������ƿ飨TCB����ָ��
    UBaseType_t uxReturn;                               // �洢���ص����ȼ�ֵ

    taskENTER_CRITICAL();                               // �����ٽ�������ֹ�ڶ�ȡ����״̬ʱ�����������л�
    {
        /* ���������л�ȡ������ƿ飨TCB���� */
        pxTCB = prvGetTCBFromHandle( xTask );

        /* ��ȡ��������ȼ��� */
        uxReturn = pxTCB->uxPriority;
    }
    taskEXIT_CRITICAL();                                // �˳��ٽ������ָ��������л�

    return uxReturn;                                    // ������������ȼ�
}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

/**
 * @brief ���жϷ��������л�ȡ��������ȼ���
 *
 * �˺����������жϷ��������а�ȫ�ػ�ȡָ����������ȼ���
 *
 * @param xTask ָ��Ҫ��ȡ���ȼ�������ľ����
 *
 * @return UBaseType_t ������������ȼ���
 */
UBaseType_t uxTaskPriorityGetFromISR( TaskHandle_t xTask )
{
    TCB_t *pxTCB;
    UBaseType_t uxReturn, uxSavedInterruptState;

    /* ȷ���ж����ȼ���Ч�� */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ���浱ǰ�ж�����״̬���������µ��ж�����״̬�� */
    uxSavedInterruptState = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        /* ����������ȡ TCB�� */
        pxTCB = prvGetTCBFromHandle( xTask );
        /* ��ȡ��������ȼ��� */
        uxReturn = pxTCB->uxPriority;
    }
    /* �ָ�֮ǰ���ж�����״̬�� */
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptState );

    /* ������������ȼ��� */
    return uxReturn;
}

#endif /* INCLUDE_uxTaskPriorityGet */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 )

/**
 * @brief ������������ȼ���
 *
 * �˺�����������ָ������������ȼ���
 *
 * @param xTask Ҫ�������ȼ�������ľ����
 * @param uxNewPriority �µ����ȼ���
 */
void vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority )
{
    TCB_t *pxTCB;
    UBaseType_t uxCurrentBasePriority, uxPriorityUsedOnEntry;
    BaseType_t xYieldRequired = pdFALSE;

    /* ȷ���µ����ȼ�����Ч�ġ� */
    configASSERT( ( uxNewPriority < configMAX_PRIORITIES ) );

    /* ȷ���µ����ȼ�����Ч�ġ� */
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
        /* ���������ǿվ��������ĵ��ǵ�������������ȼ��� */
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
            /* ���ȼ��ĸ��Ŀ��ܻ�ʹ�ȵ�ǰ����������ȼ�������׼����ִ�С� */
            if( uxNewPriority > uxCurrentBasePriority )
            {
                if( pxTCB != pxCurrentTCB )
                {
                    /* �����ǵ�ǰ������������ȼ����������������ȼ����ڵ�ǰ��������ȼ����������Ҫ�л��� */
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
                    /* ������ǰ��������ȼ��������ڵ�ǰ�����Ѿ�����߿����е����ȼ�����˲���Ҫ�л��� */
                }
            }
            else if( pxTCB == pxCurrentTCB )
            {
                /* ���͵�ǰ������������ȼ���ζ�ſ��ܴ�����һ���������ȼ�������׼��ִ�С� */
                xYieldRequired = pdTRUE;
            }
            else
            {
                /* ����������������ȼ�����Ҫ�л�����Ϊ��ǰ���е���������ȼ�һ�����ڱ��޸�����������ȼ��� */
            }

            /* ��¼����ǰ������ܱ����õľ����б��Ա� taskRESET_READY_PRIORITY ������������ */
            uxPriorityUsedOnEntry = pxTCB->uxPriority;

            #if ( configUSE_MUTEXES == 1 )
            {
                /* ֻ��������δʹ�ü̳����ȼ�������²Ÿ�����ʹ�õ����ȼ��� */
                if( pxTCB->uxBasePriority == pxTCB->uxPriority )
                {
                    pxTCB->uxPriority = uxNewPriority;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER();
                }

                /* ������ζ�Ҫ���û����ȼ��� */
                pxTCB->uxBasePriority = uxNewPriority;
            }
            #else
            {
                pxTCB->uxPriority = uxNewPriority;
            }
            #endif

            /* ֻ�����¼��б���ֵδ������������;������²�������ֵ�� */
            if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
            {
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) uxNewPriority ) ); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            /* �������������������б��У�ֻ��Ҫ���������ȼ��������ɡ�Ȼ������������ھ����б��У�����Ҫ���Ƴ�����ӵ��µľ����б� */
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxPriorityUsedOnEntry ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                /* ����ǰ��������б��� - ���Ƴ�����ӵ��µľ����б���ʹ��������ͣ�������������ٽ�����Ҳ������������ */
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    /* ��֪������������б��У���������ٴμ�飬����ֱ�ӵ��ö˿ڼ������úꡣ */
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

            /* ���˿��Ż�������ѡ��δʹ��ʱ���Ƴ�δʹ�õı������档 */
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

    // �����ٽ�������ֹ�жϷ�����ȷ��������ԭ����
    taskENTER_CRITICAL();
    {
        // ����������ȡ��Ӧ��TCBָ��
        pxTCB = prvGetTCBFromHandle( xTaskToSuspend );

        // ��¼����������־��Ϣ
        traceTASK_SUSPEND( pxTCB );

        // �ӵ�ǰ����ľ����б����Ƴ�����״̬��
        if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
        {
            // ��������ľ������ȼ�
            taskRESET_READY_PRIORITY( pxTCB->uxPriority );
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        // ���¼��б����Ƴ������¼���
        if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
        {
            ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        // ������״̬����뵽���������б��ĩβ
        vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    // �����������������
    if( xSchedulerRunning != pdFALSE )
    {
        // �ٴν����ٽ���
        taskENTER_CRITICAL();
        {
            // ������һ�����������ʱ��
            prvResetNextTaskUnblockTime();
        }
        // �˳��ٽ���
        taskEXIT_CRITICAL();
    }
    else
    {
        // ���ǲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }

    // �������������ǵ�ǰ����ִ�е�����
    if( pxTCB == pxCurrentTCB )
    {
        // �����������������
        if( xSchedulerRunning != pdFALSE )
        {
            // ���Լ�飬ȷ��û���������񱻹���
            configASSERT( uxSchedulerSuspended == 0 );
            // �������ȣ��ó�CPU����������
            portYIELD_WITHIN_API();
        }
        else
        {
            // ����������񶼱������������ǰ����ָ��
            if( listCURRENT_LIST_LENGTH( &xSuspendedTaskList ) == uxCurrentNumberOfTasks )
            {
                pxCurrentTCB = NULL;
            }
            else
            {
                // �л�����������
                vTaskSwitchContext();
            }
        }
    }
    else
    {
        // ���ǲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

/**
 * @brief ��������Ƿ��ڹ���״̬��
 *
 * �˺������ڼ����������Ƿ��Ѿ�������
 *
 * @param xTask Ҫ������������
 *
 * @return BaseType_t ������񱻹����򷵻� pdTRUE�����򷵻� pdFALSE��
 */
static BaseType_t prvTaskIsTaskSuspended( const TaskHandle_t xTask )
{
    BaseType_t xReturn = pdFALSE;
    const TCB_t * const pxTCB = ( TCB_t * ) xTask;

    /* ��Ҫ���ٽ�����ִ�У���Ϊ����� xPendingReadyList�� */

    /* ����������Ƿ���Ч�����Ҳ�Ӧ��鵱ǰ�����Ƿ񱻹��� */
    configASSERT( xTask );

    /* Ҫ�ָ��������Ƿ��ڹ����б��У� */
    if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
    {
        /* �������Ƿ��Ѿ����жϷ�������б��ָ��ˣ� */
        if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) == pdFALSE )
        {
            /* �������ڹ���״̬������Ϊû�г�ʱ���������� */
            if( listIS_CONTAINED_WITHIN( NULL, &( pxTCB->xEventListItem ) ) != pdFALSE )
            {
                xReturn = pdTRUE;
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
        }
    }
    else
    {
        mtCOVERAGE_TEST_MARKER(); /* ���븲���ʱ�ǡ� */
    }

    return xReturn;
} /*lint !e818 xTask ������ָ�� const ��ָ�룬��Ϊ����һ�����Ͷ��塣*/
#endif /* INCLUDE_vTaskSuspend */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )
//����ָ�������������ӹ����б����Ƴ�����ӽ������б�
void vTaskResume( TaskHandle_t xTaskToResume )
{
    TCB_t * const pxTCB = ( TCB_t * ) xTaskToResume;

    // ���Լ�飬ȷ�����ݵľ���Ƿǿյ�
    configASSERT( xTaskToResume );

    // �����������Ч���Ҳ��ǵ�ǰ����ִ�е�����
    if( ( pxTCB != NULL ) && ( pxTCB != pxCurrentTCB ) )
    {
        taskENTER_CRITICAL();
        {
            // ��������Ƿ��ڹ���״̬
            if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
            {
                // ��¼����ָ�����־��Ϣ
                traceTASK_RESUME( pxTCB );

                // �ӹ��������б����Ƴ�������
                ( void ) uxListRemove(  &( pxTCB->xStateListItem ) );

                // ��������ӵ�����������
                prvAddTaskToReadyList( pxTCB );

                // ����ָ����������ȼ����ڻ���ڵ�ǰִ�е��������ȼ����򴥷�����
                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                {
                    taskYIELD_IF_USING_PREEMPTION();
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ��������ǹ���״̬��ֻ�������
                mtCOVERAGE_TEST_MARKER();
            }
        }
        taskEXIT_CRITICAL();
    }
    else
    {
        // �����������Ч����ǵ�ǰ�����������
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

    // ���Լ�飬ȷ�����ݵ��������Ƿǿյ�
    configASSERT( xTaskToResume );

    // ȷ���ж����ȼ�����Ч��
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ���浱ǰ�ж�״̬���������ж�����
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        // ��������Ƿ��ڹ���״̬
        if( prvTaskIsTaskSuspended( pxTCB ) != pdFALSE )
        {
            // ��¼����ָ�����־��Ϣ
            traceTASK_RESUME_FROM_ISR( pxTCB );

            // ���������δ������
            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                // ���ָ����������ȼ��Ƿ���ڻ���ڵ�ǰִ�е��������ȼ�
                if( pxTCB->uxPriority >= pxCurrentTCB->uxPriority )
                {
                    // ����ָ����������ȼ����ߣ�����Ҫ���е���
                    xYieldRequired = pdTRUE;
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }

                // �ӹ��������б����Ƴ�������
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                // ��������ӵ�����������
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // �����������������������ӵ�������ľ����б���
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }
        }
        else
        {
            // ��������ǹ���״̬���������
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // �ָ�֮ǰ���ж�״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    // �����Ƿ���Ҫ����
    return xYieldRequired;
}

#endif /* ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) ) */
/*-----------------------------------------------------------*/

/**
 * @brief ���������������
 *
 * �˺�����������FreeRTOS�ں˵ĵ�����������������������Ͷ�ʱ��������������˶�ʱ������
 * ����ʼ���ȵ�һ������
 */
void vTaskStartScheduler( void )
{
    BaseType_t xReturn;

    /* ��������ȼ�����ӿ������� */
    #if( configSUPPORT_STATIC_ALLOCATION == 1 )
    {
        StaticTask_t *pxIdleTaskTCBBuffer = NULL;
        StackType_t *pxIdleTaskStackBuffer = NULL;
        uint32_t ulIdleTaskStackSize;

        /* ʹ���û��ṩ��RAM�������������� - ��ȡRAM��ַȻ�󴴽��������� */
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
        /* ʹ�ö�̬�����RAM�������������� */
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
        /* �ر��жϣ���ȷ���ڵ��� xPortStartScheduler() ֮ǰ���ڼ䲻�����δ�
        �����������ջ����һ���жϿ�����״̬�֣���˵���һ������ʼ����ʱ�жϻ��Զ��������á� */
        portDISABLE_INTERRUPTS();

        #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* �� Newlib �� _impure_ptr �����л���ָ���һ����������� _reent �ṹ�� */
            _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
        }
        #endif /* configUSE_NEWLIB_REENTRANT */

        xNextTaskUnblockTime = portMAX_DELAY;
        xSchedulerRunning = pdTRUE;
        xTickCount = ( TickType_t ) 0U;

        /* ��������� configGENERATE_RUN_TIME_STATS������붨�����º���������������ʱ������ʱ���׼�Ķ�ʱ��/�������� */
        portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();

        /* ���ö�ʱ���δ���Ӳ����صģ�����ڿ���ֲ�ӿ���ʵ�֡� */
        if( xPortStartScheduler() != pdFALSE )
        {
            /* ��Ӧ�õ��������Ϊ����������������У����������᷵�ء� */
        }
        else
        {
            /* ֻ�е�ĳ��������� xTaskEndScheduler() ʱ�Żᵽ����� */
        }
    }
    else
    {
        /* ����ں�δ��������������Ϊû���㹻��FreeRTOS�����������������ʱ������ */
        configASSERT( xReturn != errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY );
    }

    /* ��ֹ���������棬��� INCLUDE_xTaskGetIdleTaskHandle ������Ϊ 0������ζ�� xIdleTaskHandle �������ط�û�б�ʹ�á� */
    ( void ) xIdleTaskHandle;
}
/*-----------------------------------------------------------*/

/**
 * @brief ���������������
 *
 * �˺�������ֹͣFreeRTOS�ں˵ĵ����������ָ�ԭʼ���жϷ����������б�Ҫ����
 */
void vTaskEndScheduler( void )
{
    /* ֹͣ�������жϣ������ÿ���ֲ�ĵ��������������Ա��ڱ�Ҫʱ���Իָ�ԭʼ���жϷ������
    �˿ڲ����ȷ���ж�ʹ��λ������ȷ��״̬�� */
    portDISABLE_INTERRUPTS(); /* �ر��жϣ���ֹ������Ĳ����б��жϡ� */
    xSchedulerRunning = pdFALSE; /* ���õ�����״̬Ϊ�����С� */
    vPortEndScheduler(); /* ���ö˿��ض��Ľ��������������� */
}
/*----------------------------------------------------------*/

/**
 * @brief ��ͣ����������ȡ�
 *
 * �˺���������ͣ��������ĵ��ȣ�ֱ����Ӧ�� `xTaskResumeAll` �����á�
 * �ڴ��ڼ䣬����������֮���л������ǿ��԰�ȫ���޸������б������������ص����ݽṹ��
 */
void vTaskSuspendAll( void )
{
    /* ���ӵ�������ͣ�������������ֹ�κν�һ���������л���
    ������������ͣʱ���κγ��Ե����������л��Ĳ������ᱻ�ӳ٣�ֱ���������ָ��� */
    ++uxSchedulerSuspended;
}
/*----------------------------------------------------------*/

#if ( configUSE_TICKLESS_IDLE != 0 )

/**
 * @brief ��ȡԤ�ڵĿ���ʱ�䡣
 *
 * �˺������ڼ��㵱ǰ���񣨼����ǿ�������Ԥ�ڵĿ���ʱ�䡣
 * ����и������ȼ�������׼�������л��ߵ�ǰ�����ǿ���������Ԥ�ڵĿ���ʱ��Ϊ0��
 *
 * @return TickType_t ����Ԥ�ڵĿ���ʱ�䣨�δ�������
 */
static TickType_t prvGetExpectedIdleTime( void )
{
    TickType_t xReturn;
    UBaseType_t uxHigherPriorityReadyTasks = pdFALSE;

    /* uxHigherPriorityReadyTasks ���� configUSE_PREEMPTION Ϊ 0 �������
    ������������������У�Ҳ�����и��ڿ������ȼ��������ھ���״̬�� */
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

        /* ��ʹ�ö˿��Ż�������ѡ��ʱ��uxTopReadyPriority ������Ϊһ��λͼ��
        ������������Чλ�⻹������λ�����ã���˵�����ڸ��ڿ������ȼ��Ҵ��ھ���״̬������
        ���ﴦ����ʹ�ú���ʽ���ȵ������ */
        if( uxTopReadyPriority > uxLeastSignificantBit )
        {
            uxHigherPriorityReadyTasks = pdTRUE;
        }
    }
    #endif

    if( pxCurrentTCB->uxPriority > tskIDLE_PRIORITY )
    {
        /* �����ǰ��������ȼ����ڿ������ȼ�����Ԥ�ڵĿ���ʱ��Ϊ0�� */
        xReturn = 0;
    }
    else if( listCURRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > 1 )
    {
        /* ����������������ȼ��������ھ���״̬������һ���δ��жϱ��뱻����
        ��������ʹ��ʱ��Ƭ���ȵ������ */
        xReturn = 0;
    }
    else if( uxHigherPriorityReadyTasks != pdFALSE )
    {
        /* ���ڸ��ڿ������ȼ��Ҵ��ھ���״̬�������������ֻ�ܷ�����
        configUSE_PREEMPTION Ϊ 0 ������¡� */
        xReturn = 0;
    }
    else
    {
        /* ���û�и������ȼ�������׼�������У���ôԤ�ڵĿ���ʱ���Ǵ����ڵ���һ�������������ʱ�䡣 */
        xReturn = xNextTaskUnblockTime - xTickCount;
    }

    return xReturn;
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

/**
 * @brief �ָ�����������ȡ�
 *
 * �˺������ڻָ���������ĵ��ȣ����� `vTaskSuspendAll` �������ʹ�á�
 * ����������ĵ��ȱ���ͣ�󣬵��ô˺������Իָ����ȣ��������κο��ܷ������������л���
 *
 * @return BaseType_t ����ڻָ�����ʱ�Ѿ��������������л����򷵻� pdTRUE�����򷵻� pdFALSE��
 */
BaseType_t xTaskResumeAll( void )
{
    TCB_t *pxTCB = NULL;
    BaseType_t xAlreadyYielded = pdFALSE;

    /* ��� uxSchedulerSuspended Ϊ�㣬��˺�����ƥ��֮ǰ�� vTaskSuspendAll() ���á� */
    configASSERT( uxSchedulerSuspended );

    /* �жϿ����ڵ���������ͣʱ����һ��������¼��б����Ƴ������������ģ���ô���Ƴ������񽫱���ӵ� xPendingReadyList �б��С�
    һ���������ָ����Ϳ��԰�ȫ�ؽ����д�����ľ�������Ӹ��б��ƶ����ʵ��ľ����б� */
    taskENTER_CRITICAL();
    {
        --uxSchedulerSuspended;

        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            if( uxCurrentNumberOfTasks > ( UBaseType_t ) 0U )
            {
                /* ���κ���׼���õ�����Ӵ������б��ƶ����ʵ��ľ����б� */
                while( listLIST_IS_EMPTY( &xPendingReadyList ) == pdFALSE )
                {
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xPendingReadyList ) );
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                    prvAddTaskToReadyList( pxTCB );

                    /* ����ƶ����������ȼ����ڵ�ǰ���������ִ���������л��� */
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
                    /* ������������ͣʱ�����һ�����񱻽���������������ֹ�´ν������ʱ�䱻���¼��㣬
                    ����������¼�����������ڵ͹����޵δ�ʵ��������Ҫ�������Է�ֹ����Ҫ���˳��͹���״̬�� */
                    prvResetNextTaskUnblockTime();
                }

                /* ����ڵ���������ͣ�ڼ����κεδ�����������Ӧ���д�����ȷ���˵δ�������Ử�䣬
                ��ȷ���κ��ӳٵ���������ȷ��ʱ�䱻�ָ��� */
                {
                    UBaseType_t uxPendedCounts = uxPendedTicks; /* ����ʧ�Ը����� */

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
 * @brief ��ȡ��ǰ�ĵδ����ֵ��
 *
 * �˺������ڻ�ȡ��ǰϵͳ�ĵδ����ֵ�����ϵͳ������16λ�������ϣ�����Ҫ�ٽ��������Ա����ȡ����һ�µ����ݡ�
 *
 * @return TickType_t ���ص�ǰ�ĵδ����ֵ��
 */
TickType_t xTaskGetTickCount( void )
{
    TickType_t xTicks;

    /* ���������16λ�������ϣ�����Ҫ�ٽ���������ȷ����ȡ�δ����ʱ��һ���ԡ� */
    portTICK_TYPE_ENTER_CRITICAL();
    {
        xTicks = xTickCount;
    }
    portTICK_TYPE_EXIT_CRITICAL();

    return xTicks;
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ�������л�ȡ��ǰ�ĵδ����ֵ��
 *
 * �˺������ڴ��жϷ�������л�ȡ��ǰϵͳ�ĵδ����ֵ��Ϊ��ȷ����ȡ�ĵδ����ֵ��һ�µģ���Ҫ���ж��������н����жϡ�
 *
 * @return TickType_t ���ص�ǰ�ĵδ����ֵ��
 */
TickType_t xTaskGetTickCountFromISR( void )
{
    TickType_t xReturn;
    UBaseType_t uxSavedInterruptStatus;

    /* ֧���ж�Ƕ�׵�RTOS�˿ھ������ϵͳ���ã������API���ã��ж����ȼ��ĸ��
    �������ϵͳ�������ȼ����ж�ʼ�ձ�������״̬����ʹRTOS�ں˴����ٽ���ʱҲ����ˣ������ܵ����κ�FreeRTOS API������
    �����FreeRTOSConfig.h�ж�����configASSERT()����portASSERT_IF_INTERRUPT_PRIORITY_INVALID()���ڸ����ȼ��ж��е���FreeRTOS API����ʱ���¶���ʧ�ܡ�
    ֻ����FromISR��β��FreeRTOS�������ܴӾ��е��ڻ���ڣ��߼��ϣ����ϵͳ�������ȼ����ж��е��á�
    FreeRTOSά����һ���������жϰ�ȫAPI����ȷ���жϽ��뾡���ܿ�ͼ򵥡�
    ������Ϣ��������Cortex-M�ض��ģ����������������ҵ���http://www.freertos.org/RTOS-Cortex-M3-M4.html */
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    /* ���浱ǰ�ж�״̬���������ж������Խ����ٽ����� */
    uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
    {
        xReturn = xTickCount;
    }
    /* �ָ��ж�״̬���˳��ٽ����� */
    portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ��ȡ��ǰϵͳ�е�����������
 *
 * �˺������ڷ��ص�ǰϵͳ�д���������������
 *
 * @return UBaseType_t ��ǰϵͳ�е�����������
 */
UBaseType_t uxTaskGetNumberOfTasks( void )
{
    return uxCurrentNumberOfTasks;
}

/**
 * @brief ��ȡָ����������֡�
 *
 * �˺������ڻ�ȡָ����������֡�����������һ���ַ��������ڱ�ʶ����
 *
 * @param xTaskToQuery Ҫ��ѯ����������
 * @return char * ����ָ���������ֵ�ָ�롣
 */
char *pcTaskGetName( TaskHandle_t xTaskToQuery ) 
{
    TCB_t *pxTCB;

    /* ���������л�ȡTCBָ�롣 */
    pxTCB = prvGetTCBFromHandle( xTaskToQuery );
    
    /* ����ȷ��TCBָ����Ч�� */
    configASSERT( pxTCB );
    
    /* ������������֡� */
    return &( pxTCB->pcTaskName[ 0 ] );
}
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/**
 * @brief �ڵ����б��а����Ʋ�������
 *
 * �˺���������һ�������б��в��Ҿ���ָ�����Ƶ�����
 *
 * @param pxList Ҫ�����������б�
 * @param pcNameToQuery Ҫ��ѯ���������ơ�
 * @return TCB_t * ����ҵ�ƥ��������򷵻�ָ����TCB��ָ�룻���򷵻�NULL��
 */
static TCB_t *prvSearchForNameWithinSingleList( List_t *pxList, const char pcNameToQuery[] )
{
    TCB_t *pxNextTCB, *pxFirstTCB, *pxReturn = NULL;
    UBaseType_t x;
    char cNextChar;

    /* �˺������ڵ���������ͣ������±����õġ� */

    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        /* ��ȡ�б��е�һ��Ԫ�ص�ӵ���ߣ�TCB���� */
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        /* ���������б� */
        do
        {
            /* ��ȡ�б�����һ��Ԫ�ص�ӵ���ߣ�TCB���� */
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

            /* ��������е�ÿ���ַ���Ѱ��ƥ���ƥ�������� */
            for( x = ( UBaseType_t ) 0; x < ( UBaseType_t ) configMAX_TASK_NAME_LEN; x++ )
            {
                cNextChar = pxNextTCB->pcTaskName[ x ];

                if( cNextChar != pcNameToQuery[ x ] )
                {
                    /* �ַ���ƥ�䡣 */
                    break;
                }
                else if( cNextChar == 0x00 )
                {
                    /* �ַ�����ֹ��˵���ҵ���ƥ������� */
                    pxReturn = pxNextTCB;
                    break;
                }
                else
                {
                    /* �ַ�ƥ�䣬����û�дﵽ�ַ���ĩβ�� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }

            /* ����ҵ���ƥ�������������ѭ���� */
            if( pxReturn != NULL )
            {
                break;
            }

        } while( pxNextTCB != pxFirstTCB );
    }
    else
    {
        /* �б�Ϊ�ա� */
        mtCOVERAGE_TEST_MARKER();
    }

    return pxReturn;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetHandle == 1 )

/**
 * @brief �����������ƻ�ȡ��������
 *
 * �˺������ڸ��ݸ������������ƣ��ڸ��������б��в��Ҷ�Ӧ�����񣬲�����������
 *
 * @param pcNameToQuery Ҫ��ѯ���������ơ�
 * @return TaskHandle_t ����ҵ������򷵻�ָ����TCB�ľ�������򷵻�NULL��
 */
TaskHandle_t xTaskGetHandle( const char *pcNameToQuery )
{
    UBaseType_t uxQueue = configMAX_PRIORITIES;
    TCB_t *pxTCB;

    /* �������ƽ����ضϵ� configMAX_TASK_NAME_LEN - 1 �ֽڡ� */
    configASSERT( strlen( pcNameToQuery ) < configMAX_TASK_NAME_LEN );

    /* ��ͣ����������ȡ� */
    vTaskSuspendAll();
    {
        /* ���������б� */
        do
        {
            uxQueue--;
            pxTCB = prvSearchForNameWithinSingleList( ( List_t * ) &( pxReadyTasksLists[ uxQueue ] ), pcNameToQuery );

            if( pxTCB != NULL )
            {
                /* �ҵ��˾���� */
                break;
            }

        } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY );

        /* �����ӳ��б� */
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
                /* ���������б� */
                pxTCB = prvSearchForNameWithinSingleList( &xSuspendedTaskList, pcNameToQuery );
            }
        }
        #endif

        #if( INCLUDE_vTaskDelete == 1 )
        {
            if( pxTCB == NULL )
            {
                /* ����ɾ���б� */
                pxTCB = prvSearchForNameWithinSingleList( &xTasksWaitingTermination, pcNameToQuery );
            }
        }
        #endif
    }
    /* �ָ�����������ȡ� */
    ( void ) xTaskResumeAll();

    return ( TaskHandle_t ) pxTCB;
}

#endif /* INCLUDE_xTaskGetHandle */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/**
 * @brief ��ȡϵͳ������״̬��
 *
 * �˺������ڻ�ȡϵͳ�����������״̬����������洢��һ���û��ṩ�������С����⣬�������ṩϵͳ������ʱ�䡣
 *
 * @param pxTaskStatusArray �û��ṩ�����飬���ڴ洢ÿ�������״̬��
 * @param uxArraySize �ṩ�������С��
 * @param pulTotalRunTime �����ΪNULL���򷵻�ϵͳ������ʱ�䡣
 * @return UBaseType_t ����ʵ����д��״̬��Ϣ��Ŀ��������
 */
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray, const UBaseType_t uxArraySize, uint32_t * const pulTotalRunTime )
{
    UBaseType_t uxTask = 0, uxQueue = configMAX_PRIORITIES;

    /* ��ͣ����������ȡ� */
    vTaskSuspendAll();
    {
        /* �ṩ�������Ƿ��㹻���ϵͳ�е�ÿ������ */
        if( uxArraySize >= uxCurrentNumberOfTasks )
        {
            /* ������ÿ������״̬������Ϣ�� TaskStatus_t �ṹ�� */
            do
            {
                uxQueue--;
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &( pxReadyTasksLists[ uxQueue ] ), eReady );

            } while( uxQueue > ( UBaseType_t ) tskIDLE_PRIORITY );

            /* ������ÿ������״̬������Ϣ�� TaskStatus_t �ṹ�� */
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxDelayedTaskList, eBlocked );
            uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), ( List_t * ) pxOverflowDelayedTaskList, eBlocked );

            #if( INCLUDE_vTaskDelete == 1 )
            {
                /* ������ÿ���ѱ�ɾ������δ�����������Ϣ�� TaskStatus_t �ṹ�� */
                uxTask += prvListTasksWithinSingleList( &( pxTaskStatusArray[ uxTask ] ), &xTasksWaitingTermination, eDeleted );
            }
            #endif

            #if ( INCLUDE_vTaskSuspend == 1 )
            {
                /* ������ÿ������״̬������Ϣ�� TaskStatus_t �ṹ�� */
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
            /* ����̫С���޷����������������Ϣ�� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    /* �ָ�����������ȡ� */
    ( void ) xTaskResumeAll();

    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskGetIdleTaskHandle == 1 )

/**
 * @brief ��ȡ��������ľ����
 *
 * �˺������ڷ��ؿ�������ľ��������ڵ���������֮ǰ���ô˺������� `xIdleTaskHandle` ��Ϊ NULL��
 *
 * @return TaskHandle_t ���ؿ�������ľ����
 */
TaskHandle_t xTaskGetIdleTaskHandle( void )
{
    /* ����ڵ���������֮ǰ���ô˺������� xIdleTaskHandle ��Ϊ NULL�� */
    configASSERT( ( xIdleTaskHandle != NULL ) );
    return xIdleTaskHandle;
}

#endif /* INCLUDE_xTaskGetIdleTaskHandle */
/*----------------------------------------------------------*/

/**
 * @brief ����ָ�������ĵδ�
 *
 * �˺��������ڵδ�����һ��ʱ��֮�������δ����ֵ��ע�⣬�Ⲣ����Ϊÿһ�������ĵδ���õδ��Ӻ�����
 *
 * @param xTicksToJump Ҫ�����ĵδ�����
 */
#if ( configUSE_TICKLESS_IDLE != 0 )

void vTaskStepTick( const TickType_t xTicksToJump )
{
    /* �ڵδ�����һ��ʱ��������δ����ֵ��ע�⣬�Ⲣ����Ϊÿһ�������ĵδ���õδ��Ӻ�����
    ȷ��������ĵδ����ֵ���ᳬ����һ��������������ʱ�䡣 */
    configASSERT( ( xTickCount + xTicksToJump ) <= xNextTaskUnblockTime );
    xTickCount += xTicksToJump;
    traceINCREASE_TICK_COUNT( xTicksToJump );
}

#endif /* configUSE_TICKLESS_IDLE */
/*----------------------------------------------------------*/

#if ( INCLUDE_xTaskAbortDelay == 1 )

/**
 * @brief ��ֹ������ӳ١�
 *
 * �˺���������ֹһ�����ӳٵ�����ĵȴ�״̬�����������·��õ����������С�
 *
 * @param xTask Ҫ��ֹ�ӳٵ���������
 * @return BaseType_t ����ɹ���ֹ�ӳ٣��򷵻� pdTRUE�����򷵻� pdFALSE��
 *                   ע�⣺����������У�����ֵʼ��Ϊ pdFALSE����Ϊ�����״̬���������� eBlocked ״̬ʱ�Ÿı䡣
 */
BaseType_t xTaskAbortDelay( TaskHandle_t xTask )
{
    TCB_t *pxTCB = ( TCB_t * ) xTask;
    BaseType_t xReturn = pdFALSE;

    configASSERT( pxTCB );

    vTaskSuspendAll();
    {
        /* ֻ�е�����ʵ���ϴ��� Blocked ״̬ʱ��������ǰ�Ƴ����� */
        if( eTaskGetState( xTask ) == eBlocked )
        {
            /* �Ƴ������� Blocked �б��е����á����ڵ���������ͣ���жϲ���Ӱ�� xStateListItem�� */
            ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

            /* �����Ƿ�Ҳ�ڵȴ��¼�������ǣ�Ҳ���¼��б����Ƴ�������ʹ����������ͣ���ж�Ҳ����Ӱ���¼��б�����ʹ���ٽ����� */
            taskENTER_CRITICAL();
            {
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    pxTCB->ucDelayAborted = pdTRUE;
                }
                else
                {
                    /* δ���¼��б��С� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            taskEXIT_CRITICAL();

            /* �������������������ʵ��ľ����б��С� */
            prvAddTaskToReadyList( pxTCB );

            /* ������������񲻻����������������л��������������ռģʽ�� */
            #if (  configUSE_PREEMPTION == 1 )
            {
                /* ��ռģʽ��������ֻ�е�����������������ȼ����ڻ���ڵ�ǰ����ִ�е�����ʱ���Ż�ִ���������л��� */
                if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
                {
                    /* ����Ҫִ�е��������л��������������ָ�ʱִ�С� */
                    xYieldPending = pdTRUE;
                }
                else
                {
                    /* ���ȼ������ߣ�����Ҫ�������л��� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configUSE_PREEMPTION */
        }
        else
        {
            /* ������ Blocked ״̬�� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    xTaskResumeAll();

    return xReturn;
}
#endif /* INCLUDE_xTaskAbortDelay */
/*----------------------------------------------------------*/

/**
 * @brief ��������δ�
 *
 * �˺����ɿ���ֲ����ÿ�εδ��жϷ���ʱ���á��������ӵδ������������µĵδ�ֵ�Ƿ�����κ����������״̬��
 *
 * @return BaseType_t �����Ҫ�����������л����򷵻� pdTRUE�����򷵻� pdFALSE��
 */
BaseType_t xTaskIncrementTick( void )
{
    TCB_t * pxTCB;
    TickType_t xItemValue;
    BaseType_t xSwitchRequired = pdFALSE;

    /* ������ֲ����ÿ�εδ��жϷ���ʱ���á����ӵδ������������µĵδ�ֵ�Ƿ�����κ����������״̬�� */
    traceTASK_INCREMENT_TICK( xTickCount );
    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* С�Ż����ڴ˿��еδ��������ı䡣 */
        const TickType_t xConstTickCount = xTickCount + 1;

        /* ����RTOS�δ𣬲��ڵδ����������0ʱ�л��ӳٺ�����ӳ��б� */
        xTickCount = xConstTickCount;

        if( xConstTickCount == ( TickType_t ) 0U )
        {
            taskSWITCH_DELAYED_LISTS();
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();
        }

        /* �����εδ��Ƿ�ʹĳ����ʱ���ڡ������份��ʱ��˳��洢�ڶ����У���ζ��һ������һ�����������ʱ��δ���ڣ���û�б�Ҫ�����������е��������� */
        if( xConstTickCount >= xNextTaskUnblockTime )
        {
            for( ;; )
            {
                if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
                {
                    /* �ӳ��б�Ϊ�ա��� xNextTaskUnblockTime ����Ϊ���ֵ��ʹ���´�ͨ��ʱ��̫����ͨ������������ */
                    xNextTaskUnblockTime = portMAX_DELAY;
                    break;
                }
                else
                {
                    /* �ӳ��б�Ϊ�գ���ȡ�ӳ��б�ͷ������ֵ������λ���ӳ��б�ͷ����������������״̬�Ƴ���ʱ�䡣 */
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
                    xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

                    if( xConstTickCount < xItemValue )
                    {
                        /* �������Ƴ������ʱ�򣬵�����ֵ��λ�������б�ͷ������������Ƴ���ʱ�䣬����� xNextTaskUnblockTime �м�¼����ֵ�� */
                        xNextTaskUnblockTime = xItemValue;
                        break;
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* ��ʱ�򽫸��������״̬�Ƴ��ˡ� */
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    /* �����Ƿ�Ҳ�ڵȴ��¼�������ǣ�����¼��б����Ƴ����� */
                    if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                    {
                        ( void ) uxListRemove( &( pxTCB->xEventListItem ) );
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* �������������������ʵ��ľ����б��С� */
                    prvAddTaskToReadyList( pxTCB );

                    /* �����ռģʽ���������ҽ���������������ȼ����ڻ���ڵ�ǰ����ִ�е���������Ҫ�������л��� */
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

        /* �����ռģʽ����������Ӧ������û����ʽ�ر�ʱ��Ƭ������ͬ���ȼ������񽫹�����ʱ�䡣 */
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

        /* ��ʹ���������������δ���Ҳ�ᶨ�ڵ��á� */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            /* �ڽ⿪�ѹ���ĵδ����ʱ��ֹ�δ��ӱ����á� */
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

        /* ��ʹ���������������δ���Ҳ�ᶨ�ڵ��á� */
        #if ( configUSE_TICK_HOOK == 1 )
        {
            vApplicationTickHook();
        }
        #endif
    }

    /* ����������������л�������Ҫ�����������л��� */
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
 * @brief ���������Ӧ�ó����ǩ�����Ӻ�������
 *
 * �˺���������������Ĺ��Ӻ������ú����������ض�������±����á�
 * ����ṩ���������������ø�����Ĺ��Ӻ��������û���ṩ�������õ�ǰ����Ĺ��Ӻ�����
 *
 * @param xTask Ҫ���ù��Ӻ����������������Ϊ NULL�������õ�ǰ����Ĺ��Ӻ�����
 * @param pxHookFunction Ҫ���õĹ��Ӻ���ָ�롣
 */
void vTaskSetApplicationTaskTag( TaskHandle_t xTask, TaskHookFunction_t pxHookFunction )
{
    TCB_t *xTCB;

    /* ��� xTask Ϊ NULL�������õ�ǰ����Ĺ��Ӻ����� */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* �� TCB �б��湳�Ӻ�������Ҫ�ٽ�����������Ϊ��ֵ���Դ��жϷ��ʡ� */
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
 * @brief ��ȡ�����Ӧ�ó����ǩ�����Ӻ�������
 *
 * �˺������ڻ�ȡָ������Ĺ��Ӻ���������ṩ�������������ȡ������Ĺ��Ӻ�����
 * ���û���ṩ�����ȡ��ǰ����Ĺ��Ӻ�����
 *
 * @param xTask Ҫ��ȡ���Ӻ����������������Ϊ NULL�����ȡ��ǰ����Ĺ��Ӻ�����
 * @return TaskHookFunction_t ����ָ������Ĺ��Ӻ���ָ�롣
 */
TaskHookFunction_t xTaskGetApplicationTaskTag( TaskHandle_t xTask )
{
    TCB_t *xTCB;
    TaskHookFunction_t xReturn;

    /* ��� xTask Ϊ NULL���������ǻ�ȡ��ǰ����Ĺ��Ӻ����� */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* ���ж���Ҳ���Է��ʸ�ֵ�������Ҫ�ٽ��������� */
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
 * @brief ���������Ӧ�ó����Ӻ�����
 *
 * �˺������ڵ���ָ������Ĺ��Ӻ����������ݲ������ú���������ṩ����������
 * ����ø�����Ĺ��Ӻ��������û���ṩ������õ�ǰ����Ĺ��Ӻ�����
 *
 * @param xTask Ҫ���ù��Ӻ����������������Ϊ NULL������õ�ǰ����Ĺ��Ӻ�����
 * @param pvParameter ���ݸ����Ӻ����Ĳ�����
 * @return BaseType_t ����ɹ����ù��Ӻ������򷵻� pdPASS�����򷵻� pdFAIL��
 */
BaseType_t xTaskCallApplicationTaskHook( TaskHandle_t xTask, void *pvParameter )
{
    TCB_t *xTCB;
    BaseType_t xReturn;

    /* ��� xTask Ϊ NULL���������ǵ��õ�ǰ����Ĺ��Ӻ����� */
    if( xTask == NULL )
    {
        xTCB = ( TCB_t * ) pxCurrentTCB;
    }
    else
    {
        xTCB = ( TCB_t * ) xTask;
    }

    /* ������Ӻ������ڣ���������� */
    if( xTCB->pxTaskTag != NULL )
    {
        xReturn = xTCB->pxTaskTag( pvParameter );
    }
    else
    {
        /* ���Ӻ��������ڡ� */
        xReturn = pdFAIL;
    }

    return xReturn;
}

#endif /* configUSE_APPLICATION_TASK_TAG */
/*-----------------------------------------------------------*/

/**
 * @brief ִ���������л���
 *
 * �˺���������RTOS��ִ���������л���ѡ��һ���µ�������ȼ����������С�
 * �������������ͣ�������������л�Ϊ��ִ�С�
 */
void vTaskSwitchContext( void )
{
    if( uxSchedulerSuspended != ( UBaseType_t ) pdFALSE )
    {
        /* �����������ǰ����ͣ���������������л��� */
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

            /* ���㵱ǰ���������ʱ�䣬���ۼӵ������������ʱ���С�
            ������ʼ���е�ʱ�� ulTaskSwitchedInTime ��ʼ���㡣
            ע������û��������������Լ���ֵֻ�ڶ�ʱ�����ǰ��Ч��
            �Ը�ֵ�ı�����Ϊ�˱�����ɵ�����ʱͳ�Ƽ�����ʵ�֡� */
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

        /* ��������˼��ջ��������鵱ǰ�����ջ�Ƿ������ */
        taskCHECK_FOR_STACK_OVERFLOW();

        /* ʹ��ͨ��C���Ի�˿��Ż��Ļ�����ѡ��һ�������������С� */
        taskSELECT_HIGHEST_PRIORITY_TASK();
        traceTASK_SWITCHED_IN();

        #if ( configUSE_NEWLIB_REENTRANT == 1 )
        {
            /* ��Newlib�� _impure_ptr ָ��ǰ����ר�õ� _reent �ṹ�� */
            _impure_ptr = &( pxCurrentTCB->xNewLib_reent );
        }
        #endif /* configUSE_NEWLIB_REENTRANT */
    }
}
/*-----------------------------------------------------------*/

/*
 * �������ƣ�vTaskPlaceOnEventList
 * ��������������ǰ������¼��б�����뵽ָ�����¼��б��У��������ȼ�����
 *          ͬʱ�����ݸ����ĵȴ�ʱ�佫��ǰ������ӵ��ӳ�ִ���б��С�
 *
 * ����˵����
 * @pxEventList: ָ���¼��б��ָ�롣���б����ڴ�ŵȴ��ض��¼�������
 * @xTicksToWait: ����ȴ���ʱ�䣨�Եδ�����ʾ�������¼�������ȴ�ʱ�䵽���
 *                ���񽫴��ӳ�ִ���б����Ƴ����ָ�ִ�С�
 */

void vTaskPlaceOnEventList( List_t * const pxEventList, const TickType_t xTicksToWait )
{
    configASSERT( pxEventList ); // ���� pxEventList ָ��ǿ�

    /* �˺�����������������µ��ã�
     * - �ж��ѱ�����
     * - �������ѱ���ͣ
     * - ��ǰ���ʵĶ����ѱ�����
     */

    /* ����ǰ������ƿ� (TCB) �е��¼��б�����뵽ָ�����¼��б��У�
     * �������ȼ�˳�����У�ȷ��������ȼ����������ȱ����ѡ�
     * ע�⣺�����ڴ��ڼ䱻�����Է�ֹ�ж�ͬʱ���ʡ�*/
    vListInsert( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* ����ǰ������ӵ��ӳ�ִ���б��У������õȴ���ʱ�䡣
     * ���� pdTRUE ��ʾ����������ȴ��¼������ӳ�ִ�С�*/
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

/*
 * �������ƣ�vTaskPlaceOnUnorderedEventList
 * ��������������ǰ������¼��б�����뵽ָ���������¼��б�ĩβ�������õȴ�ʱ�䡣
 *          �˺������ڴ����¼����ʵ�֡�
 *
 * ����˵����
 * @pxEventList: ָ���¼��б��ָ�롣���б����ڴ�ŵȴ��ض��¼�������
 * @xItemValue: Ҫ�洢���¼��б����е�ֵ��
 * @xTicksToWait: ����ȴ���ʱ�䣨�Եδ�����ʾ�������¼�������ȴ�ʱ�䵽���
 *                ���񽫴��ӳ�ִ���б����Ƴ����ָ�ִ�С�
 */

void vTaskPlaceOnUnorderedEventList( List_t * pxEventList, const TickType_t xItemValue, const TickType_t xTicksToWait )
{
    configASSERT( pxEventList ); // ���� pxEventList ָ��ǿ�

    /* �˺��������ڵ���������ͣ������µ��á��������¼����ʵ�֡�*/
    configASSERT( uxSchedulerSuspended != 0 );

    /* �洢�¼��б����ֵ�������жϲ�����ʴ��ڷ�����״̬��������¼��б��
     * �������������¼��б����ǰ�ȫ�ġ�*/
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* ����ǰ������ƿ� (TCB) �е��¼��б�����õ�ָ���¼��б��ĩβ��
     * ����������¼��б��ǰ�ȫ�ģ���Ϊ�����¼���ʵ�ֵ�һ���֡���
     * �жϲ���ֱ�ӷ����¼��飨����ͨ������Ĺ��ܵ��ü�ӷ������񼶱𣩡�*/
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* ����ǰ������ӵ��ӳ�ִ���б��У������õȴ�ʱ�䡣
     * ���� pdTRUE ��ʾ����������ȴ��¼������ӳ�ִ�С�*/
    prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
}
/*-----------------------------------------------------------*/

#if( configUSE_TIMERS == 1 )

/*
 * �������ƣ�vTaskPlaceOnEventListRestricted
 * ��������������ǰ������¼��б�����뵽ָ�����¼��б�ĩβ��������ָ���ĵȴ�ʱ����Ƿ������ڵȴ���־�������ӳ١�
 *          �˺��������ں˴���ʹ�ã������ڹ���API������Ҫ���ڵ���������ͣ������µ��á�
 *
 * ����˵����
 * @pxEventList: ָ���¼��б��ָ�롣���б����ڴ�ŵȴ��ض��¼�������
 * @xTicksToWait: ����ȴ���ʱ�䣨�Եδ�����ʾ������������������ڵȴ������ֵ��Ч��
 * @xWaitIndefinitely: ���Ϊ pdTRUE�������������ڵȴ�������ʹ�� xTicksToWait ָ���ĵȴ�ʱ�䡣
 */

void vTaskPlaceOnEventListRestricted( List_t * const pxEventList, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely )
{
    configASSERT( pxEventList ); // ���� pxEventList ָ��ǿ�

    /* �˺�����Ӧ��Ӧ�ó��������ã�����������к��� 'Restricted'��
     * �����ǹ���API��һ���֣�����רΪ�ں˴�����Ƶģ���������ĵ���Ҫ�󡪡�
     * �����ڵ���������ͣ������µ��ô˺�����*/

    /* ����ǰ������ƿ� (TCB) �е��¼��б�����뵽ָ�����¼��б�ĩβ��
     * ����������£��ٶ�����Ψһһ���ȴ����¼��б������
     * ��˿���ʹ�ø���� vListInsertEnd() ���������� vListInsert��*/
    vListInsertEnd( pxEventList, &( pxCurrentTCB->xEventListItem ) );

    /* �������Ӧ�������ڵȴ��������õȴ�ʱ��Ϊһ�������ֵ��
     * ��ֵ���� prvAddCurrentTaskToDelayedList() �����ڲ���ʶ��Ϊ�������ӳ١�*/
    if( xWaitIndefinitely != pdFALSE )
    {
        xTicksToWait = portMAX_DELAY;
    }

    /* ��¼�����ӳ�ֱ��ָ����ʱ��㡣*/
    traceTASK_DELAY_UNTIL( ( xTickCount + xTicksToWait ) );

    /* ����ǰ������ӵ��ӳ�ִ���б��У������� xWaitIndefinitely �����ӳٱ�־��*/
    prvAddCurrentTaskToDelayedList( xTicksToWait, xWaitIndefinitely );
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

/*
 * �������ƣ�xTaskRemoveFromEventList
 * ������������ָ�����¼��б����Ƴ�������ȼ������񣬲�������ӳ�ִ���б����Ƴ���
 *          Ȼ������ӵ�����ִ���б��С��������������ͣ�������񱣳ִ���״̬��
 *          ֱ���������ָ���
 *
 * ����˵����
 * @pxEventList: ָ���¼��б��ָ�롣���б����ڴ�ŵȴ��ض��¼�������
 *
 * ����ֵ��
 * ������¼��б����Ƴ����������ȼ����ڵ��ô˺����������򷵻� pdTRUE��
 * ���򷵻� pdFALSE��
 */

BaseType_t xTaskRemoveFromEventList( const List_t * const pxEventList )
{
    TCB_t *pxUnblockedTCB;
    BaseType_t xReturn;

    /* �˺����������ٽ����е��á�Ҳ�������жϷ��������ڵ��ٽ����ڵ��á�*/

    /* �¼��б����ȼ�������˿����Ƴ��б��еĵ�һ��������Ϊ���϶���������ȼ��ġ�
       �Ƴ� TCB ���ӳ�ִ���б���������ӵ�����ִ���б�

       ���һ�����б���������˺�����Զ���ᱻ���á��������޸Ķ����ϵ���������
       ����ζ���ڴ˴����Ա�֤���¼��б�Ķ�ռ���ʡ�

       �����Ѿ����� pxEventList ��Ϊ�ա�*/
    pxUnblockedTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( &( pxUnblockedTCB->xEventListItem ) );

    if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
    {
        /* ���������δ����ͣ����������ӳ��б����Ƴ�������ӵ������б��С�*/
        ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
        prvAddTaskToReadyList( pxUnblockedTCB );
    }
    else
    {
        /* �������������ͣ�������񱣳ִ���״̬��ֱ���������ָ���*/
        vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
    }

    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* ������¼��б����Ƴ����������ȼ����ڵ��ô˺����������򷵻� pdTRUE��
           �������������֪���Ƿ�Ӧ������ǿ���������л���*/
        xReturn = pdTRUE;

        /* ��Ǽ��������������л����Է��û�û��ʹ�� "xHigherPriorityTaskWoken"
           �����������жϰ�ȫ�� FreeRTOS ������*/
        xYieldPending = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    #if( configUSE_TICKLESS_IDLE != 0 )
    {
        /* �������������һ���ں˶����ϣ��� xNextTaskUnblockTime ��������Ϊ
           ��������ĳ�ʱʱ�䡣�������������ԭ������������ͨ������
           xNextTaskUnblockTime ���䣬��Ϊ���������� xNextTaskUnblockTime ʱ���Զ����á�
           ���ǣ����ʹ���޽��Ŀ���ģʽ������ܸ���Ҫ���Ǿ������˯��ģʽ��
           �������������� xNextTaskUnblockTime ��ȷ��������¡�*/
        prvResetNextTaskUnblockTime();
    }
    #endif

    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * �������ƣ�xTaskRemoveFromUnorderedEventList
 * �����������������¼��б����Ƴ�ָ�������񣬲�������ӳ�ִ���б����Ƴ���
 *          Ȼ������ӵ�����ִ���б��С��˺��������¼���־��ʵ�֡�
 *
 * ����˵����
 * @pxEventListItem: ָ���¼��б����ָ�롣���б�������Ҫ�Ƴ�������
 * @xItemValue: Ҫ�洢���¼��б����е���ֵ��
 *
 * ����ֵ��
 * ������¼��б����Ƴ����������ȼ����ڵ��ô˺����������򷵻� pdTRUE��
 * ���򷵻� pdFALSE��
 */

BaseType_t xTaskRemoveFromUnorderedEventList( ListItem_t * pxEventListItem, const TickType_t xItemValue )
{
    TCB_t *pxUnblockedTCB;
    BaseType_t xReturn;

    /* �˺��������ڵ���������ͣ������µ��á��������¼���־��ʵ�֡�*/
    configASSERT( uxSchedulerSuspended != pdFALSE );

    /* ���¼��б��д洢�µ���Ŀֵ��*/
    listSET_LIST_ITEM_VALUE( pxEventListItem, xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE );

    /* ���¼���־���Ƴ��¼��б��жϲ�������¼���־��*/
    pxUnblockedTCB = ( TCB_t * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
    configASSERT( pxUnblockedTCB );
    ( void ) uxListRemove( pxEventListItem );

    /* ��������ӳ��б����Ƴ�������ӵ������б��С�����������ͣ��
       ����жϲ�����ʾ����б�*/
    ( void ) uxListRemove( &( pxUnblockedTCB->xStateListItem ) );
    prvAddTaskToReadyList( pxUnblockedTCB );

    if( pxUnblockedTCB->uxPriority > pxCurrentTCB->uxPriority )
    {
        /* ������¼��б����Ƴ����������ȼ����ڵ��ô˺����������򷵻� pdTRUE��
           �������������֪���Ƿ�Ӧ������ǿ���������л���*/
        xReturn = pdTRUE;

        /* ��Ǽ��������������л����Է��û�û��ʹ�� "xHigherPriorityTaskWoken"
           �����������жϰ�ȫ�� FreeRTOS ������*/
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
 * �������ƣ�vTaskSetTimeOutState
 * ��������������ָ����ʱ�ṹ��״̬��Ϣ���˺�����¼��ǰ�ĵδ���������������
 *          �Ա�������㳬ʱʱ�䡣
 *
 * ����˵����
 * @pxTimeOut: ָ�� TimeOut �ṹ��ָ�룬���ڼ�¼��ʱ״̬��Ϣ��
 */

void vTaskSetTimeOutState( TimeOut_t * const pxTimeOut )
{
    configASSERT( pxTimeOut ); // ���� pxTimeOut ָ��ǿ�

    /* ��¼��ǰ�ĵδ���������������*/
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/

/*
 * �������ƣ�xTaskCheckForTimeOut
 * ������������鵱ǰ�����Ƿ�ʱ�������ݽ������ʣ��ȴ�ʱ�䡣
 *          �����ʱ���򷵻� pdTRUE�����û�г�ʱ���򷵻� pdFALSE ������ʣ��ȴ�ʱ�䡣
 *
 * ����˵����
 * @pxTimeOut: ָ�� TimeOut �ṹ��ָ�룬���ڼ�¼��ʱ״̬��Ϣ��
 * @pxTicksToWait: ָ��һ��������ָ�룬�ñ�������ָ���ȴ�ʱ�䣨�Եδ�����ʾ����
 *
 * ����ֵ��
 * ��������ѳ�ʱ���򷵻� pdTRUE�����򷵻� pdFALSE��
 */

BaseType_t xTaskCheckForTimeOut( TimeOut_t * const pxTimeOut, TickType_t * const pxTicksToWait )
{
    BaseType_t xReturn;

    configASSERT( pxTimeOut ); // ���� pxTimeOut ָ��ǿ�
    configASSERT( pxTicksToWait ); // ���� pxTicksToWait ָ��ǿ�

    taskENTER_CRITICAL();
    {
        /* �ڴ˴�����У��δ��������ı䡣*/
        const TickType_t xConstTickCount = xTickCount;

        #if( INCLUDE_xTaskAbortDelay == 1 )
            if( pxCurrentTCB->ucDelayAborted != pdFALSE )
            {
                /* ����ӳٱ��жϣ�����Ϊ�����˳�ʱ������ʵ�ʲ��ǳ�ʱ�������� pdTRUE��*/
                pxCurrentTCB->ucDelayAborted = pdFALSE;
                xReturn = pdTRUE;
            }
            else
        #endif

        #if ( INCLUDE_vTaskSuspend == 1 )
            if( *pxTicksToWait == portMAX_DELAY )
            {
                /* ��������� INCLUDE_vTaskSuspend ���ҵȴ�ʱ�䱻����Ϊ���ֵ��
                   ������Ӧ�����ڵȴ�����˲��ᳬʱ��*/
                xReturn = pdFALSE;
            }
            else
        #endif

        if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xConstTickCount >= pxTimeOut->xTimeOnEntering ) )
        {
            /* �δ�������� vTaskSetTimeOut ������ʱ��ʱ�䣬���Ҵ���ʱ��Ҳ�����������
               ������� vTaskSetTimeOut ����������������һ��ʱ�䣬���¼�����������ٴ����ӡ�*/
            xReturn = pdTRUE;
        }
        else if( ( ( TickType_t ) ( xConstTickCount - pxTimeOut->xTimeOnEntering ) ) < *pxTicksToWait )
        {
            /* δ������ʱ������ʣ��ȴ�ʱ�䡣*/
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
 * �������ƣ�vTaskMissedYield
 * ������������ǵ�ǰ��������һ���������л��Ļ��ᡣ��ͨ������ָʾϵͳ��Ҫ�������һ���������л���
 *
 * ����˵����
 * ��
 */

void vTaskMissedYield( void )
{
    xYieldPending = pdTRUE; // �����Ҫ�����������л�
}
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )
/*
 * �������ƣ�uxTaskGetTaskNumber
 * ������������ȡָ������ı�š������������Ч���򷵻�����ı�ţ�
 *          ���򷵻� 0��
 *
 * ����˵����
 * @xTask: ָ��������ƿ�ľ����TaskHandle_t ���ͣ���
 *
 * ����ֵ��
 * ��������ı�ţ������������Ч���򷵻� 0��
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
 * �������ƣ�vTaskSetTaskNumber
 * ��������������ָ������ı�š������������Ч������������ı�š�
 *
 * ����˵����
 * @xTask: ָ��������ƿ�ľ����TaskHandle_t ���ͣ���
 * @uxHandle: Ҫ���õ������š�
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
 * ��������
 * -----------------------------------------------------------
 *
 * portTASK_FUNCTION() ����������˿�/�������ض���������չ��
 * �˺����ȼ۵�ԭ��Ϊ��
 *
 * void prvIdleTask( void *pvParameters );
 *
 */

static portTASK_FUNCTION( prvIdleTask, pvParameters )
{
    /* ����δʹ�õĲ������档 */
    ( void ) pvParameters;

    /** ���� RTOS �������� ���� ������������ʱ���Զ������� **/

    for( ;; )
    {
        /* ����Ƿ����κ���������ɾ��������У�������������ͷ���ɾ������� TCB ��ջ�ռ䡣 */
        prvCheckTasksWaitingTermination();

        #if ( configUSE_PREEMPTION == 0 )
        {
            /* ���û��ʹ����ռʽ���ȣ���ǿ�ƽ��������л��Բ鿴�Ƿ�������������á�
               ���ʹ����ռʽ���ȣ�����Ҫ����������Ϊ�κο��õ����񶼻��Զ���ô������� */
            taskYIELD();
        }
        #endif /* configUSE_PREEMPTION */

        #if ( ( configUSE_PREEMPTION == 1 ) && ( configIDLE_SHOULD_YIELD == 1 ) )
        {
            /* ʹ����ռʽ����ʱ����ͬ���ȼ������񽫱�ʱ��Ƭ��ת�������������ȼ����������׼�����У�
               ���������Ӧ��ʱ��Ƭ����ǰ�ó���������
               �����ڴ˴�ʹ���ٽ�������Ϊ����ֻ�Ƕ�ȡ�б�ż���Ĵ���ֵҲ����Ӱ�졣 */
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

            /* �ڿ��������е����û�����ĺ�����������Ӧ���������û�ж������������������Ӻ�̨���ܡ�
               ע�⣺vApplicationIdleHook() ���Բ��ܵ��ÿ��ܻ������ĺ����� */
            vApplicationIdleHook();
        }
        #endif /* configUSE_IDLE_HOOK */

        /* ��������Ӧ��ʹ�ò����� 0 ���жϣ������ǵ��� 1 ���жϡ�����Ϊ��ȷ��
           ���û�����ĵ͹���ģʽʵ������Ҫ�� configUSE_TICKLESS_IDLE ����Ϊ��ͬ�� 1 ��ֵʱ��
           ��Ȼ�ܹ����� portSUPPRESS_TICKS_AND_SLEEP()�� */
        #if ( configUSE_TICKLESS_IDLE != 0 )
        {
            TickType_t xExpectedIdleTime;

            /* ÿ�ο����������ʱ����ϣ������Ȼ��ָ�����������ˣ��Ƚ���һ�γ�����Ԥ�ƿ���ʱ����ԣ�
               ����������������ʱ�Ľ����һ����Ч�� */
            xExpectedIdleTime = prvGetExpectedIdleTime();

            if( xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP )
            {
                vTaskSuspendAll();
                {
                    /* ���ڵ������ѹ��𣬿����ٴβ���Ԥ�ƿ���ʱ�䣬������ο���ʹ�ø�ֵ�� */
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
 * �������ƣ�eTaskConfirmSleepModeStatus
 * ����������ȷ��ϵͳ�Ƿ���Խ���͹���ģʽ�������Ƿ�Ӧ��ֹ����͹���ģʽ��
 *          ���ݵ�ǰ�����״̬������Ӧ��ö��ֵ��
 *
 * ����˵����
 * ��
 *
 * ����ֵ��
 * ���� eSleepModeStatus ö�������е�һ��ֵ����ʾ�Ƿ���԰�ȫ�ؽ���͹���ģʽ��
 */

eSleepModeStatus eTaskConfirmSleepModeStatus( void )
{
    /* ����Ӧ�ó��������⣬����һ������������ڡ� */
    const UBaseType_t uxNonApplicationTasks = 1;
    eSleepModeStatus eReturn = eStandardSleep;

    if( listCURRENT_LIST_LENGTH( &xPendingReadyList ) != 0 )
    {
        /* �ڵ������������ڼ���һ��������Ϊ����״̬�� */
        eReturn = eAbortSleep;
    }
    else if( xYieldPending != pdFALSE )
    {
        /* �ڵ������������ڼ�������һ���������л��� */
        eReturn = eAbortSleep;
    }
    else
    {
        /* ������������ڹ����б��У��������ζ�����Ǿ������޵�����ʱ�䣬������ʵ���ϱ�����
           ��ô���԰�ȫ�عر�����ʱ�Ӳ��ȴ��ⲿ�жϡ� */
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
 * @brief ����������ֲ߳̾��洢ָ�롣
 *
 * �˺�����������һ��������ֲ߳̾��洢��TLS��ָ�롣TLS ����ÿ������ӵ���Լ���һ�����ݣ�
 * ��ʹ���������ͬһ��ȫ�����ݽṹ���˺���ȷ�� TLS ָ����ָ��������������������Ч�ġ�
 *
 * @param xTaskToSet Ҫ���� TLS ָ�����������
 * @param xIndex TLS ָ�������е�������
 * @param pvValue Ҫ���õ��� TLS ָ��ֵ��
 */
void vTaskSetThreadLocalStoragePointer( TaskHandle_t xTaskToSet, BaseType_t xIndex, void *pvValue )
{
    TCB_t *pxTCB; // ָ��������ƿ飨Task Control Block����ָ��

    // ��������Ƿ��� TLS ָ���������Ч��Χ��
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        // ����������ȡ������ƿ飨TCB��
        pxTCB = prvGetTCBFromHandle( xTaskToSet );

        // ���� TLS ָ��
        pxTCB->pvThreadLocalStoragePointers[ xIndex ] = pvValue;
    }
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( configNUM_THREAD_LOCAL_STORAGE_POINTERS != 0 )

/**
 * @brief ��ȡ������ֲ߳̾��洢ָ�롣
 *
 * �˺������ڻ�ȡһ��������ֲ߳̾��洢��TLS��ָ�롣TLS ����ÿ������ӵ���Լ���һ�����ݣ�
 * ��ʹ���������ͬһ��ȫ�����ݽṹ���˺�������ָ���������� TLS ָ��ֵ��
 *
 * @param xTaskToQuery Ҫ��ѯ TLS ָ�����������
 * @param xIndex TLS ָ�������е�������
 * @return ����ָ���������� TLS ָ��ֵ��
 */
void *pvTaskGetThreadLocalStoragePointer( TaskHandle_t xTaskToQuery, BaseType_t xIndex )
{
    void *pvReturn = NULL; // Ĭ�Ϸ���ֵΪ NULL
    TCB_t *pxTCB; // ָ��������ƿ飨Task Control Block����ָ��

    // ��������Ƿ��� TLS ָ���������Ч��Χ��
    if( xIndex < configNUM_THREAD_LOCAL_STORAGE_POINTERS )
    {
        // ����������ȡ������ƿ飨TCB��
        pxTCB = prvGetTCBFromHandle( xTaskToQuery );

        // ��ȡ TLS ָ��
        pvReturn = pxTCB->pvThreadLocalStoragePointers[ xIndex ];
    }
    else
    {
        // �������������Χ������ NULL
        pvReturn = NULL;
    }

    return pvReturn;
}

#endif /* configNUM_THREAD_LOCAL_STORAGE_POINTERS */
/*-----------------------------------------------------------*/

#if ( portUSING_MPU_WRAPPERS == 1 )

/**
 * @brief Ϊ������� MPU ����
 *
 * �˺�������Ϊһ����������ڴ汣����Ԫ��MPU������MPU ��һ��Ӳ�����ԣ�
 * ���ڱ����ڴ����򣬷�ֹ����֮����ڴ��ͻ���˺��������û�Ϊָ��������
 * �����ض����ڴ����򣬴Ӷ�ʵ���ڴ汣����
 *
 * @param xTaskToModify Ҫ�޸��� MPU ���õ������������Ϊ NULL�����޸ĵ�ǰ����
 * @param xRegions ָ�� MemoryRegion_t �ṹ�������ָ�룬������Ҫ����� MPU ����
 */
void vTaskAllocateMPURegions( TaskHandle_t xTaskToModify, const MemoryRegion_t * const xRegions )
{
    TCB_t *pxTCB; // ָ��������ƿ飨Task Control Block����ָ��

    // �������ľ��Ϊ NULL�����޸ĵ�ǰ����� MPU ����
    pxTCB = prvGetTCBFromHandle( xTaskToModify );

    // ��ָ���� MPU �������ô洢��������ƿ���
    vPortStoreTaskMPUSettings( &( pxTCB->xMPUSettings ), xRegions, NULL, 0 );
}

#endif /* portUSING_MPU_WRAPPERS */
/*-----------------------------------------------------------*/

/**
 * @brief ��ʼ�������б�
 *
 * �˺������ڳ�ʼ�������б���Щ�б����ڹ���ͬ���ȼ��������Լ��ӳ�����
 * ���������񡢵ȴ���ֹ������͹�������񡣴˺���ȷ����������б���ϵͳ����ʱ
 * ����ȷ��ʼ����
 */
static void prvInitialiseTaskLists( void )
{
    UBaseType_t uxPriority; // ����ѭ�������ȼ�����

    // �����������ȼ�����
    for( uxPriority = ( UBaseType_t ) 0U; uxPriority < ( UBaseType_t ) configMAX_PRIORITIES; uxPriority++ )
    {
        // ��ʼ��ÿ�����ȼ���Ӧ�ľ��������б�
        vListInitialise( &( pxReadyTasksLists[ uxPriority ] ) );
    }

    // ��ʼ�������ӳ������б�
    vListInitialise( &xDelayedTaskList1 );
    vListInitialise( &xDelayedTaskList2 );

    // ��ʼ�������������б�
    vListInitialise( &xPendingReadyList );

    // �������������ɾ�����ܣ����ʼ���ȴ���ֹ�������б�
    #if ( INCLUDE_vTaskDelete == 1 )
    {
        vListInitialise( &xTasksWaitingTermination );
    }
    #endif /* INCLUDE_vTaskDelete */

    // �����������������ܣ����ʼ�����������б�
    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        vListInitialise( &xSuspendedTaskList );
    }
    #endif /* INCLUDE_vTaskSuspend */

    // ���ó�ʼ���ӳ������б������ӳ������б�
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*-----------------------------------------------------------*/

/**
 * @brief ��鲢����ȴ���ֹ�������б�
 *
 * �˺������ڼ�鲢����ȴ���ֹ�������б��ú����� RTOS �Ŀ�������Idle Task��
 * ���ã�ȷ����ɾ���������ϵͳ����ȫ�������������ά��ϵͳ���ڴ����Դ����
 *
 * ע�⣺�˺���ֻ�ܴӿ���������á�
 */
static void prvCheckTasksWaitingTermination( void )
{
    #if ( INCLUDE_vTaskDelete == 1 )
    {
        BaseType_t xListIsEmpty; // ���ڼ���б��Ƿ�Ϊ�յı�־

        // ucTasksDeleted ���ڷ�ֹ vTaskSuspendAll() �ڿ��������й���Ƶ���ص���
        while( uxDeletedTasksWaitingCleanUp > ( UBaseType_t ) 0U )
        {
            // ��ͣ�����������
            vTaskSuspendAll();

            // ���ȴ���ֹ�������б��Ƿ�Ϊ��
            xListIsEmpty = listLIST_IS_EMPTY( &xTasksWaitingTermination );

            // �ָ��������
            ( void ) xTaskResumeAll();

            if( xListIsEmpty == pdFALSE )
            {
                TCB_t *pxTCB; // ָ��������ƿ飨Task Control Block����ָ��

                // �����ٽ���
                taskENTER_CRITICAL();

                {
                    // ��ȡ�ȴ���ֹ�����б�ͷ����������ƿ�
                    pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( ( &xTasksWaitingTermination ) );

                    // ���б����Ƴ���������ƿ�
                    ( void ) uxListRemove( &( pxTCB->xStateListItem ) );

                    // ���ٵ�ǰ��������
                    --uxCurrentNumberOfTasks;

                    // ���ٵȴ��������ɾ��������
                    --uxDeletedTasksWaitingCleanUp;
                }

                // �˳��ٽ���
                taskEXIT_CRITICAL();

                // ����������ƿ�
                prvDeleteTCB( pxTCB );
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }
    #endif /* INCLUDE_vTaskDelete */
}
/*-----------------------------------------------------------*/

#if( configUSE_TRACE_FACILITY == 1 )

/**
 * @brief ��ȡ�������Ϣ��
 *
 * �˺������ڻ�ȡָ���������Ϣ��������Щ��Ϣ��䵽һ�� TaskStatus_t �ṹ���С�
 * �˺��������ڼ�غ͵���Ŀ�ģ����Ի�ȡ����ľ�������ơ����ȼ���ջ��ַ�������š�
 * �����ȼ������֧�ֻ�������������ʱ������������֧������ʱͳ�ƣ�����ǰ״̬�Լ�ջ�ռ��
 * ���ˮλ�ߡ�
 *
 * @param xTask Ҫ��ȡ��Ϣ�������������Ϊ NULL�����ȡ���ô˺������������Ϣ��
 * @param pxTaskStatus ���ڴ洢������Ϣ�� TaskStatus_t �ṹ��ָ�롣
 * @param xGetFreeStackSpace ���Ϊ pdTRUE�����ȡջ�ռ�����ˮλ�ߣ����򣬲���ȡ��
 * @param eState ������������״̬��ö��ֵ�����Ϊ eInvalid�����ȡʵ��״̬��
 */
void vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t *pxTaskStatus, BaseType_t xGetFreeStackSpace, eTaskState eState )
{
    TCB_t *pxTCB; // ָ��������ƿ飨Task Control Block����ָ��

    // ��� xTask �� NULL�����ȡ���ô˺������������Ϣ
    pxTCB = prvGetTCBFromHandle( xTask );

    // ��� TaskStatus_t �ṹ��Ļ�����Ϣ
    pxTaskStatus->xHandle = ( TaskHandle_t ) pxTCB;
    pxTaskStatus->pcTaskName = ( const char * ) &( pxTCB->pcTaskName[ 0 ] );
    pxTaskStatus->uxCurrentPriority = pxTCB->uxPriority;
    pxTaskStatus->pxStackBase = pxTCB->pxStack;
    pxTaskStatus->xTaskNumber = pxTCB->uxTCBNumber;

    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        // ��������ڹ����б��У����п���ʵ�����Ǳ�������������
        if( pxTaskStatus->eCurrentState == eSuspended )
        {
            // ��ͣ�����������
            vTaskSuspendAll();

            {
                // ����¼��б����Ƿ���ĳ���б���
                if( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) != NULL )
                {
                    // ����Ϊ����״̬
                    pxTaskStatus->eCurrentState = eBlocked;
                }
            }

            // �ָ��������
            xTaskResumeAll();
        }
    }
    #endif /* INCLUDE_vTaskSuspend */

    #if ( configUSE_MUTEXES == 1 )
    {
        // ���֧�ֻ����������ȡ�����ȼ�
        pxTaskStatus->uxBasePriority = pxTCB->uxBasePriority;
    }
    #else
    {
        // �����֧�ֻ�������������ȼ���Ϊ 0
        pxTaskStatus->uxBasePriority = 0;
    }
    #endif

    #if ( configGENERATE_RUN_TIME_STATS == 1 )
    {
        // ���֧������ʱͳ�ƣ����ȡ����ʱ�������
        pxTaskStatus->ulRunTimeCounter = pxTCB->ulRunTimeCounter;
    }
    #else
    {
        // �����֧������ʱͳ�ƣ�������ʱ���������Ϊ 0
        pxTaskStatus->ulRunTimeCounter = 0;
    }
    #endif

    // ������ݵ�״̬���� eInvalid����ʹ�ô��ݵ�״̬
    if( eState != eInvalid )
    {
        pxTaskStatus->eCurrentState = eState;
    }
    else
    {
        // �����ȡʵ��״̬
        pxTaskStatus->eCurrentState = eTaskGetState( xTask );
    }

    // �����Ҫ��ȡջ�ռ�����ˮλ��
    if( xGetFreeStackSpace != pdFALSE )
    {
        #if ( portSTACK_GROWTH > 0 )
        {
            // ջ�������������
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxEndOfStack );
        }
        #else
        {
            // ջ�������������
            pxTaskStatus->usStackHighWaterMark = prvTaskCheckFreeStackSpace( ( uint8_t * ) pxTCB->pxStack );
        }
        #endif
    }
    else
    {
        // �������Ҫ��ȡջ�ռ�����ˮλ�ߣ�����Ϊ 0
        pxTaskStatus->usStackHighWaterMark = 0;
    }
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( configUSE_TRACE_FACILITY == 1 )

/**
 * @brief �ӵ��������б����г������������Ϣ��
 *
 * �˺������ڴ�һ�������б��л�ȡ�����������Ϣ��������Щ��Ϣ��䵽һ�� TaskStatus_t
 * �ṹ�������С��˺�����Ҫ�����ڲ�����������ͨ�������������������ռ��ض�״̬�µ�����������Ϣ��
 *
 * @param pxTaskStatusArray ���ڴ洢������Ϣ�� TaskStatus_t �ṹ������ָ�롣
 * @param pxList Ҫ�г����������ڵ��б�
 * @param eState ������������״̬��ö��ֵ��
 * @return �����Ѵ��������������
 */
static UBaseType_t prvListTasksWithinSingleList( TaskStatus_t *pxTaskStatusArray, List_t *pxList, eTaskState eState )
{
    volatile TCB_t *pxNextTCB, *pxFirstTCB; // ָ��������ƿ飨Task Control Block����ָ��
    UBaseType_t uxTask = 0; // ���ڼ�¼�Ѵ������������

    // ����б����Ƿ����0
    if( listCURRENT_LIST_LENGTH( pxList ) > ( UBaseType_t ) 0 )
    {
        // ��ȡ�б��һ��Ԫ�ص�������
        listGET_OWNER_OF_NEXT_ENTRY( pxFirstTCB, pxList );

        // �����б��е�ÿ�����񣬲���� TaskStatus_t �ṹ��
        do
        {
            // ��ȡ��һ��Ԫ�ص�������
            listGET_OWNER_OF_NEXT_ENTRY( pxNextTCB, pxList );

            // ��ȡ������Ϣ����䵽������
            vTaskGetInfo( ( TaskHandle_t ) pxNextTCB, &( pxTaskStatusArray[ uxTask ] ), pdTRUE, eState );

            // �����Ѵ������������
            uxTask++;
        } while( pxNextTCB != pxFirstTCB );
    }
    else
    {
        // ���븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }

    // �����Ѵ������������
    return uxTask;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) )

/**
 * @brief �������ջ��ʣ������ɿռ䡣
 *
 * �˺������ڼ������ջ��ʣ������ɿռ䡣�˺�������ջ��ֱ���ҵ���һ��������
 * tskSTACK_FILL_BYTE ���ֽ�Ϊֹ��������ջ���������� tskSTACK_FILL_BYTE ���ֽ�����
 * ��Щ�ֽ�ͨ������ջ��ʼ��ʱ�������ջ�Ŀ��пռ���ֽڡ���������������ͬ�ֽڵ�������
 * ��Ϊջ��ʣ�����ɿռ��һ�����ƶ�����
 *
 * @param pucStackByte ָ��ջ��ĳ�ֽڵ�ָ�룬ͨ����ջ����ջ�ס�
 * @return ����������ͬ�ֽڵ���������Ϊջ��ʣ�����ɿռ��һ�����ƶ�����
 */
static uint16_t prvTaskCheckFreeStackSpace( const uint8_t * pucStackByte )
{
    uint32_t ulCount = 0U; // ���ڼ���������ͬ�ֽڵ�����

    // ����ջ��ֱ���ҵ���һ�������� tskSTACK_FILL_BYTE ���ֽ�
    while( *pucStackByte == ( uint8_t ) tskSTACK_FILL_BYTE )
    {
        // ����ջ�����������ƶ�ָ��
        pucStackByte -= portSTACK_GROWTH;

        // ���Ӽ�����
        ulCount++;
    }

    // ������ֵת��Ϊ StackType_t ���͵��ֽ���
    ulCount /= ( uint32_t ) sizeof( StackType_t );

    // ����������ͬ�ֽڵ���������Ϊջ��ʣ�����ɿռ��һ�����ƶ���
    return ( uint16_t ) ulCount;
}

#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) || ( INCLUDE_uxTaskGetStackHighWaterMark == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskGetStackHighWaterMark == 1 )

//��ȡ����ջ�ĸ�ˮλ��ǣ���������ջ����ʼλ�ã������ڲ���������ջ�ĸ�ˮλ���
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask )
{
    TCB_t *pxTCB;                           // ����ָ��������ƿ��ָ��
    uint8_t *pucEndOfStack;                 // ����ָ��ջ����ջ�׵�ָ��
    UBaseType_t uxReturn;                   // ���巵��ֵ�����ڴ洢ջ�ĸ�ˮλ���

    // ����������ȡ��Ӧ��TCBָ��
    pxTCB = prvGetTCBFromHandle( xTask );

    // ����ջ��������ȷ��ջ����ʼλ��
    #if portSTACK_GROWTH < 0
    {
        // ���ջ������������С���򣩣���ջ����ʼλ���������ջ��ַ
        pucEndOfStack = ( uint8_t * ) pxTCB->pxStack;
    }
    #else
    {
        // ���ջ�������������ӷ��򣩣���ջ����ʼλ����ջ�Ľ���λ��
        pucEndOfStack = ( uint8_t * ) pxTCB->pxEndOfStack;
    }
    #endif

    // �����ڲ���������ջ�ĸ�ˮλ���
    uxReturn = ( UBaseType_t ) prvTaskCheckFreeStackSpace( pucEndOfStack );

    // ����ջ�ĸ�ˮλ���
    return uxReturn;
}

#endif /* INCLUDE_uxTaskGetStackHighWaterMark */
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelete == 1 )

	/*
 * @brief prvDeleteTCB ɾ��������ƿ飨TCB�����������Դ��
 *
 * �˺��������ͷ���������ƿ飨TCB����ص�������Դ������ջ�ռ�� TCB ����
 *
 * @param pxTCB ָ��������ƿ��ָ�롣
 */
static void prvDeleteTCB(TCB_t *pxTCB)
{
    /* ���� TCB ���ݽṹ�� */
    portCLEAN_UP_TCB(pxTCB);

    #if (configUSE_NEWLIB_REENTRANT == 1)
    {
        /* ��������� Newlib �Ŀ�����֧�֣����ͷ� Newlib �� reent �ṹ�� */
        _reclaim_reent(&(pxTCB->xNewLib_reent));
    }
    #endif /* configUSE_NEWLIB_REENTRANT */

    #if( (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 0) && (portUSING_MPU_WRAPPERS == 0) )
    {
        /* �����֧�ֶ�̬���䣬�Ҳ�֧�־�̬���䣬����û��ʹ�� MPU ��װ����
         * ���ͷ�ջ�� TCB�� */

        /* �ͷ�����ջ�� */
        vPortFree(pxTCB->pxStack);

        /* �ͷ� TCB �ṹ�� */
        vPortFree(pxTCB);
    }
    #elif( tskSTATIC_AND_DYNAMIC_ALLOCATION_POSSIBLE == 1 )
    {
        /* ���ͬʱ֧�־�̬�Ͷ�̬���䣬������Щ��Դ�Ƕ�̬����ģ����ͷ����ǡ� */

        if(pxTCB->ucStaticallyAllocated == tskDYNAMICALLY_ALLOCATED_STACK_AND_TCB)
        {
            /* ���ջ�� TCB ���Ƕ�̬����ģ����ͷ����ǡ� */

            /* �ͷ�����ջ�� */
            vPortFree(pxTCB->pxStack);

            /* �ͷ� TCB �ṹ�� */
            vPortFree(pxTCB);
        }
        else if(pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_ONLY)
        {
            /* ���ֻ��ջ�Ǿ�̬����ģ���ֻ�ͷ� TCB �ṹ�� */

            /* �ͷ� TCB �ṹ�� */
            vPortFree(pxTCB);
        }
        else
        {
            /* ���ջ�� TCB ���Ǿ�̬����ģ�����Ҫ�ͷ��κ���Դ�� */

            /* ����ȷ��ջ�� TCB ���Ǿ�̬����ġ� */
            configASSERT(pxTCB->ucStaticallyAllocated == tskSTATICALLY_ALLOCATED_STACK_AND_TCB);

            /* ���ǲ��Ա�ǡ� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}

#endif /* INCLUDE_vTaskDelete */
/*-----------------------------------------------------------*/

/**
 * @brief �����´������������ʱ�䡣
 *
 * �˺������������´������������ʱ�䡣���ӳ������б����仯ʱ��
 * �˺��������б��Ƿ�Ϊ�գ�������������� xNextTaskUnblockTime ������
 * �Ա����´μ��ʱ֪����ʱ������Ӧ�ô�����״̬��Ϊ�ɵ���״̬��
 */
static void prvResetNextTaskUnblockTime( void )
{
    TCB_t *pxTCB; // ָ��������ƿ飨Task Control Block����ָ��

    // ����ӳ������б��Ƿ�Ϊ��
    if( listLIST_IS_EMPTY( pxDelayedTaskList ) != pdFALSE )
    {
        // ����ӳ������б�Ϊ�գ����´����������ʱ������Ϊ���ֵ
        // ���������ӳ������б�Ϊ�յ�����£���������������
        // if( xTickCount >= xNextTaskUnblockTime ) ������
        xNextTaskUnblockTime = portMAX_DELAY;
    }
    else
    {
        // ����ӳ������б�Ϊ�գ����ȡ�б�ͷ����������ƿ�
        pxTCB = ( TCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );

        // ��ȡ�б�ͷ������Ľ�����ʱ��
        xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );
    }
}
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) )

/**
 * @brief ��ȡ��ǰִ������ľ����
 *
 * �˺������ڻ�ȡ��ǰִ������ľ�������ڴ˺��������ж��������е��ã�
 * ���Ҷ����κε�һ��ִ���߳���˵����ǰ��������ƿ飨TCB��������ͬ�ģ�
 * ��˲���Ҫ�����ٽ�����
 *
 * @return ���ص�ǰ����ľ����
 */
TaskHandle_t xTaskGetCurrentTaskHandle( void )
{
    TaskHandle_t xReturn; // ���ڴ洢��ǰ�������ı���

    // ����Ҫ�ٽ�����������Ϊ�˺��������ж��������е��ã�
    // ���ҵ�ǰ TCB ���Ƕ����κε�һִ���̶߳�������ͬ�ġ�
    xReturn = pxCurrentTCB;

    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetCurrentTaskHandle == 1 ) || ( configUSE_MUTEXES == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )

/**
 * @brief ��ȡ��ǰ��������״̬��
 *
 * �˺������ڻ�ȡ��ǰ��������״̬�������������Ƿ��Ѿ�������������
 * ��������״̬������Ӧ��ö��ֵ��
 *
 * @return ���ص�ǰ��������״̬ö��ֵ��
 */
BaseType_t xTaskGetSchedulerState( void )
{
    BaseType_t xReturn; // ���ڴ洢�����ص�ǰ������״̬�ı���

    // ���������Ƿ��Ѿ�����
    if( xSchedulerRunning == pdFALSE )
    {
        // �����������δ�������򷵻� taskSCHEDULER_NOT_STARTED
        xReturn = taskSCHEDULER_NOT_STARTED;
    }
    else
    {
        // ����������Ѿ����������һ������Ƿ���ͣ
        if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
        {
            // ����������������У��򷵻� taskSCHEDULER_RUNNING
            xReturn = taskSCHEDULER_RUNNING;
        }
        else
        {
            // �������������ͣ���򷵻� taskSCHEDULER_SUSPENDED
            xReturn = taskSCHEDULER_SUSPENDED;
        }
    }

    return xReturn;
}

#endif /* ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) ) */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

/**
 * @brief ʵ�����ȼ��̳л��ơ�
 *
 * �˺�������ʵ���ڶ����񻷾��У���һ�������ȼ����������һ���������������ȼ�������
 * ��ͼ��ȡ����ʱ�������ȼ�����̳и����ȼ���������ȼ����Ӷ��������ȼ���ת�����⡣
 *
 * @param pxMutexHolder ���л���������������
 */
void vTaskPriorityInherit( TaskHandle_t const pxMutexHolder )
{
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder; // ָ����л�������������ƿ飨TCB��

    // ��������������߲�Ϊ NULL�����������
    if( pxMutexHolder != NULL )
    {
        // ������л��������������ȼ�������ͼ��ȡ����������������ȼ�
        if( pxTCB->uxPriority < pxCurrentTCB->uxPriority )
        {
            // ���������������ߵ�״̬���Է�ӳ���µ����ȼ�
            // ֻ�����б���ֵδ������������;ʱ�������¼��б���ֵ
            if( ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) == 0UL )
            {
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority );
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }

            // ������޸ĵ������ھ���״̬������Ҫ�����ƶ����µ��б���
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                // �ӵ�ǰ���ȼ��ľ��������б����Ƴ�����
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    // ���븲���ʲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }

                // �̳������ȼ�ǰ�Ƚ������ƶ����µ��б���
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // ֻ�̳������ȼ�
                pxTCB->uxPriority = pxCurrentTCB->uxPriority;
            }

            // ��¼���ȼ��̳е���־
            traceTASK_PRIORITY_INHERIT( pxTCB, pxCurrentTCB->uxPriority );
        }
        else
        {
            // ���븲���ʲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // ���������������Ϊ NULL���������븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

/**
 * @brief ʵ�����ȼ��黹���ơ�
 *
 * �˺�������ʵ�����ȼ��黹���ơ���һ�������ȼ������ͷ���һ�������еĻ�������
 * �������ȼ�����֮ǰΪ�˻�ȡ�û��������̳��˸����ȼ�����ʱ�õ����ȼ�����Ӧ��
 * �黹��ԭ�������ȼ���
 *
 * @param pxMutexHolder ���л���������������
 * @return �������黹�˼̳е����ȼ����򷵻� pdTRUE�����򷵻� pdFALSE��
 */
BaseType_t xTaskPriorityDisinherit( TaskHandle_t const pxMutexHolder )
{
    TCB_t * const pxTCB = ( TCB_t * ) pxMutexHolder; // ָ����л�������������ƿ飨TCB��
    BaseType_t xReturn = pdFALSE; // Ĭ�Ϸ���ֵΪ pdFALSE

    // ��������������߲�Ϊ NULL�����������
    if( pxMutexHolder != NULL )
    {
        // ���ԣ����л���������������ǵ�ǰ�������е�����
        configASSERT( pxTCB == pxCurrentTCB );

        // ���ԣ����л�����������������ٳ���һ��������
        configASSERT( pxTCB->uxMutexesHeld );

        // ���ٳ��л������ļ���
        ( pxTCB->uxMutexesHeld )--;

        // �����л������������Ƿ�̳�����һ����������ȼ�
        if( pxTCB->uxPriority != pxTCB->uxBasePriority )
        {
            // ֻ����û������������������ʱ���ܹ黹���ȼ�
            if( pxTCB->uxMutexesHeld == ( UBaseType_t ) 0 )
            {
                // �ӵ�ǰ���ȼ��ľ��������б����Ƴ�����
                if( uxListRemove( &( pxTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
                {
                    taskRESET_READY_PRIORITY( pxTCB->uxPriority );
                }
                else
                {
                    // ���븲���ʲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }

                // �黹���ȼ�ǰ�ȼ�¼��־
                traceTASK_PRIORITY_DISINHERIT( pxTCB, pxTCB->uxBasePriority );

                // �黹ԭ�������ȼ�
                pxTCB->uxPriority = pxTCB->uxBasePriority;

                // �����¼��б���ֵ�����������������в��ұ����������ͷŻ�����ʱ��
                // �����������κ�����Ŀ�ġ�
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxTCB->uxPriority );

                // ��������ӵ��µľ����б���
                prvAddTaskToReadyList( pxTCB );

                // ���� pdTRUE ��ʾ��Ҫ�����������л�
                // �������ֻ�ڶ�������������в��һ��������ͷ�˳��ͬ�ڻ�ȡ˳��ʱ������
                // ��ʹû������ȴ���һ���������������һ�����������ͷ�ʱҲӦ�ý����������л���
                xReturn = pdTRUE;
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���븲���ʲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // ���������������Ϊ NULL���������븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }

    return xReturn;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

/**
 * @brief �����ٽ�����
 *
 * �˺������ڽ���һ���ٽ������ڸ������ڲ������жϷ�������ȷ��ĳЩ�ؼ������ܹ�ԭ�ӵ���ɡ�
 * �������ٽ���ʱ���жϻᱻ���ã����ҵ�ǰ������ٽ���Ƕ�׼��������ӡ�
 */
void vTaskEnterCritical( void )
{
    // �����ж�
    portDISABLE_INTERRUPTS();

    if( xSchedulerRunning != pdFALSE )
    {
        // ���ӵ�ǰ������ٽ���Ƕ�׼���
        ( pxCurrentTCB->uxCriticalNesting )++;

        // �ⲻ���жϰ�ȫ�汾�Ľ����ٽ����������������������ж������ĵ��ã������ʧ�ܡ�
        // ֻ���� "FromISR" ��β�� API �����������ж���ʹ�á�
        // ֻ���ٽ���Ƕ�׼���Ϊ 1 ʱ���ж��ԣ��Է�ֹ�ݹ����ʱ���Ժ�������Ҳʹ���ٽ����������
        if( pxCurrentTCB->uxCriticalNesting == 1 )
        {
            portASSERT_IF_IN_ISR();
        }
    }
    else
    {
        // ���븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( portCRITICAL_NESTING_IN_TCB == 1 )

/**
 * @brief �˳��ٽ�����
 *
 * �˺��������˳�һ���ٽ������ڸ������ڲ������жϷ��������˳��ٽ���ʱ��
 * ��ǰ������ٽ���Ƕ�׼�������٣���������������һ��Ƕ�׼������жϻᱻ�������á�
 */
void vTaskExitCritical( void )
{
    if( xSchedulerRunning != pdFALSE )
    {
        // ��鵱ǰ������ٽ���Ƕ�׼����Ƿ����0
        if( pxCurrentTCB->uxCriticalNesting > 0U )
        {
            // ���ٵ�ǰ������ٽ���Ƕ�׼���
            ( pxCurrentTCB->uxCriticalNesting )--;

            // ����������һ��Ƕ�׼��������������ж�
            if( pxCurrentTCB->uxCriticalNesting == 0U )
            {
                portENABLE_INTERRUPTS();
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���븲���ʲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // ���븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* portCRITICAL_NESTING_IN_TCB */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

/**
 * @brief ����������д�뻺����������ĩβ���ո�
 *
 * �˺������ڽ���������д��ָ���Ļ������������ַ���ĩβ���ո�
 * ��ȷ����ӡ���ʱ�ж��롣�˺���ͨ�����ڸ�ʽ����������ڵ��Ի�ͳ����Ϣ��ʾ�С�
 *
 * @param pcBuffer ָ��Ŀ�껺������ָ�롣
 * @param pcTaskName ָ�����������ַ�����ָ�롣
 * @return ����ָ�򻺳���ĩβ����ָ�롣
 */
static char *prvWriteNameToBuffer( char *pcBuffer, const char *pcTaskName )
{
    size_t x;

    // ���ȸ��������ַ�����������
    strcpy( pcBuffer, pcTaskName );

    // ���ַ���ĩβ���ո���ȷ����ӡ���ʱ�ж���
    for( x = strlen( pcBuffer ); x < ( size_t )( configMAX_TASK_NAME_LEN - 1 ); x++ )
    {
        pcBuffer[ x ] = ' ';
    }

    // ����ַ�����ֹ��
    pcBuffer[ x ] = 0x00;

    // ����ָ�򻺳���ĩβ����ָ��
    return &( pcBuffer[ x ] );
}

#endif /* ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) */
/*-----------------------------------------------------------*/

#if ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

/**
 * @brief ���ɰ������л����״̬��Ϣ�Ŀɶ��ַ�����
 *
 * �˺�����������һ���������л�����״̬��Ϣ�Ŀɶ��ַ����������ȵ���
 * uxTaskGetSystemState ��ȡϵͳ��״̬���ݣ�Ȼ���ʽ���������Ϊ����ɶ��ı����ʽ��
 * չʾ�������ơ�״̬��ջʹ���������Ϣ��
 *
 * @param pcWriteBuffer ָ�����ڴ������ַ����Ļ�������
 */
void vTaskList( char * pcWriteBuffer )
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    char cStatus;

    // ���д����������ֹ�����ַ�������
    *pcWriteBuffer = 0x00;

    // ��ȡ��ǰ����������
    uxArraySize = uxCurrentNumberOfTasks;

    // �����㹻��С������������ÿ�������״̬��Ϣ
    // ע�⣺��� configSUPPORT_DYNAMIC_ALLOCATION ����Ϊ 0���� pvPortMalloc() ���ȼ��� NULL
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    if( pxTaskStatusArray != NULL )
    {
        // ���ɶ���������
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, NULL );

        // ������������ת��������ɶ��ı����ʽ
        for( x = 0; x < uxArraySize; x++ )
        {
            // ��������ǰ״̬�����ַ���־
            switch( pxTaskStatusArray[ x ].eCurrentState )
            {
                case eReady:      cStatus = tskREADY_CHAR; break;
                case eBlocked:    cStatus = tskBLOCKED_CHAR; break;
                case eSuspended:  cStatus = tskSUSPENDED_CHAR; break;
                case eDeleted:    cStatus = tskDELETED_CHAR; break;
                default:          // ��ֹ��̬������
                                cStatus = 0x00; break;
            }

            // ����������д���ַ���������ĩβ���ո��Ա�������Ա����ʽ��ӡ
            pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

            // д��������ַ�����Ϣ
            sprintf( pcWriteBuffer, "\t%c\t%u\t%u\t%u\r\n", cStatus, ( unsigned int ) pxTaskStatusArray[ x ].uxCurrentPriority, ( unsigned int ) pxTaskStatusArray[ x ].usStackHighWaterMark, ( unsigned int ) pxTaskStatusArray[ x ].xTaskNumber );
            pcWriteBuffer += strlen( pcWriteBuffer );
        }

        // �ͷŷ���������ڴ�
        // ע�⣺��� configSUPPORT_DYNAMIC_ALLOCATION Ϊ 0���� vPortFree() ��������Ϊ��
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        // ������븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}
#endif /* ( ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*----------------------------------------------------------*/

#if ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) )

/**
 * @brief ��ȡ�������������ʱ��ͳ�����ݣ�����ʽ�������ָ���Ļ�������
 * 
 * �˺������ڻ�ȡ��ǰϵͳ���������������ʱ��ͳ�����ݣ�������Щͳ������
 * ��ʽ����һ���׶��ı����ʽ�������ָ���Ļ������С���Ҫ���ܰ�����
 * 1. ��ȡ����״̬��
 * 2. ����������ʱ�䣻
 * 3. ��ʽ�������
 * 4. �ڴ����
 * 
 * @param pcWriteBuffer ָ���Ļ����������ڴ洢��ʽ����ͳ�����ݡ�
 */
void vTaskGetRunTimeStats( char *pcWriteBuffer )
{
    TaskStatus_t *pxTaskStatusArray; // ���ڴ洢����״̬������ָ��
    volatile UBaseType_t uxArraySize, x; // ���ڴ洢�����С��ѭ�������ı���
    uint32_t ulTotalTime, ulStatsAsPercentage; // �洢������ʱ��Ͱٷֱȵı���

    // ȷ�������� trace facility
    #if( configUSE_TRACE_FACILITY != 1 )
    {
        #error configUSE_TRACE_FACILITY ������ FreeRTOSConfig.h ������Ϊ 1 ��ʹ�� vTaskGetRunTimeStats()��
    }
    #endif

    /*
     * ��ע�⣺
     *
     * �˺��������ڷ��㣬���������ʾӦ�ó���ʹ�á���Ҫ������Ϊ���ȳ����һ���֡�
     *
     * vTaskGetRunTimeStats() ���� uxTaskGetSystemState()��Ȼ�󽫲��� uxTaskGetSystemState() �����ʽ��Ϊһ������ɶ��ı�񣬸ñ����ʾ��ÿ�������� Running ״̬��ʱ�䣨����ʱ��Ͱٷֱȣ���
     *
     * vTaskGetRunTimeStats() ������ C �⺯�� sprintf()������ܻ����Ӵ����С��ʹ�ô�����ջ�������ڲ�ͬƽ̨���ṩ��ͬ�Ľ������Ϊ������������ FreeRTOS/Demo ��Ŀ¼�е� printf-stdarg.c �ļ��ṩ�� printf() �� va_args ��С�͡������������޹���ʵ�֣�ע�� printf-stdarg.c ���ṩ������ snprintf() ʵ�֣�����
     *
     * ��������ϵͳֱ�ӵ��� uxTaskGetSystemState() �Ի�ȡԭʼͳ�����ݣ������Ǽ��ͨ�� vTaskGetRunTimeStats() ���á�
     */

    // ȷ��д�������������ַ���
    *pcWriteBuffer = 0x00;

    // ��ȡ��ǰ���������Ŀ��գ��Է��ڴ˺���ִ���ڼ䷢���仯
    uxArraySize = uxCurrentNumberOfTasks;

    // Ϊÿ���������һ������������ע�⣡��� configSUPPORT_DYNAMIC_ALLOCATION ����Ϊ 0���� pvPortMalloc() ����ͬ�� NULL��
    pxTaskStatusArray = pvPortMalloc( uxCurrentNumberOfTasks * sizeof( TaskStatus_t ) );

    if( pxTaskStatusArray != NULL )
    {
        // ���ɶ���������
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

        // ���ڰٷֱȼ���
        ulTotalTime /= 100UL;

        // ����������
        if( ulTotalTime > 0 )
        {
            // �Ӷ��������ݴ�������ɶ��ı��
            for( x = 0; x < uxArraySize; x++ )
            {
                // ����ʹ���˶��ٰٷֱȵ�������ʱ�䣿
                // �⽫ʼ������ȡ������ӽ���������
                // ulTotalRunTimeDiv100 �Ѿ����� 100��
                ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;

                // ����������д���ַ��������ո��Ա�������Ա����ʽ��ӡ��
                pcWriteBuffer = prvWriteNameToBuffer( pcWriteBuffer, pxTaskStatusArray[ x ].pcTaskName );

                if( ulStatsAsPercentage > 0UL )
                {
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        // ʹ�ó����͸�ʽ�����
                        sprintf( pcWriteBuffer, "\t%lu\t\t%lu%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter, ulStatsAsPercentage );
                    }
                    #else
                    {
                        // sizeof(int) == sizeof(long)�����Կ���ʹ�ý�С�� printf() ��
                        sprintf( pcWriteBuffer, "\t%u\t\t%u%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter, ( unsigned int ) ulStatsAsPercentage );
                    }
                    #endif
                }
                else
                {
                    // ����ٷֱ�Ϊ�㣬���ʾ�������ĵ�ʱ������������ʱ���1%��
                    #ifdef portLU_PRINTF_SPECIFIER_REQUIRED
                    {
                        // ʹ�ó����͸�ʽ�����
                        sprintf( pcWriteBuffer, "\t%lu\t\t<1%%\r\n", pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #else
                    {
                        // sizeof(int) == sizeof(long)�����Կ���ʹ�ý�С�� printf() ��
                        sprintf( pcWriteBuffer, "\t%u\t\t<1%%\r\n", ( unsigned int ) pxTaskStatusArray[ x ].ulRunTimeCounter );
                    }
                    #endif
                }

                // ����д������ָ��
                pcWriteBuffer += strlen( pcWriteBuffer );
            }
        }
        else
        {
            // ���븲���ʲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        // �ͷ����顣ע�⣡��� configSUPPORT_DYNAMIC_ALLOCATION Ϊ 0���� vPortFree() ��������Ϊ�ա�
        vPortFree( pxTaskStatusArray );
    }
    else
    {
        // ���븲���ʲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }
}

#endif /* ( ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 ) ) */
/*-----------------------------------------------------------*/

/**
 * @brief ���õ�ǰ�����¼��б����ֵ��
 *
 * �˺����������õ�ǰ�����¼��б����ֵ�����Ȼ�ȡ��ǰ�����¼��б����ֵ��
 * Ȼ�󽫸��б�������Ϊ��Ĭ��ֵ���Ա��������ٴ����ڶ��к��ź���������
 *
 * @return ����ԭ�����¼��б���ֵ��
 */
TickType_t uxTaskResetEventItemValue( void )
{
    TickType_t uxReturn;

    // ��ȡ��ǰ�����¼��б����ֵ
    uxReturn = listGET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ) );

    // �����¼��б��������ֵ���Ա��������ٴ����ڶ��к��ź�������
    listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xEventListItem ), ( ( TickType_t ) configMAX_PRIORITIES - ( TickType_t ) pxCurrentTCB->uxPriority ) );

    return uxReturn;
}
/*-----------------------------------------------------------*/

#if ( configUSE_MUTEXES == 1 )

/**
 * @brief ���ӵ�ǰ������еĻ���������������
 *
 * �˺��������ڵ�ǰ������л�����ʱ���Ӹ�������еĻ���������������
 * �˺���ͨ���ڻ��������ɹ���ȡʱ���ã��Ը��ٵ�ǰ���������еĻ�����������
 *
 * @return ���ص�ǰ����Ŀ��ƿ�ָ�루TCB���������ǰû�����񣨼���������δ��ʼ���У����򷵻� NULL��
 */
void *pvTaskIncrementMutexHeldCount( void )
{
    // �����������δ��ʼ���л�����δ���������� pxCurrentTCB Ϊ NULL��
    if( pxCurrentTCB != NULL )
    {
        // ���ӵ�ǰ������еĻ�������������
        ( pxCurrentTCB->uxMutexesHeld )++;
    }

    // ���ص�ǰ����Ŀ��ƿ�ָ�루TCB��
    return pxCurrentTCB;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief �ӵ�ǰ������ȡ��һ��ֵ֪ͨ��
 *
 * �˺������ڴӵ�ǰ������ȡ��һ��ֵ֪ͨ���˺�������ǰ����ȴ�ֱ�����յ�һ��֪ͨ��
 * �����������ص�ǰ��ֵ֪ͨ����������� xClearCountOnExit ���������ڷ��غ�����֪ͨ������
 *
 * @param xClearCountOnExit ���Ϊ pdTRUE�����ڷ���ʱ���֪ͨ����ֵ��
 * @param xTicksToWait �ȴ���ʱ�䣨�Եδ�Ϊ��λ�������Ϊ 0���򲻻������ȴ���
 * @return ���ص�ǰ�����ֵ֪ͨ��
 */
uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait )
{
    uint32_t ulReturn;

    // �����ٽ���
    taskENTER_CRITICAL();
    {
        // ���֪ͨ�����Ѿ��Ƿ��㣬����Ҫ�ȴ�
        if( pxCurrentTCB->ulNotifiedValue == 0UL )
        {
            // ��ǵ�ǰ����Ϊ�ȴ�֪ͨ״̬
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            if( xTicksToWait > ( TickType_t ) 0 )
            {
                // ����ǰ������ӵ��ӳ��б��У������õȴ�ʱ��
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );
                traceTASK_NOTIFY_TAKE_BLOCK();

                // �������ٽ����ڽ��е���
                portYIELD_WITHIN_API();
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���븲���ʲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    // �ٴν����ٽ���
    taskENTER_CRITICAL();
    {
        // ��¼֪ͨȡ������־
        traceTASK_NOTIFY_TAKE();
        ulReturn = pxCurrentTCB->ulNotifiedValue;

        if( ulReturn != 0UL )
        {
            if( xClearCountOnExit != pdFALSE )
            {
                // ���֪ͨ����ֵ
                pxCurrentTCB->ulNotifiedValue = 0UL;
            }
            else
            {
                // ������������ֵ�����ȥ��ȡ����֪ͨ��
                pxCurrentTCB->ulNotifiedValue = ulReturn - 1;
            }
        }
        else
        {
            // ���븲���ʲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        // ���õ�ǰ����Ϊ���ٵȴ�֪ͨ״̬
        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
    }
    // �ٴ��˳��ٽ���
    taskEXIT_CRITICAL();

    return ulReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/*
 * @brief xTaskNotifyWait ���ڵȴ��������֪ͨ��
 *
 * �ú������ڵȴ�������յ�һ��֪ͨ�������ָ����ʱ���ڽ��յ�֪ͨ���򷵻� pdTRUE��
 * ���������ʱ���򷵻� pdFALSE��
 *
 * @param ulBitsToClearOnEntry �ڽ���ȴ�ǰ���������ֵ֪ͨ�е���Щλ��
 * @param ulBitsToClearOnExit ���˳��ȴ�ǰ���������ֵ֪ͨ�е���Щλ��
 * @param pulNotificationValue ָ�룬ָ�����ֵ֪ͨ�ı��������Ϊ NULL���򲻷���ֵ֪ͨ��
 * @param xTicksToWait �ȴ�ʱ�䣨�� tick Ϊ��λ�������Ϊ 0 �� pdMS_TO_TICKS(0)���򲻵ȴ���
 *
 * @return ���� pdTRUE ��ʾ�ѽ��յ�֪ͨ��pdFALSE ��ʾ��ʱ��û�н��յ�֪ͨ��
 */
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry, uint32_t ulBitsToClearOnExit, uint32_t *pulNotificationValue, TickType_t xTicksToWait )
{
    BaseType_t xReturn;

    taskENTER_CRITICAL(); // �����ٽ���
    {
        /* ֻ����û��֪ͨ�����������²������� */
        if( pxCurrentTCB->ucNotifyState != taskNOTIFICATION_RECEIVED )
        {
            /* �������ֵ֪ͨ�����ָ����λ�������������ֵ���㡣 */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnEntry;

            /* ��Ǵ�����Ϊ�ȴ�֪ͨ�� */
            pxCurrentTCB->ucNotifyState = taskWAITING_NOTIFICATION;

            if( xTicksToWait > ( TickType_t ) 0 )
            {
                /* ��������˵ȴ�ʱ�䣬�򽫵�ǰ������ӵ��ӳ������б��С� */
                prvAddCurrentTaskToDelayedList( xTicksToWait, pdTRUE );

                traceTASK_NOTIFY_WAIT_BLOCK(); // ��¼����ȴ�֪ͨ����־

                /* ���ж˿ڶ������ڴ��ٽ����ڽ��е��ȣ���Щ�˿ڻ��������ȣ���Щ��ȵ��ٽ����˳����������ⲻ��Ӧ�ó������Ӧ�����ġ� */
                portYIELD_WITHIN_API();
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // ���Ը����ʱ��
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER(); // ���Ը����ʱ��
        }
    }
    taskEXIT_CRITICAL(); // �˳��ٽ���

    taskENTER_CRITICAL(); // �ٴν����ٽ���
    {
        traceTASK_NOTIFY_WAIT(); // ��¼����ȴ�֪ͨ��ɵ���־

        if( pulNotificationValue != NULL )
        {
            /* �����ǰ��ֵ֪ͨ����ֵ�����Ѿ��ı䡣 */
            *pulNotificationValue = pxCurrentTCB->ulNotifiedValue;
        }

        /* ��� ucNotifyValue �Ѿ����ã���Ҫô�����δ��������״̬����Ϊ����֪ͨ��������Ҫô�������յ�֪ͨ���������������������ʱ����������� */
        if( pxCurrentTCB->ucNotifyState == taskWAITING_NOTIFICATION )
        {
            /* δ�յ�֪ͨ�� */
            xReturn = pdFALSE;
        }
        else
        {
            /* ֪ͨ�Ѵ������ȴ��ڼ��յ���֪ͨ�� */
            pxCurrentTCB->ulNotifiedValue &= ~ulBitsToClearOnExit;
            xReturn = pdTRUE;
        }

        pxCurrentTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION; // ��������ٵȴ�֪ͨ
    }
    taskEXIT_CRITICAL(); // �ٴ��˳��ٽ���

    return xReturn; // ���ؽ��
}
#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )
/*
 * @brief xTaskGenericNotify ������ָ��������֪ͨ��
 *
 * �ú�����������ָ�������ֵ֪ͨ��������ӵȴ��б����Ƴ�����������б�������Ƿ���Ҫ�л�����
 *
 * @param xTaskToNotify Ҫ֪ͨ������ľ����
 * @param ulValue ֪ͨ��ֵ��
 * @param eAction ֪ͨ�Ķ������͡�
 * @param pulPreviousNotificationValue ָ�룬���ڴ��֮ǰ��ֵ֪ͨ�����Ϊ NULL���򲻱���֮ǰ��ֵ��
 *
 * @return ���� pdPASS ��ʾ�ɹ���pdFAIL ��ʾʧ�ܡ�
 */
BaseType_t xTaskGenericNotify(TaskHandle_t xTaskToNotify,
                               uint32_t ulValue,
                               eNotifyAction eAction,
                               uint32_t *pulPreviousNotificationValue)
{
    TCB_t *pxTCB;                                     // ����ָ��������ƿ飨TCB����ָ��
    BaseType_t xReturn = pdPASS;                      // ����״̬����ʼ��Ϊ�ɹ�
    uint8_t ucOriginalNotifyState;                    // ��������ԭʼ֪ͨ״̬

    configASSERT(xTaskToNotify);                      // ���� xTaskToNotify ��Ч����Ϊ NULL��
    pxTCB = (TCB_t *) xTaskToNotify;                  // ��������ת��Ϊ������ƿ飨TCB��

    taskENTER_CRITICAL();                             // �����ٽ�������ֹ���޸�����״̬ʱ�����������л�
    {
        if (pulPreviousNotificationValue != NULL)     // ����Ƿ���Ҫ���֮ǰ��ֵ֪ͨ
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue; // ���浱ǰ��ֵ֪ͨ
        }

        ucOriginalNotifyState = pxTCB->ucNotifyState; // ��ȡ��ǰ�����֪ͨ״̬

        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED; // ��������Ϊ����֪ͨ״̬

        switch (eAction)                              // ����֪ͨ�������ʹ���֪ͨ
        {
            case eSetBits:
                pxTCB->ulNotifiedValue |= ulValue;    // ����ָ����λ
                break;

            case eIncrement:
                pxTCB->ulNotifiedValue++;             // ����ֵ֪ͨ
                break;

            case eSetValueWithOverwrite:
                pxTCB->ulNotifiedValue = ulValue;     // ֱ������ֵ֪ͨ�����ǣ�
                break;

            case eSetValueWithoutOverwrite:
                if (ucOriginalNotifyState != taskNOTIFICATION_RECEIVED) // ����Ƿ���Ը���
                {
                    pxTCB->ulNotifiedValue = ulValue; // ����û�н��յ�֪ͨ�����������ֵ
                }
                else
                {
                    xReturn = pdFAIL;                // ����ѽ��յ�֪ͨ������ʧ��
                }
                break;

            case eNoAction:
                break;                               // ��ִ���κβ���
        }

        traceTASK_NOTIFY();                          // ��¼����֪ͨ�¼������ڵ��Ժͼ�أ�

        if (ucOriginalNotifyState == taskWAITING_NOTIFICATION) // ��������Ƿ��ڵȴ�֪ͨ
        {
            (void) uxListRemove(&(pxTCB->xStateListItem)); // �ӵȴ��б����Ƴ�����
            prvAddTaskToReadyList(pxTCB);                // ��������ӵ������б�

            configASSERT(listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) == NULL); // �����¼��б�Ϊ��

            #if (configUSE_TICKLESS_IDLE != 0)
            {
                prvResetNextTaskUnblockTime();         // ������һ������ʱ�䣨����������޵δ����ģʽ��
            }
            #endif

            // �����֪ͨ�����ȼ����ڵ�ǰ��������ȼ�������������л�
            if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
            {
                taskYIELD_IF_USING_PREEMPTION();       // ���ʹ����ռ��������������л�
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();              // ִ�и����ʲ��Ա�ǣ���ʾ�˷�֧��ִ�У�
            }
        }
        else
        {
            mtCOVERAGE_TEST_MARKER();                 // ִ�и����ʲ��Ա�ǣ���ʾδ�ڵȴ�֪ͨ��״̬��
        }
    }
    taskEXIT_CRITICAL();                              // �˳��ٽ������ָ��������л�

    return xReturn;                                   // ����֪ͨ�Ľ�����ɹ���ʧ�ܣ�
}


#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief ���жϷ��������֪ͨһ������
 *
 * �˺������ڴ��жϷ������ISR����֪ͨһ�����񡣴˺����������ж���������
 * ���������ֵ֪ͨ����������Ҫ���ѵȴ�֪ͨ�����񡣴��⣬��������ȷ���Ƿ���
 * �������ȼ���������Ҫ���У���������Ӧ�ı�־��
 *
 * @param xTaskToNotify Ҫ֪ͨ����������
 * @param ulValue ���ڸ���ֵ֪ͨ����ֵ��
 * @param eAction ָ����θ���ֵ֪ͨ�Ĳ������͡�
 * @param pulPreviousNotificationValue �洢����֮ǰ�ľ�ֵ֪ͨ��ָ�롣
 * @param pxHigherPriorityTaskWoken �洢�Ƿ����˸������ȼ�����Ĳ�����־ָ�롣
 * @return ���ز����Ľ����pdPASS ��ʾ�ɹ���pdFAIL ��ʾʧ�ܡ�
 */
BaseType_t xTaskGenericNotifyFromISR( TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction, uint32_t *pulPreviousNotificationValue, BaseType_t *pxHigherPriorityTaskWoken )
{
    TCB_t * pxTCB;
    uint8_t ucOriginalNotifyState;
    BaseType_t xReturn = pdPASS;
    UBaseType_t uxSavedInterruptStatus;

    // ����ȷ�����ݵ���������Ч
    configASSERT( xTaskToNotify );

    // ȷ�����ж��е��ô˺����ǺϷ���
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ��ȡ��ǰ����� TCB ָ��
    pxTCB = ( TCB_t * ) xTaskToNotify;

    // �����жϲ������ж�״̬
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        // ����ṩ�˴洢��ֵ֪ͨ��ָ�룬�򱣴浱ǰ��ֵ֪ͨ
        if( pulPreviousNotificationValue != NULL )
        {
            *pulPreviousNotificationValue = pxTCB->ulNotifiedValue;
        }

        // ����ԭʼ֪ͨ״̬
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        // �����ṩ�Ĳ������͸���ֵ֪ͨ
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
                    // �޷�д����ֵ
                    xReturn = pdFAIL;
                }
                break;

            case eNoAction:
                // ����֪ͨ��������ֵ֪ͨ
                break;
        }

        // ��¼֪ͨ����
        traceTASK_NOTIFY_FROM_ISR();

        // ����������ȴ�����֪ͨ�������Ƴ�����״̬
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            // ȷ���������κ��¼��б���
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                // ���ӳ��б����Ƴ�����
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                // �������������б�
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // �������������ͣ�����������ֱ���������ָ�
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            // �����֪ͨ���������ȼ����ڵ�ǰִ�е���������Ҫ�л�������
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                // ��Ǹ������ȼ��������ѱ�����
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    // ����û�û��ʹ�� "xHigherPriorityTaskWoken" �����������������л�����
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }

    // �ָ��ж�״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief ���жϷ��������֪ͨһ������
 *
 * �˺������ڴ��жϷ������ISR����֪ͨһ�����񡣴˺������ж���������
 * ���������ֵ֪ͨ����������Ҫ���ѵȴ�֪ͨ�����������֪ͨ���������ȼ�
 * ���ڵ�ǰ����ִ�е�������������Ӧ�ı�־��ָʾ������Ҫ�����������л���
 *
 * @param xTaskToNotify Ҫ֪ͨ����������
 * @param pxHigherPriorityTaskWoken �洢�Ƿ����˸������ȼ�����Ĳ�����־ָ�롣
 */
void vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify, BaseType_t *pxHigherPriorityTaskWoken )
{
    TCB_t * pxTCB;
    uint8_t ucOriginalNotifyState;
    UBaseType_t uxSavedInterruptStatus;

    // ����ȷ�����ݵ���������Ч
    configASSERT( xTaskToNotify );

    // ȷ�����ж��е��ô˺����ǺϷ���
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ��ȡ��ǰ����� TCB ָ��
    pxTCB = ( TCB_t * ) xTaskToNotify;

    // �����жϲ������ж�״̬
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        // ����ԭʼ֪ͨ״̬
        ucOriginalNotifyState = pxTCB->ucNotifyState;
        pxTCB->ucNotifyState = taskNOTIFICATION_RECEIVED;

        // 'Giving' �൱���ڼ������ź����е�������
        ( pxTCB->ulNotifiedValue )++;

        // ��¼֪ͨ����
        traceTASK_NOTIFY_GIVE_FROM_ISR();

        // ����������ȴ�����֪ͨ�������Ƴ�����״̬
        if( ucOriginalNotifyState == taskWAITING_NOTIFICATION )
        {
            // ȷ���������κ��¼��б���
            configASSERT( listLIST_ITEM_CONTAINER( &( pxTCB->xEventListItem ) ) == NULL );

            if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE )
            {
                // ���ӳ��б����Ƴ�����
                ( void ) uxListRemove( &( pxTCB->xStateListItem ) );
                // �������������б�
                prvAddTaskToReadyList( pxTCB );
            }
            else
            {
                // �������������ͣ�����������ֱ���������ָ�
                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
            }

            // �����֪ͨ���������ȼ����ڵ�ǰִ�е���������Ҫ�л�������
            if( pxTCB->uxPriority > pxCurrentTCB->uxPriority )
            {
                // ��Ǹ������ȼ��������ѱ�����
                if( pxHigherPriorityTaskWoken != NULL )
                {
                    *pxHigherPriorityTaskWoken = pdTRUE;
                }
                else
                {
                    // ����û�û��ʹ�� "xHigherPriorityTaskWoken" �����������������л�����
                    xYieldPending = pdTRUE;
                }
            }
            else
            {
                // ���븲���ʲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
    }

    // �ָ��ж�״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
}

#endif /* configUSE_TASK_NOTIFICATIONS */

/*-----------------------------------------------------------*/

#if( configUSE_TASK_NOTIFICATIONS == 1 )

/**
 * @brief ��������֪ͨ״̬��
 *
 * �˺����������һ�������֪ͨ״̬���������ǰ�����ѽ��յ�֪ͨ��״̬��
 * ����״̬����Ϊδ�ȴ�֪ͨ��״̬���˺�������һ������ֵ����ʾ�Ƿ�ɹ�
 * �����֪ͨ״̬��
 *
 * @param xTask Ҫ���֪ͨ״̬�������������Ϊ NULL���������ǰ�����֪ͨ״̬��
 * @return ���ز����Ľ����pdPASS ��ʾ�ɹ���pdFAIL ��ʾʧ�ܡ�
 */
BaseType_t xTaskNotifyStateClear( TaskHandle_t xTask )
{
    TCB_t *pxTCB;
    BaseType_t xReturn;

    /* �������ľ��Ϊ NULL���������ǰ�����֪ͨ״̬�� */
    pxTCB = prvGetTCBFromHandle( xTask );

    // �����ٽ���
    taskENTER_CRITICAL();
    {
        if( pxTCB->ucNotifyState == taskNOTIFICATION_RECEIVED )
        {
            // �������״̬����Ϊδ�ȴ�֪ͨ
            pxTCB->ucNotifyState = taskNOT_WAITING_NOTIFICATION;
            xReturn = pdPASS;
        }
        else
        {
            // ����������ڵȴ�֪ͨ��״̬�����������κ�����
            xReturn = pdFAIL;
        }
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    return xReturn;
}

#endif /* configUSE_TASK_NOTIFICATIONS */
/*-----------------------------------------------------------*/


/**
 * @brief ����ǰ������ӵ��ӳ��б��С�
 *
 * �˺������ڽ���ǰ������ӵ��ӳ��б��С��˺�������������ȴ��ض��¼�����ʱ��
 * ����Ӿ����б����Ƴ���������һ������ʱ��������ӳ��б��С��������������
 * �صȴ�����������ڹ��������б��С�
 *
 * @param xTicksToWait ����ȴ��ĵδ�����
 * @param xCanBlockIndefinitely �����Ƿ���������ڵصȴ���
 */
static void prvAddCurrentTaskToDelayedList( TickType_t xTicksToWait, const BaseType_t xCanBlockIndefinitely )
{
    TickType_t xTimeToWake;
    const TickType_t xConstTickCount = xTickCount;

    #if( INCLUDE_xTaskAbortDelay == 1 )
    {
        /* �ڽ����ӳ��б�֮ǰ��ȷ�� ucDelayAborted ��Ǳ�����Ϊ pdFALSE��
           �Ա��������뿪 Blocked ״̬ʱ�ܹ���⵽�ñ�Ǳ�����Ϊ pdTRUE�� */
        pxCurrentTCB->ucDelayAborted = pdFALSE;
    }
    #endif

    /* ������Ӿ����б����Ƴ�����Ϊͬһ���б���ͬʱ���ھ����б���ӳ��б� */
    if( uxListRemove( &( pxCurrentTCB->xStateListItem ) ) == ( UBaseType_t ) 0 )
    {
        /* ��ǰ���������ĳ�������б��У���˿���ֱ�ӵ��ö˿����úꡣ */
        portRESET_READY_PRIORITY( pxCurrentTCB->uxPriority, uxTopReadyPriority );
    }
    else
    {
        /* ���븲���ʲ��Ա�� */
        mtCOVERAGE_TEST_MARKER();
    }

    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        if( ( xTicksToWait == portMAX_DELAY ) && ( xCanBlockIndefinitely != pdFALSE ) )
        {
            /* ������������ڵصȴ���������ӵ����������б��У��������ӳ��б�
               ��ȷ����������ʱ���¼��������ѡ� */
            vListInsertEnd( &xSuspendedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* ��������Ӧ�ñ����ѵ�ʱ�䣨����¼�δ������������ܻ���������ں˻���ȷ���� */
            xTimeToWake = xConstTickCount + xTicksToWait;

            /* ���б�����밴����ʱ�������λ�á� */
            listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

            if( xTimeToWake < xConstTickCount )
            {
                /* ����ʱ�䷢���������������Ŀ��������б� */
                vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
            }
            else
            {
                /* ����ʱ��û�������ʹ�õ�ǰ���ӳ��б� */
                vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

                /* ������񱻷������ӳ������б��ͷ��������Ҫ���� xNextTaskUnblockTime�� */
                if( xTimeToWake < xNextTaskUnblockTime )
                {
                    xNextTaskUnblockTime = xTimeToWake;
                }
                else
                {
                    /* ���븲���ʲ��Ա�� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        }
    }
    #else /* INCLUDE_vTaskSuspend */
    {
        /* ��������Ӧ�ñ����ѵ�ʱ�䣨����¼�δ������������ܻ���������ں˻���ȷ���� */
        xTimeToWake = xConstTickCount + xTicksToWait;

        /* ���б�����밴����ʱ�������λ�á� */
        listSET_LIST_ITEM_VALUE( &( pxCurrentTCB->xStateListItem ), xTimeToWake );

        if( xTimeToWake < xConstTickCount )
        {
            /* ����ʱ�䷢���������������Ŀ��������б� */
            vListInsert( pxOverflowDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );
        }
        else
        {
            /* ����ʱ��û�������ʹ�õ�ǰ���ӳ��б� */
            vListInsert( pxDelayedTaskList, &( pxCurrentTCB->xStateListItem ) );

            /* ������񱻷������ӳ������б��ͷ��������Ҫ���� xNextTaskUnblockTime�� */
            if( xTimeToWake < xNextTaskUnblockTime )
            {
                xNextTaskUnblockTime = xTimeToWake;
            }
            else
            {
                /* ���븲���ʲ��Ա�� */
                mtCOVERAGE_TEST_MARKER();
            }
        }

        /* ������ INCLUDE_vTaskSuspend ��Ϊ 1 ʱ�������������档 */
        ( void ) xCanBlockIndefinitely;
    }
    #endif /* INCLUDE_vTaskSuspend */
}


#ifdef FREERTOS_MODULE_TEST
	#include "tasks_test_access_functions.h"
#endif

