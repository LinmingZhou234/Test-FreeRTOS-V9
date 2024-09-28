/* ��׼�����ļ��� */
#include <stdlib.h>

/* ���� MPU_WRAPPERS_INCLUDED_FROM_API_FILE �Է�ֹ task.h ���¶���
���� API ������ʹ�� MPU ��װ������Ӧ��ֻ�� task.h ��Ӧ�ó����ļ�����ʱ��ɡ� */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* ���� FreeRTOS ͷ�ļ��� */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* ��� INCLUDE_xTimerPendFunctionCall ����Ϊ 1 ���� configUSE_TIMERS Ϊ 0�����׳��������
ȷ�� configUSE_TIMERS ��Ϊ 1 ��ʹ xTimerPendFunctionCall() �������á� */
#if ( INCLUDE_xTimerPendFunctionCall == 1 ) && ( configUSE_TIMERS == 0 )
    #error configUSE_TIMERS ������Ϊ 1 ����ʹ xTimerPendFunctionCall() �������á�
#endif

/* ��� MPU_WRAPPERS_INCLUDED_FROM_API_FILE �Ķ��塣lint ���� e961 �� e750 �����ԣ���Ϊ MPU �˿�Ҫ��
������ͷ�ļ��ж��� MPU_WRAPPERS_INCLUDED_FROM_API_FILE�����ڱ��ļ��в���Ҫ����������ȷ����Ȩ�����Ȩ���Ӻͷ��á� */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750 */

/* ���Ӧ�ó������ò����������ʱ�����ܣ�������Դ�ļ������������� #if ���ļ���ײ��رա�
���������������ʱ�����ܣ���ȷ���� FreeRTOSConfig.h �н� configUSE_TIMERS ��Ϊ 1�� */
#if ( configUSE_TIMERS == 1 )

/* ���ֶ��塣 */
#define tmrNO_DELAY		( TickType_t ) 0U  /* ���ӳٵĶ��壬��ʾ����ִ�С� */

/* ��ʱ������Ķ��塣 */
typedef struct tmrTimerControl {
    const char *pcTimerName;        /* �ı����ơ��ⲻ���ں�ʹ�õģ�������Ϊ�˷�����ԡ� */
                                      /* lint !e971 �����ַ����͵��ַ�ʹ��δ�޶����ַ����͡� */
    ListItem_t xTimerListItem;      /* �����¼�����ı�׼����� */
    TickType_t xTimerPeriodInTicks; /* ��ʱ�������ڣ��Եδ�Ϊ��λ�� */
    UBaseType_t uxAutoReload;       /* �������Ϊ pdTRUE����ʱ�����ں��Զ��������������Ϊ pdFALSE����ʱ��Ϊһ���Զ�ʱ���� */
    void *pvTimerID;                /* ʶ��ʱ����һ�� ID����������ͬһ���ص����ڶ����ʱ��ʱʶ��ʱ���� */
    TimerCallbackFunction_t pxCallbackFunction; /* ��ʱ������ʱ�������õĺ����� */
    
    #if( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxTimerNumber;  /* �ɸ��ٹ��ߣ��� FreeRTOS+Trace������� ID�� */
    #endif
    
    #if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated; /* �����ʱ����̬������������Ϊ pdTRUE���Ա���ɾ����ʱ��ʱ�����ͷ��ڴ档 */
    #endif
} xTIMER;

/* ���ɵ� xTIMER ���Ʊ��������棬��ʹ�� typedef �����µ� Timer_t ���ƣ������öԽϾɵ��ں˸�֪��������֧�֡� */
typedef xTIMER Timer_t;

/* ��������ڶ�ʱ�������з��ͺͽ��յ���Ϣ���͡�
�����Ŷ��������͵���Ϣ�������ڲ��������ʱ������Ϣ������ִ�зǶ�ʱ����ػص�����Ϣ��
��������Ϣ���ͷֱ�����������ͬ�Ľṹ���У�xTimerParametersType �� xCallbackParametersType��*/
typedef struct tmrTimerParameters {
    TickType_t xMessageValue;          /* ����ĳЩ����Ŀ�ѡֵ�����磬�����Ķ�ʱ��������ʱ�� */
    Timer_t *pxTimer;                  /* Ӧ������Ķ�ʱ���� */
} TimerParameter_t;

typedef struct tmrCallbackParameters {
    PendedFunction_t pxCallbackFunction; /* Ҫִ�еĻص������� */
    void *pvParameter1;                  /* ��Ϊ�ص�������һ��������ֵ�� */
    uint32_t ulParameter2;               /* ��Ϊ�ص������ڶ���������ֵ�� */
} CallbackParameters_t;

/* ����������Ϣ���͵Ľṹ�壬�Լ�һ����ʶ��������ȷ��������Ϣ������Ч��*/
typedef struct tmrTimerQueueMessage {
    BaseType_t xMessageID;              /* ���͵���ʱ�������������� */
    union {
        TimerParameter_t xTimerParameters;

        /* ���������ʹ�� xCallbackParameters����Ҫ��������
           ��Ϊ����ʹ�ṹ�壨�Լ���˶�ʱ�����У���� */
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
            CallbackParameters_t xCallbackParameters;
        #endif /* INCLUDE_xTimerPendFunctionCall */
    } u;
} DaemonTaskMessage_t;

/* lint -e956 �Ѿ�ͨ���ֶ������ͼ��ȷ����Щ��̬������������Ϊ volatile��*/

/* �洢���ʱ�����б���ʱ�����չ���ʱ����������Ĺ���ʱ��λ���б�ǰ�档
ֻ�ж�ʱ�������������������Щ�б� */
PRIVILEGED_DATA static List_t xActiveTimerList1;
PRIVILEGED_DATA static List_t xActiveTimerList2;
PRIVILEGED_DATA static List_t *pxCurrentTimerList;
PRIVILEGED_DATA static List_t *pxOverflowTimerList;

/* һ��������ʱ����������������Ķ��С� */
PRIVILEGED_DATA static QueueHandle_t xTimerQueue = NULL;
PRIVILEGED_DATA static TaskHandle_t xTimerTaskHandle = NULL;

/*lint +e956 */

/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )

/* ���֧�־�̬���䣬��Ӧ�ó�������ṩ���»ص�����������ʹ��Ӧ�ó������ѡ���Ե��ṩ��ʱ������ʹ�õ��ڴ棬
��Ϊ����Ķ�ջ��TCB��������ƿ飩��*/
extern void vApplicationGetTimerTaskMemory(
    StaticTask_t **ppxTimerTaskTCBBuffer,   /* ���������ָ�����ڴ�Ŷ�ʱ������TCB�ľ�̬�ڴ��ָ�� */
    StackType_t **ppxTimerTaskStackBuffer,  /* ���������ָ�����ڴ�Ŷ�ʱ�������ջ�ľ�̬�ڴ��ָ�� */
    uint32_t *pulTimerTaskStackSize         /* �����������ʱ�������ջ�Ĵ�С */
);

#endif

/*
 * ��ʼ����ʱ����������ʹ�õĻ�����ʩ�������δ��ʼ���Ļ���
 */
static void prvCheckForValidListAndQueue( void ) PRIVILEGED_FUNCTION;

/*
 * ��ʱ�����������ػ����̣�����ʱ�����������������ơ���������ͨ�� xTimerQueue �����붨ʱ����������ͨ�š�
 */
static void prvTimerTask( void *pvParameters ) PRIVILEGED_FUNCTION;

/*
 * �ɶ�ʱ������������ã��Խ��ͺʹ������ڶ�ʱ�������Ͻ��յ������
 */
static void prvProcessReceivedCommands( void ) PRIVILEGED_FUNCTION;

/*
 * ����ʱ������ xActiveTimerList1 �� xActiveTimerList2������ȡ���ڹ���ʱ���Ƿ��¶�ʱ�������������
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime ) PRIVILEGED_FUNCTION;

/*
 * һ�����ʱ���Ѿ����������ʱ�䡣�������һ���Զ����ض�ʱ���������ض�ʱ����Ȼ�������ص�������
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow ) PRIVILEGED_FUNCTION;

/*
 * �δ�������������ȷ����ǰ��ʱ���б��������κζ�ʱ��֮���л���ʱ���б�
 */
static void prvSwitchTimerLists( void ) PRIVILEGED_FUNCTION;

/*
 * ��ȡ��ǰ�ĵδ����ֵ�������ϴε��� prvSampleTimeNow() ���������δ�������������½� *pxTimerListsWereSwitched ����Ϊ pdTRUE��
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched ) PRIVILEGED_FUNCTION;

/*
 * �����ʱ���б�����κλ��ʱ�����򷵻ؽ�Ҫ���ȹ��ڵĶ�ʱ���Ĺ���ʱ�䣬���� *pxListWasEmpty ����Ϊ false��
 * �����ʱ���б������κζ�ʱ�����򷵻� 0 ���� *pxListWasEmpty ����Ϊ pdTRUE��
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * �����ʱ���ѹ��ڣ�������������������ʱ����������ֱ����ʱ��ȷʵ���ڻ���յ����
 */
static void prvProcessTimerOrBlockTask( const TickType_t xNextExpireTime, BaseType_t xListWasEmpty ) PRIVILEGED_FUNCTION;

/*
 * �ھ�̬��̬���� Timer_t �ṹ֮����ã������ṹ�ĳ�Ա��
 */
static void prvInitialiseNewTimer(	const char * const pcTimerName,     /* ��ʱ�����ı����� */
									const TickType_t xTimerPeriodInTicks, /* ��ʱ�������ڣ��δ�Ϊ��λ�� */
									const UBaseType_t uxAutoReload,     /* �Զ����ر�־ */
									void * const pvTimerID,             /* ��ʱ���� ID */
									TimerCallbackFunction_t pxCallbackFunction, /* ��ʱ������ʱ���õĻص����� */
									Timer_t *pxNewTimer ) PRIVILEGED_FUNCTION; /*lint !e971 �������ַ����͵����ַ�ʹ��δ�޶����ַ����͡� */
/*-----------------------------------------------------------*/
									
/*
 * �ú������ڴ�����ʱ����������prvTimerTask�������ڴ����ɹ�ʱ���� pdPASS
 *
 */
BaseType_t xTimerCreateTimerTask( void )
{
    BaseType_t xReturn = pdFAIL;

    /* �������� configUSE_TIMERS Ϊ 1 ʱ���˺������ڵ���������ʱ�����á�
       ��鶨ʱ����������ʹ�õĻ�����ʩ�Ƿ��Ѿ�������/��ʼ����
       ����Ѿ��ж�ʱ�������������ʼ��Ӧ���Ѿ���ɡ�*/
    prvCheckForValidListAndQueue();

    if( xTimerQueue != NULL )
    {
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            StaticTask_t *pxTimerTaskTCBBuffer = NULL;
            StackType_t *pxTimerTaskStackBuffer = NULL;
            uint32_t ulTimerTaskStackSize;

            /* ��ȡ���ڶ�ʱ������ľ�̬�ڴ棨��ջ��TCB����*/
            vApplicationGetTimerTaskMemory( &pxTimerTaskTCBBuffer, &pxTimerTaskStackBuffer, &ulTimerTaskStackSize );
            
            /* ������ʱ���������񣬲�ʹ�þ�̬������ڴ档*/
            xTimerTaskHandle = xTaskCreateStatic(
                                    prvTimerTask,    /* ������ */
                                    "Tmr Svc",       /* �������� */
                                    ulTimerTaskStackSize, /* ��ջ��С */
                                    NULL,            /* ������� */
                                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, /* ���ȼ� */
                                    pxTimerTaskStackBuffer, /* ��ջ������ */
                                    pxTimerTaskTCBBuffer /* TCB ������ */
                                );

            if( xTimerTaskHandle != NULL )
            {
                xReturn = pdPASS;
            }
        }
        #else
        {
            /* ������ʱ����������ʹ��Ĭ�ϵĶ�ջ��ȺͶ�̬�ڴ���䡣*/
            xReturn = xTaskCreate(
                                    prvTimerTask,    /* ������ */
                                    "Tmr Svc",       /* �������� */
                                    configTIMER_TASK_STACK_DEPTH, /* ��ջ��� */
                                    NULL,            /* ������� */
                                    ( ( UBaseType_t ) configTIMER_TASK_PRIORITY ) | portPRIVILEGE_BIT, /* ���ȼ� */
                                    &xTimerTaskHandle /* ������ */
                                );
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }
    else
    {
        /* ���ǲ��Ա�ǡ�*/
        mtCOVERAGE_TEST_MARKER();
    }

    /* ����ȷ����ʱ���������񴴽��ɹ���*/
    configASSERT( xReturn );
    return xReturn;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

/*
 * @brief xTimerCreate ����һ����ʱ����
 *
 * �˺������ڴ���һ���µĶ�ʱ������������г�ʼ����
 *
 * @param pcTimerName ��ʱ�������֣��ַ�����������Ϊ NULL��
 * @param xTimerPeriodInTicks ��ʱ�������ڣ���λΪ�δ�ticks����
 * @param uxAutoReload �Ƿ��Զ����ء������ʾ�Զ����أ����ʾ���δ�����
 * @param pvTimerID ��ʱ���� ID ���û����ݡ�
 * @param pxCallbackFunction �ص�����������ʱ����ʱʱ�����á�
 *
 * @return ���ش����Ķ�ʱ��������������ʧ�ܣ��򷵻� NULL��
 */
TimerHandle_t xTimerCreate(
    const char * const pcTimerName,
    const TickType_t xTimerPeriodInTicks,
    const UBaseType_t uxAutoReload,
    void * const pvTimerID,
    TimerCallbackFunction_t pxCallbackFunction )
{
    Timer_t *pxNewTimer; // ����ָ��ʱ���ṹ��ָ��

    // �����ڴ��������µĶ�ʱ���ṹ
    pxNewTimer = ( Timer_t * ) pvPortMalloc( sizeof( Timer_t ) );

    if( pxNewTimer != NULL )
    {
        // ��ʼ���µĶ�ʱ���ṹ
        prvInitialiseNewTimer( pcTimerName, xTimerPeriodInTicks, uxAutoReload, pvTimerID, pxCallbackFunction, pxNewTimer );

        // ��������˾�̬����֧�֣������ö�ʱ���ı�־
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            // ��Ƕ�ʱ��Ϊ��̬����
            pxNewTimer->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */
    }

    // ���ش����Ķ�ʱ�����
    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
/*
 * �ú������ھ�̬����һ����ʱ���������ض�ʱ���ľ��
 *
 */
TimerHandle_t xTimerCreateStatic(
    const char * const pcTimerName,        /* ��ʱ�������� */
    const TickType_t xTimerPeriodInTicks,  /* ��ʱ�������ڣ��δ�Ϊ��λ�� */
    const UBaseType_t uxAutoReload,        /* �Ƿ��Զ����� */
    void * const pvTimerID,                /* ��ʱ����Ψһ��ʶ�� */
    TimerCallbackFunction_t pxCallbackFunction, /* ��ʱ������ʱ���õĻص����� */
    StaticTimer_t *pxTimerBuffer           /* ��̬��ʱ�������� */
) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    Timer_t *pxNewTimer;

    #if( configASSERT_DEFINED == 1 )
    {
        /* ��� StaticTimer_t �ṹ��Ĵ�С�Ƿ����ʵ�ʶ�ʱ���ṹ��Ĵ�С����ȷ�������Ǽ��ݵġ� */
        volatile size_t xSize = sizeof( StaticTimer_t );
        configASSERT( xSize == sizeof( Timer_t ) );
    }
    #endif /* configASSERT_DEFINED */

    /* �����ṩһ��ָ�� StaticTimer_t �ṹ���ָ�룬ʹ������ */
    configASSERT( pxTimerBuffer );
    pxNewTimer = ( Timer_t * ) pxTimerBuffer; /*lint !e740 ���������ת���Ǻ���ģ���Ϊ�������ṹ�����Ϊ������ͬ�Ķ��뷽ʽ�����Ҵ�Сͨ�����Խ����˼�顣 */

    if( pxNewTimer != NULL )
    {
        /* ��ʼ���µĶ�ʱ���ṹ�� */
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
            /* ��ʱ�����Ծ�̬��̬��������¼�����ʱ���Ǿ�̬�����ģ��Է��Ժ�ɾ��ʱ��Ҫ�õ������Ϣ�� */
            pxNewTimer->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
    }

    /* ���ض�ʱ������� */
    return pxNewTimer;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/*
 * ��ʼ���µĶ�ʱ���ṹ���������������Ա������
 *
 * ����:
 * @param pcTimerName: ��ʱ�������ƣ����ڵ��ԡ�
 * @param xTimerPeriodInTicks: ��ʱ�������ڣ��Եδ�Ϊ��λ��
 * @param uxAutoReload: ������־��ָʾ��ʱ���Ƿ��Զ����ء�
 * @param pvTimerID: ��ʱ����Ψһ��ʶ����
 * @param pxCallbackFunction: ��ʱ������ʱ���õĻص�������
 * @param pxNewTimer: ָ���¶�ʱ���ṹ��ָ�롣
 */
static void prvInitialiseNewTimer(
    const char * const pcTimerName,        /* ��ʱ�������� */
    const TickType_t xTimerPeriodInTicks,  /* ��ʱ�������ڣ��δ�Ϊ��λ�� */
    const UBaseType_t uxAutoReload,        /* �Ƿ��Զ����� */
    void * const pvTimerID,                /* ��ʱ����Ψһ��ʶ�� */
    TimerCallbackFunction_t pxCallbackFunction, /* ��ʱ������ʱ���õĻص����� */
    Timer_t *pxNewTimer                    /* �¶�ʱ���ṹ��ָ�� */
) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    /* 0 ����һ����Ч������ֵ�� */
    configASSERT( ( xTimerPeriodInTicks > 0 ) );

    if( pxNewTimer != NULL )
    {
        /* ȷ����ʱ����������ʹ�õĻ�����ʩ�ѱ�����/��ʼ���� */
        prvCheckForValidListAndQueue();

        /* ʹ�ú���������ʼ����ʱ���ṹ�ĳ�Ա�� */
        pxNewTimer->pcTimerName = pcTimerName;       /* ��ʱ�������� */
        pxNewTimer->xTimerPeriodInTicks = xTimerPeriodInTicks; /* ��ʱ�������� */
        pxNewTimer->uxAutoReload = uxAutoReload;     /* �Ƿ��Զ����� */
        pxNewTimer->pvTimerID = pvTimerID;           /* ��ʱ����Ψһ��ʶ�� */
        pxNewTimer->pxCallbackFunction = pxCallbackFunction; /* ��ʱ������ʱ���õĻص����� */
        
        /* ��ʼ����ʱ���б�� */
        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
        
        /* ��¼��ʱ�������ĸ�����Ϣ�� */
        traceTIMER_CREATE( pxNewTimer );
    }
}
/*-----------------------------------------------------------*/

/*
 * @brief xTimerGenericCommand �����������ʱ������������ִ���ض�������
 *
 * �˺���������ʱ�����������������ִ���ض��Ĳ�������������ֹͣ�����õȡ�
 *
 * @param xTimer ��ʱ�������
 * @param xCommandID �����ʶ����
 * @param xOptionalValue ��ѡ�����������
 * @param pxHigherPriorityTaskWoken ����˺������¸������ȼ������񱻻��ѣ�������Ϊ pdTRUE��
 * @param xTicksToWait �ȴ����п��е�ʱ�䣨�Եδ�Ϊ��λ����
 *
 * @return ���ز����Ľ��������ɹ���������򷵻� pdPASS�����򷵻� pdFAIL��
 */
BaseType_t xTimerGenericCommand(
    TimerHandle_t xTimer,
    const BaseType_t xCommandID,
    const TickType_t xOptionalValue,
    BaseType_t * const pxHigherPriorityTaskWoken,
    const TickType_t xTicksToWait )
{
    BaseType_t xReturn = pdFAIL; // ��ʼ������ֵΪʧ��
    DaemonTaskMessage_t xMessage; // ������Ϣ�ṹ��

    // ����ȷ����ʱ�������Ч
    configASSERT( xTimer );

    /* �����ʱ�������ѳ�ʼ����������Ϣ����ʱ����������ִ���ض����� */
    if( xTimerQueue != NULL )
    {
        /* ������Ϣ */
        xMessage.xMessageID = xCommandID;
        xMessage.u.xTimerParameters.xMessageValue = xOptionalValue;
        xMessage.u.xTimerParameters.pxTimer = ( Timer_t * ) xTimer;

        if( xCommandID < tmrFIRST_FROM_ISR_COMMAND )
        {
            /* ����������������У�������ȴ����� */
            if( xTaskGetSchedulerState() == taskSCHEDULER_RUNNING )
            {
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );
            }
            else
            {
                /* ���������δ���У�������ȴ����� */
                xReturn = xQueueSendToBack( xTimerQueue, &xMessage, tmrNO_DELAY );
            }
        }
        else
        {
            /* ����Ǵ��жϷ��������ã�������������Ϣ��������Ƿ��и������ȼ����񱻻��� */
            xReturn = xQueueSendToBackFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );
        }

        /* ��¼�������� */
        traceTIMER_COMMAND_SEND( xTimer, xCommandID, xOptionalValue, xReturn );
    }
    else
    {
        /* ���ǲ��Ա�� */
        mtCOVERAGE_TEST_MARKER();
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * ��ȡ��ʱ���ػ�����ľ����
 *
 * ����ֵ:
 * ���ض�ʱ���ػ�����ľ����
 */
TaskHandle_t xTimerGetTimerDaemonTaskHandle( void )
{
    /* ����ڵ���������֮ǰ������ xTimerGetTimerDaemonTaskHandle()��
       �� xTimerTaskHandle ��Ϊ NULL�� */
    configASSERT( ( xTimerTaskHandle != NULL ) );
    
    return xTimerTaskHandle;
}
/*-----------------------------------------------------------*/

/*
 * ��ȡָ����ʱ�������ڡ�
 *
 * ����:
 * @param xTimer: ��ʱ���ľ����
 *
 * ����ֵ:
 * ���ض�ʱ�������ڣ��δ�Ϊ��λ����
 */
TickType_t xTimerGetPeriod( TimerHandle_t xTimer )
{
    Timer_t *pxTimer = ( Timer_t * ) xTimer;

    /* ȷ����ʱ�������Ч�� */
    configASSERT( xTimer );
    
    return pxTimer->xTimerPeriodInTicks;
}
/*-----------------------------------------------------------*/

/*
 * ��ȡָ����ʱ���Ĺ���ʱ�䡣
 *
 * ����:
 * @param xTimer: ��ʱ���ľ����
 *
 * ����ֵ:
 * ���ض�ʱ���Ĺ���ʱ�䣨�δ�Ϊ��λ����
 */
TickType_t xTimerGetExpiryTime( TimerHandle_t xTimer )
{
    Timer_t *pxTimer = ( Timer_t * ) xTimer;
    TickType_t xReturn;

    /* ȷ����ʱ�������Ч�� */
    configASSERT( xTimer );

    /* ��ȡ��ʱ���б����ֵ��������ʱ�䡣 */
    xReturn = listGET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ) );
    
    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * ��ȡָ����ʱ�������ơ�
 *
 * ����:
 * @param xTimer: ��ʱ���ľ����
 *
 * ����ֵ:
 * ���ض�ʱ�������ơ�
 */
const char * pcTimerGetName( TimerHandle_t xTimer ) /*lint !e971 Unqualified char types are allowed for strings and single characters only. */
{
    Timer_t *pxTimer = ( Timer_t * ) xTimer;

    /* ȷ����ʱ�������Ч�� */
    configASSERT( xTimer );
    
    return pxTimer->pcTimerName;
}
/*-----------------------------------------------------------*/

/*
 * �����ѹ��ڵĶ�ʱ����
 *
 * ����:
 * @param xNextExpireTime: ��һ������ʱ�䣨�δ�Ϊ��λ����
 * @param xTimeNow: ��ǰʱ�䣨�δ�Ϊ��λ����
 */
static void prvProcessExpiredTimer( const TickType_t xNextExpireTime, const TickType_t xTimeNow )
{
    BaseType_t xResult;
    Timer_t * const pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );

    /* �ӻ��ʱ���б����Ƴ���ʱ�����Ѿ�ִ���˼����ȷ���б�ǿա� */
    ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
    traceTIMER_EXPIRED( pxTimer );

    /* �����ʱ�����Զ����ض�ʱ�����������һ������ʱ�䣬�����²��뵽���ʱ���б��С� */
    if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
    {
        /* ��ʱ��ʹ������ڵ�ǰʱ�������ʱ�䱻���뵽�б��С���ˣ����������뵽����ڵ�ǰ��Ϊ��ʱ�����ȷ�б��С� */
        if( prvInsertTimerInActiveList( pxTimer, ( xNextExpireTime + pxTimer->xTimerPeriodInTicks ), xTimeNow, xNextExpireTime ) != pdFALSE )
        {
            /* ��ʱ���ڱ���ӵ����ʱ���б�֮ǰ�Ѿ����ڡ��������¼������� */
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

    /* ���ö�ʱ���ص������� */
    pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );
}
/*-----------------------------------------------------------*/

/*
 * ��ʱ�����������ػ����̣�����ʱ�����������������ơ���������ͨ�� xTimerQueue �����붨ʱ����������ͨ�š�
 *
 * ����:
 * @param pvParameters: ���ݸ�����Ĳ�����δʹ�ã���
 */
static void prvTimerTask( void *pvParameters )
{
    TickType_t xNextExpireTime;
    BaseType_t xListWasEmpty;

    /* ֻ��Ϊ�˱�����������档 */
    ( void ) pvParameters;

    #if( configUSE_DAEMON_TASK_STARTUP_HOOK == 1 )
    {
        extern void vApplicationDaemonTaskStartupHook( void );

        /* ����Ӧ�ó����д���ڴ�����ʼִ��ʱ���ڴ�������������ִ��һЩ���롣�������õģ����Ӧ�ó������һЩ��ʼ�����룬��Щ���뽫�ڵ�����������������ִ�С� */
        vApplicationDaemonTaskStartupHook();
    }
    #endif /* configUSE_DAEMON_TASK_STARTUP_HOOK */

    for( ;; )
    {
        /* ��ѯ��ʱ���б��Բ鿴�����Ƿ�����κζ�ʱ����������ڣ����ȡ��һ����ʱ�������ڵ�ʱ�䡣 */
        xNextExpireTime = prvGetNextExpireTime( &xListWasEmpty );

        /* �����ʱ���ѹ��ڣ�����������������������ֱ����ʱ��ȷʵ���ڻ���յ���� */
        prvProcessTimerOrBlockTask( xNextExpireTime, xListWasEmpty );

        /* ���������С� */
        prvProcessReceivedCommands();
    }
}
/*-----------------------------------------------------------*/

/*
 * �����ʱ���ѹ��ڣ�������������������ʱ����������ֱ����ʱ��ȷʵ���ڻ���յ����
 *
 * ����:
 * @param xNextExpireTime: ��һ������ʱ�䣨�δ�Ϊ��λ����
 * @param xListWasEmpty: ��־��ָʾ��ʱ���б��Ƿ�Ϊ�ա�
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
 * ��ȡ�������ʱ�䡣
 *
 * ����:
 * @param pxListWasEmpty: ���������ָʾ��ʱ���б��Ƿ�Ϊ�ա�
 *
 * ����ֵ:
 * �����������ʱ�䣨�Եδ�Ϊ��λ����
 */
static TickType_t prvGetNextExpireTime( BaseType_t * const pxListWasEmpty )
{
    TickType_t xNextExpireTime;

    /* ��ʱ��������ʱ�������б�ͷ�����õ������ȹ��ڵ�����
       ��ȡ�������ʱ�䡣
       ���û�л�Ķ�ʱ�����򽫹���ʱ������Ϊ0��
       �⽫���������ڼ������ʱ�����������ʱ���л���ʱ���б�������������Ĺ���ʱ�䡣 */
    *pxListWasEmpty = listLIST_IS_EMPTY( pxCurrentTimerList );
    if( *pxListWasEmpty == pdFALSE )
    {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );
    }
    else
    {
        /* ȷ�������ڼ������ʱ��������� */
        xNextExpireTime = ( TickType_t ) 0U;
    }

    return xNextExpireTime;
}
/*-----------------------------------------------------------*/

/*
 * ��ȡ��ǰʱ�䣬������Ƿ����˼��������
 *
 * ����:
 * @param pxTimerListsWereSwitched: ���������ָʾ�Ƿ����˶�ʱ���б���л���
 *
 * ����ֵ:
 * ���ص�ǰʱ�䣨�Եδ�Ϊ��λ����
 */
static TickType_t prvSampleTimeNow( BaseType_t * const pxTimerListsWereSwitched )
{
    TickType_t xTimeNow;
    PRIVILEGED_DATA static TickType_t xLastTime = ( TickType_t ) 0U; /*lint !e956 ��������һ������ɷ��ʡ� */

    xTimeNow = xTaskGetTickCount();

    if( xTimeNow < xLastTime )
    {
        /* �����ǰʱ��С���ϴ�ʱ�䣬������������������� */
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
 * ����ʱ�����뵽���ʱ���б��С�
 *
 * ����:
 * @param pxTimer: ��ʱ���ṹ��ָ�롣
 * @param xNextExpiryTime: ��һ������ʱ�䣨�Եδ�Ϊ��λ����
 * @param xTimeNow: ��ǰʱ�䣨�Եδ�Ϊ��λ����
 * @param xCommandTime: ���������ʱ�䣨�Եδ�Ϊ��λ����
 *
 * ����ֵ:
 * �����ʱ��Ӧ�����������򷵻� pdTRUE�����򷵻� pdFALSE��
 */
static BaseType_t prvInsertTimerInActiveList( Timer_t * const pxTimer, const TickType_t xNextExpiryTime, const TickType_t xTimeNow, const TickType_t xCommandTime )
{
    BaseType_t xProcessTimerNow = pdFALSE;

    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    if( xNextExpiryTime <= xTimeNow )
    {
        /* ��鶨ʱ������ʱ���Ƿ��ڷ�������ʹ�������֮���Ѿ���ȥ�� */
        if( ( ( TickType_t ) ( xTimeNow - xCommandTime ) ) >= pxTimer->xTimerPeriodInTicks ) /*lint !e961 MISRA ���⣬��Ϊ����ת������ĳЩ�˿�ֻ������ġ� */
        {
            /* ʵ���ϣ�������ʹ���֮���ʱ�䳬���˶�ʱ�������ڡ� */
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
            /* �������������������Ѿ������������ʱ����δ���
               ��ʱ��ʵ�����Ѿ��������Ĺ���ʱ�䣬Ӧ���������� */
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
 * ����Ӷ�ʱ�����н��յ����
 *
 * ����:
 * �˺�����Ӷ�ʱ������ xTimerQueue �ж�ȡ���п��õ���Ϣ����������Ϣ����ִ����Ӧ�Ĳ�����
 * ��������ʱ����������������ֹͣ��ɾ��������Լ�ִ�й���ĺ������á�
 */
static void prvProcessReceivedCommands( void )
{
    DaemonTaskMessage_t xMessage;
    Timer_t *pxTimer;
    BaseType_t xTimerListsWereSwitched, xResult;
    TickType_t xTimeNow;

    while( xQueueReceive( xTimerQueue, &xMessage, tmrNO_DELAY ) != pdFAIL ) /*lint !e603 xMessage ���س�ʼ������Ϊ�����ڴ��������Ǵ��룬���ҳ��� xQueueReceive() ���� pdTRUE ���򲻻�ʹ������*/
    {
        #if ( INCLUDE_xTimerPendFunctionCall == 1 )
        {
            /* ���������ǹ���ĺ������ö��Ƕ�ʱ����� */
            if( xMessage.xMessageID < ( BaseType_t ) 0 )
            {
                const CallbackParameters_t * const pxCallback = &( xMessage.u.xCallbackParameters );

                /* ��ʱ��ʹ�� xCallbackParameters ��Ա����ִ�лص������ص����� NULL�� */
                configASSERT( pxCallback );

                /* ִ�лص������� */
                pxCallback->pxCallbackFunction( pxCallback->pvParameter1, pxCallback->ulParameter2 );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* INCLUDE_xTimerPendFunctionCall */

        /* ���������Ƕ�ʱ��������ǹ���ĺ������á� */
        if( xMessage.xMessageID >= ( BaseType_t ) 0 )
        {
            /* ��Ϣʹ�� xTimerParameters ��Ա�����������ʱ���� */
            pxTimer = xMessage.u.xTimerParameters.pxTimer;

            if( listIS_CONTAINED_WITHIN( NULL, &( pxTimer->xTimerListItem ) ) == pdFALSE )
            {
                /* ��ʱ�����б��У������Ƴ��� */
                ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
            }
            else
            {
                mtCOVERAGE_TEST_MARKER();
            }

            traceTIMER_COMMAND_RECEIVED( pxTimer, xMessage.xMessageID, xMessage.u.xTimerParameters.xMessageValue );

            /* ����������£�xTimerListsWereSwitched ����û��ʹ�ã�������������ں��������С�
            prvSampleTimeNow() �����ڴ� xTimerQueue ������Ϣ֮����ã��Ա�û���κθ������ȼ�������
            ��ʱ��ȶ�ʱ���ػ�����������ǰ������½���Ϣ��ӵ���Ϣ���У���Ϊ����ռ�˶�ʱ���ػ���������������� xTimeNow ֵ���� */
            xTimeNow = prvSampleTimeNow( &xTimerListsWereSwitched );

            switch( xMessage.xMessageID )
            {
                case tmrCOMMAND_START :
                case tmrCOMMAND_START_FROM_ISR :
                case tmrCOMMAND_RESET :
                case tmrCOMMAND_RESET_FROM_ISR :
                case tmrCOMMAND_START_DONT_TRACE :
                    /* ������������ʱ���� */
                    if( prvInsertTimerInActiveList( pxTimer,  xMessage.u.xTimerParameters.xMessageValue + pxTimer->xTimerPeriodInTicks, xTimeNow, xMessage.u.xTimerParameters.xMessageValue ) != pdFALSE )
                    {
                        /* ��ʱ���ڱ�������ʱ���б�֮ǰ���Ѿ����ڡ����ڴ������� */
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
                    /* ��ʱ���Ѿ����ӻ�б����Ƴ�������û��ʲôҪ���ġ� */
                    break;

                case tmrCOMMAND_CHANGE_PERIOD :
                case tmrCOMMAND_CHANGE_PERIOD_FROM_ISR :
                    pxTimer->xTimerPeriodInTicks = xMessage.u.xTimerParameters.xMessageValue;
                    configASSERT( ( pxTimer->xTimerPeriodInTicks > 0 ) );

                    /* ������û�������Ĳο������ҿ��ܱȾ����ڳ���̡�
                       ����ʱ����˱�����Ϊ��ǰʱ�䣬�����������ڲ���Ϊ�㣬
                       ��һ������ʱ��ֻ����δ������ζ�ţ���������� xTimerStart() �����
                       ����û����Ҫ�����ʧ������� */
                    ( void ) prvInsertTimerInActiveList( pxTimer, ( xTimeNow + pxTimer->xTimerPeriodInTicks ), xTimeNow, xTimeNow );
                    break;

                case tmrCOMMAND_DELETE :
                    /* ��ʱ���Ѿ����ӻ�б����Ƴ�������ڴ��Ƕ�̬����ģ����ͷ��ڴ档 */
                    #if( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 0 ) )
                    {
                        /* ��ʱ��ֻ���Ƕ�̬����� - �ٴ��ͷ����� */
                        vPortFree( pxTimer );
                    }
                    #elif( ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )
                    {
                        /* ��ʱ�������Ǿ�̬����Ļ�̬����ģ������ڳ����ͷ��ڴ�֮ǰ���һ�¡� */
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
                    /* ������������� */
                    break;
            }
        }
    }
}
/*-----------------------------------------------------------*/

/*
 * �л���ʱ���б�
 *
 * ����:
 * �˺������ڴ���ϵͳ������������������ϵͳ���������ʱ����ʱ���б���Ҫ�����л���
 * ���л�֮ǰ����Ҫ�������е�ǰ�б��еĶ�ʱ������ȷ�����ǲ��������ǵĵ���ʱ�䡣
 * ������Զ����صĶ�ʱ��������Ҫȷ�����Ǳ���ȷ������������
 */
static void prvSwitchTimerLists( void )
{
    TickType_t xNextExpireTime, xReloadTime;
    List_t *pxTemp;
    Timer_t *pxTimer;
    BaseType_t xResult;

    /* ϵͳ�������Ѿ��������ʱ���б�����л���
       �����ǰ��ʱ���б��л��ж�ʱ����������һ���Ѿ����ڣ�Ӧ���б��л�ǰ�������ǡ� */
    while( listLIST_IS_EMPTY( pxCurrentTimerList ) == pdFALSE )
    {
        xNextExpireTime = listGET_ITEM_VALUE_OF_HEAD_ENTRY( pxCurrentTimerList );

        /* ���б����Ƴ���ʱ���� */
        pxTimer = ( Timer_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxCurrentTimerList );
        ( void ) uxListRemove( &( pxTimer->xTimerListItem ) );
        traceTIMER_EXPIRED( pxTimer );

        /* ִ����ص�������Ȼ����������Զ����ض�ʱ������������������ʱ����
           ����������������ʱ������Ϊ�б�û���л��� */
        pxTimer->pxCallbackFunction( ( TimerHandle_t ) pxTimer );

        if( pxTimer->uxAutoReload == ( UBaseType_t ) pdTRUE )
        {
            /* ��������ֵ���������ֵ���¶�ʱ��������ͬ�Ķ�ʱ���б������Ѿ����ڣ�
               Ӧ�ý���ʱ�����²��뵽��ǰ�б��У��Ա������ѭ�����ٴδ�������
               ����Ӧ��������������ʱ������ȷ��ֻ�����б�����Ž�����뵽�б��С� */
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

    /* ������ʱ���б� */
    pxTemp = pxCurrentTimerList;
    pxCurrentTimerList = pxOverflowTimerList;
    pxOverflowTimerList = pxTemp;
}
/*-----------------------------------------------------------*/

/*
 * ��鶨ʱ���б�Ͷ����Ƿ��ѳ�ʼ����
 *
 * ����:
 * �˺�������ȷ���������û��ʱ�����б�������붨ʱ������ͨ�ŵĶ����ѱ���ʼ����
 * �����Щ�ṹ��δ��ʼ������˺��������г�ʼ����
 */
static void prvCheckForValidListAndQueue( void )
{
    /* ����������û��ʱ�����б�������붨ʱ������ͨ�ŵĶ����Ƿ��ѳ�ʼ���� */
    taskENTER_CRITICAL();
    {
        if( xTimerQueue == NULL )
        {
            /* ��ʼ���������ʱ���б� */
            vListInitialise( &xActiveTimerList1 );
            vListInitialise( &xActiveTimerList2 );

            /* ���õ�ǰ��ʱ���б�������ʱ���б� */
            pxCurrentTimerList = &xActiveTimerList1;
            pxOverflowTimerList = &xActiveTimerList2;

            #if( configSUPPORT_STATIC_ALLOCATION == 1 )
            {
                /* ��� configSUPPORT_DYNAMIC_ALLOCATION Ϊ 0����̬���䶨ʱ�����С� */
                static StaticQueue_t xStaticTimerQueue;
                static uint8_t ucStaticTimerQueueStorage[ configTIMER_QUEUE_LENGTH * sizeof( DaemonTaskMessage_t ) ];

                /* ������̬��ʱ�����С� */
                xTimerQueue = xQueueCreateStatic( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ), &( ucStaticTimerQueueStorage[ 0 ] ), &xStaticTimerQueue );
            }
            #else
            {
                /* ��̬������ʱ�����С� */
                xTimerQueue = xQueueCreate( ( UBaseType_t ) configTIMER_QUEUE_LENGTH, sizeof( DaemonTaskMessage_t ) );
            }
            #endif /* configSUPPORT_STATIC_ALLOCATION */

            #if ( configQUEUE_REGISTRY_SIZE > 0 )
            {
                /* �������ע����С����0����ע�ᶨʱ�����С� */
                if( xTimerQueue != NULL )
                {
                    /* ����ʱ������ע�ᵽ����ע����С� */
                    vQueueAddToRegistry( xTimerQueue, "TmrQ" );
                }
                else
                {
                    /* ���ڸ����ʲ��Եı�ǡ� */
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            #endif /* configQUEUE_REGISTRY_SIZE */
        }
        else
        {
            /* ���ڸ����ʲ��Եı�ǡ� */
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/*
 * @brief xTimerIsTimerActive ��鶨ʱ���Ƿ��ڻ״̬��
 *
 * �˺������ڼ��ָ���Ķ�ʱ���Ƿ��ڻ״̬�����Ƿ��ڵ�ǰ�������ʱ���б��У���
 *
 * @param xTimer ��ʱ�������
 *
 * @return ����ֵָʾ��ʱ���Ƿ��ڻ״̬�������ʱ�����ڻ״̬���򷵻� pdTRUE�����򷵻� pdFALSE��
 */
BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer)
{
    BaseType_t xTimerIsInActiveList; // ��ʼ������ֵ
    Timer_t *pxTimer = (Timer_t *)xTimer; // ����ָ��ʱ���ṹ��ָ��

    // ����ȷ����ʱ�������Ч
    configASSERT(xTimer);

    /* ��鶨ʱ���Ƿ��ڻ��ʱ���б��� */
    taskENTER_CRITICAL();
    {
        /* ��鶨ʱ���Ƿ��ڿ��б���ʵ������Ϊ�˼�����Ƿ񱻰����ڵ�ǰ��ʱ���б�������ʱ���б��У�
         * �������߼���Ҫ��ת������ʹ�� '!' ����ȡ�������� */
        xTimerIsInActiveList = (BaseType_t)!listIS_CONTAINED_WITHIN(NULL, &(pxTimer->xTimerListItem));
    }
    taskEXIT_CRITICAL();

    return xTimerIsInActiveList;
} /*lint !e818 Can't be pointer to const due to the typedef. */
/*-----------------------------------------------------------*/
/*
 * ��ȡ��ʱ���ı�ʶ����
 *
 * ����:
 * @param xTimer: ��ʱ�������
 *
 * ����ֵ:
 * ���ض�ʱ���ı�ʶ����
 */
void *pvTimerGetTimerID( const TimerHandle_t xTimer )
{
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;
    void *pvReturn;

    // ����ȷ����ʱ�������Ч
    configASSERT( xTimer );

    taskENTER_CRITICAL();
    {
        // ���ٽ�����ȡ��ʱ���ı�ʶ��
        pvReturn = pxTimer->pvTimerID;
    }
    taskEXIT_CRITICAL();

    return pvReturn;
}
/*-----------------------------------------------------------*/

/*
 * ���ö�ʱ���ı�ʶ����
 *
 * ����:
 * @param xTimer: ��ʱ�������
 * @param pvNewID: �µĶ�ʱ����ʶ����
 */
void vTimerSetTimerID( TimerHandle_t xTimer, void *pvNewID )
{
    Timer_t * const pxTimer = ( Timer_t * ) xTimer;

    // ����ȷ����ʱ�������Ч
    configASSERT( xTimer );

    taskENTER_CRITICAL();
    {
        // ���ٽ������ö�ʱ�����±�ʶ��
        pxTimer->pvTimerID = pvNewID;
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*
 * ���жϷ�������й���һ���������á�
 *
 * ����:
 * �˺����������ж��������а���һ��������ִ�С��ú������ڶ�ʱ���ػ����������б�ִ�С�
 *
 * ����:
 * @param xFunctionToPend: ��Ҫ���Ժ�ִ�еĺ���ָ�롣
 * @param pvParameter1: ���ݸ������ĵ�һ��������
 * @param ulParameter2: ���ݸ������ĵڶ���������
 * @param pxHigherPriorityTaskWoken: ����˺������¸������ȼ������񱻻��ѣ�������Ϊ pdTRUE��
 *
 * ����ֵ:
 * ���� xQueueSendFromISR �Ľ����ָʾ�Ƿ�ɹ�������Ϣ��
 */
BaseType_t xTimerPendFunctionCallFromISR( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, BaseType_t *pxHigherPriorityTaskWoken )
{
    DaemonTaskMessage_t xMessage;
    BaseType_t xReturn;

    /* ʹ�ú������������Ϣ�����䷢�����ػ��������� */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK_FROM_ISR;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* ���жϷ��������ʱ�����з�����Ϣ�� */
    xReturn = xQueueSendFromISR( xTimerQueue, &xMessage, pxHigherPriorityTaskWoken );

    /* ��¼�����������Ϣ�� */
    tracePEND_FUNC_CALL_FROM_ISR( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

#if( INCLUDE_xTimerPendFunctionCall == 1 )

/*
 * ����һ���������á�
 *
 * ����:
 * �˺��������������������а���һ��������ִ�С��ú������ڶ�ʱ���ػ����������б�ִ�С�
 *
 * ����:
 * @param xFunctionToPend: ��Ҫ���Ժ�ִ�еĺ���ָ�롣
 * @param pvParameter1: ���ݸ������ĵ�һ��������
 * @param ulParameter2: ���ݸ������ĵڶ���������
 * @param xTicksToWait: �ȴ���ʱ�䣨�δ����������ڵȴ���Ϣ���͵����С�
 *
 * ����ֵ:
 * ���� xQueueSendToBack �Ľ����ָʾ�Ƿ�ɹ�������Ϣ��
 */
BaseType_t xTimerPendFunctionCall( PendedFunction_t xFunctionToPend, void *pvParameter1, uint32_t ulParameter2, TickType_t xTicksToWait )
{
    DaemonTaskMessage_t xMessage;
    BaseType_t xReturn;

    /* ֻ���ڴ�����ʱ��֮������������֮����ô˺�����
       ��Ϊ�ڴ�֮ǰ����ʱ�����в����ڡ� */
    configASSERT( xTimerQueue );

    /* ʹ�ú������������Ϣ�����䷢�����ػ��������� */
    xMessage.xMessageID = tmrCOMMAND_EXECUTE_CALLBACK;
    xMessage.u.xCallbackParameters.pxCallbackFunction = xFunctionToPend;
    xMessage.u.xCallbackParameters.pvParameter1 = pvParameter1;
    xMessage.u.xCallbackParameters.ulParameter2 = ulParameter2;

    /* ����Ϣ���͵���ʱ�����е�ĩβ�����ȴ�ָ����ʱ�䡣 */
    xReturn = xQueueSendToBack( xTimerQueue, &xMessage, xTicksToWait );

    /* ��¼�����������Ϣ�� */
    tracePEND_FUNC_CALL( xFunctionToPend, pvParameter1, ulParameter2, xReturn );

    return xReturn;
}

#endif /* INCLUDE_xTimerPendFunctionCall */
/*-----------------------------------------------------------*/

/* ���Ӧ�ó���û�����ð��������ʱ�����ܣ���ô����Դ�ļ����ᱻ���ԡ����������������ʱ�����ܣ���ȷ���� FreeRTOSConfig.h �н� configUSE_TIMERS ����Ϊ 1��*/
#endif /* configUSE_TIMERS == 1 */



