
#include <stdlib.h>
#include <string.h>

/* 
 * ���� MPU_WRAPPERS_INCLUDED_FROM_API_FILE ����Է�ֹ task.h ���¶���
 * ���е� API ������ʹ�� MPU ��װ��������Ӧ��ֻ�� task.h ��������
 * Ӧ�ó����ļ���ʱ���С�
 */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#if ( configUSE_CO_ROUTINES == 1 )
	#include "croutine.h"
#endif

/* 
 * ���� lint ���� e961 �� e750����Ϊ����һ��������� MISRA ���������
 * ���� MPU �˿�Ҫ��������ͷ�ļ��ж��� MPU_WRAPPERS_INCLUDED_FROM_API_FILE��
 * ���ڱ��ļ��в���Ҫ���壬�Ա�������ȷ����Ȩ�����Ȩ���Ӻͷ��á�
 */
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE /*lint !e961 !e750.*/


/* 
 * ���� cRxLock �� cTxLock �ṹ��Ա�ĳ�����
 */
#define queueUNLOCKED                    ( ( int8_t ) -1 ) // ������н���״̬
#define queueLOCKED_UNMODIFIED           ( ( int8_t ) 0 )  // �������������δ�޸ĵ�״̬

/* 
 * �� Queue_t �ṹ���ڱ�ʾ��������ʱ���� pcHead �� pcTail ��Ա��Ϊָ����д洢����ָ�롣
 * �� Queue_t �ṹ���ڱ�ʾ������ʱ��pcHead �� pcTail ָ���ǲ���Ҫ�ģ����� pcHead ָ�뱻����Ϊ NULL��
 * ��ָʾ pcTail ָ��ʵ����ָ�򻥳��������ߣ�����У���Ϊ��ȷ������Ŀɶ��ԣ����ܴ��ڶ������ṹ��Ա��˫��ʹ�ã�
 * ��Ȼ���������ӳ�䵽 pcHead �� pcTail �ṹ��Ա����һ��ʵ�ַ�����ʹ�������壨union������ʹ��������Υ���˱����׼
 * ��������˫��ʹ��Ҳ�����ı��˽ṹ��Ա���͵����������Ա�׼�����⣩��
 */
#define pxMutexHolder                    pcTail // �����������ߵı���
#define uxQueueType                      pcHead // �������͵ı���
#define queueQUEUE_IS_MUTEX              NULL   // ��ʾ�����ǻ������ı�־

/* 
 * �ź���ʵ���ϲ��洢Ҳ���������ݣ��������Ŀ��СΪ�㡣
 */
#define queueSEMAPHORE_QUEUE_ITEM_LENGTH  ( ( UBaseType_t ) 0 )
#define queueMUTEX_GIVE_BLOCK_TIME        ( ( TickType_t ) 0U )

#if( configUSE_PREEMPTION == 0 )
    /* ���ʹ�õ���Э��ʽ����������Ӧ������Ϊ������һ�����ȼ����ߵ������ִ�е����ò��� */
    #define queueYIELD_IF_USING_PREEMPTION()
#else
    /* ���ʹ�õ�����ռʽ������������Ҫִ�е����ò��� */
    #define queueYIELD_IF_USING_PREEMPTION() portYIELD_WITHIN_API()
#endif

/*
 * �����˵�����ʹ�õĶ��С�
 * ��Ŀ��ͨ���������������Ŷӵġ�������μ��������ӣ�http://www.freertos.org/Embedded-RTOS-Queues.html
 */
typedef struct QueueDefinition
{
    int8_t *pcHead;                    /*!< ָ����д洢������ʼλ�á� */
    int8_t *pcTail;                    /*!< ָ����д洢��ĩβ���ֽڡ�������ֽ����ȴ洢����������Ķ�һ����������Ϊ��ǡ� */
    int8_t *pcWriteTo;                 /*!< ָ��洢���е���һ������λ�á� */

    union                              /*!< ʹ���������ǶԱ����׼��һ�����⣬��ȷ����������Ľṹ��Ա����ͬʱ���֣��˷� RAM���� */
    {
        int8_t *pcReadFrom;            /*!< ���ṹ��������ʱ��ָ�����һ�δ��ж�ȡ�������λ�á� */
        UBaseType_t uxRecursiveCallCount; /*!< ���ṹ����������ʱ����¼�˵ݹ顰��ȡ���������Ĵ����� */
    } u;

    List_t xTasksWaitingToSend;        /*!< �ȴ���˶��з������ݵı����������б������ȼ�˳��洢�� */
    List_t xTasksWaitingToReceive;     /*!< �ȴ��Ӵ˶��ж�ȡ���ݵı����������б������ȼ�˳��洢�� */

    volatile UBaseType_t uxMessagesWaiting; /*!< ��ǰ�����е������� */
    UBaseType_t uxLength;              /*!< ���еĳ��ȣ�����Ϊ���п������ɵ��������������ֽ����� */
    UBaseType_t uxItemSize;            /*!< ���н������ÿ��Ĵ�С�� */

    volatile int8_t cRxLock;           /*!< �洢�ڶ��������ڼ�Ӷ��н��գ��Ƴ�����������������δ����ʱ����Ϊ queueUNLOCKED�� */
    volatile int8_t cTxLock;           /*!< �洢�ڶ��������ڼ�����з��ͣ���ӣ���������������δ����ʱ����Ϊ queueUNLOCKED�� */

    #if( ( configSUPPORT_STATIC_ALLOCATION == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )
        uint8_t ucStaticallyAllocated; /*!< ���ʹ�þ�̬������ڴ棬������Ϊ pdTRUE����ȷ�����᳢���ͷ��ڴ档 */
    #endif

    #if ( configUSE_QUEUE_SETS == 1 )
        struct QueueDefinition *pxQueueSetContainer; /*!< ��������˶��м����ܣ���ָ����������Ķ��м��� */
    #endif

    #if ( configUSE_TRACE_FACILITY == 1 )
        UBaseType_t uxQueueNumber;     /*!< ���б�ţ����ڸ�����ʩ�� */
        uint8_t ucQueueType;            /*!< �������ͣ����ڸ�����ʩ�� */
    #endif

} xQUEUE;

/*
 * �����ɵ� xQUEUE ���ƣ�������ת��Ϊ�µ� Queue_t ���ƣ�
 * �Ա��ܹ�ʹ�ýϾɵ��ں˸�֪��������
 */
typedef xQUEUE Queue_t;

/*-----------------------------------------------------------*/

/*
 * ����ע���ֻ���ں˸�֪��������λ���нṹ��һ���ֶΡ�
 * ��û��������;�������һ����ѡ�����
 */
#if ( configQUEUE_REGISTRY_SIZE > 0 )

    /* 
     * �洢�ڶ���ע��������е����͡�
     * ������Ϊÿ�����з���һ�����ƣ�ʹ�ں˸�֪���Ը����û��Ѻá�
     */
    typedef struct QUEUE_REGISTRY_ITEM
    {
        const char *pcQueueName; /*!< lint !e971 ����ʹ��δ�޶��� char ���������ַ����͵��ַ��� */
        QueueHandle_t xHandle;   /*!< ���еľ���� */
    } xQueueRegistryItem;

    /* 
     * �����ɵ� xQueueRegistryItem ���ƣ�������ת��Ϊ�µ� QueueRegistryItem_t ���ƣ�
     * �Ա��ܹ�ʹ�ýϾɵ��ں˸�֪��������
     */
    typedef xQueueRegistryItem QueueRegistryItem_t;

    /* 
     * ����ע���ֻ��һ�� QueueRegistryItem_t �ṹ�����顣
     * �ṹ�е� pcQueueName ��ԱΪ NULL ��ʾ������λ���ǿ��еġ�
     */
    PRIVILEGED_DATA QueueRegistryItem_t xQueueRegistry[ configQUEUE_REGISTRY_SIZE ];

#endif /* configQUEUE_REGISTRY_SIZE */

/*
 * ������ prvLockQueue ���������Ķ��С�
 * �������в�������ֹ�жϷ������̣�ISR���������ӻ��Ƴ��������ֹ�жϷ������̴Ӷ����¼��б����Ƴ�����
 * ����жϷ������̷��ֶ��б�����������������Ӧ�Ķ���������������ָʾ������Ҫ��������������
 * �����н���ʱ��������Щ��������������ȡ�ʵ��Ĵ�ʩ��
 */
static void prvUnlockQueue(Queue_t * const pxQueue) PRIVILEGED_FUNCTION;

/*
 * ʹ���ٽ�����ȷ���������Ƿ������ݡ�
 *
 * @return ���������û���κ���򷵻� pdTRUE�����򷵻� pdFALSE��
 */
static BaseType_t prvIsQueueEmpty(const Queue_t *pxQueue) PRIVILEGED_FUNCTION;

/*
 * ʹ���ٽ�����ȷ���������Ƿ��пռ䡣
 *
 * @return ���������û�пռ䣬�򷵻� pdTRUE�����򷵻� pdFALSE��
 */
static BaseType_t prvIsQueueFull(const Queue_t *pxQueue) PRIVILEGED_FUNCTION;

/*
 * ��һ�����ݸ��Ƶ������У����Ը��Ƶ����е�ǰ�˻��ˡ�
 */
static BaseType_t prvCopyDataToQueue(Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition) PRIVILEGED_FUNCTION;

/*
 * �Ӷ����и���һ�����ݡ�
 */
static void prvCopyDataFromQueue(Queue_t * const pxQueue, void * const pvBuffer) PRIVILEGED_FUNCTION;

#if (configUSE_QUEUE_SETS == 1)
    /*
     * �������Ƿ����ڶ��м���������ǵĻ���֪ͨ���м��ö��а������ݡ�
     */
    static BaseType_t prvNotifyQueueSetContainer(const Queue_t * const pxQueue, const BaseType_t xCopyPosition) PRIVILEGED_FUNCTION;
#endif

/*
 * �ھ�̬��̬���� Queue_t �ṹ����ã������ṹ�ĳ�Ա��
 */
static void prvInitialiseNewQueue(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize, uint8_t *pucQueueStorage, const uint8_t ucQueueType, Queue_t *pxNewQueue) PRIVILEGED_FUNCTION;

/*
 * ��������һ������Ķ������͡�������һ��������ʱ�����ȴ������У�
 * Ȼ����� prvInitialiseMutex() ����������������Ϊ��������
 */
#if( configUSE_MUTEXES == 1 )
    static void prvInitialiseMutex(Queue_t *pxNewQueue) PRIVILEGED_FUNCTION;
#endif

/*-----------------------------------------------------------*/

/*
 * ��Ƕ���Ϊ����״̬�ĺꡣ�������п��Է�ֹ�жϷ������̣�ISR�����ʶ����¼��б�
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
 * @brief ���ö��е�״̬��
 * 
 * �˺����������ö��У�ʹ��ָ�����ʼ״̬����ն����е�������Ϣ�������ݲ��� xNewQueue ��ֵ�������Ƿ����³�ʼ�����еĵȴ��б�
 * 
 * @param xQueue ��Ҫ�����õĶ��еľ����
 * @param xNewQueue һ����־λ������ָʾ�Ƿ����ʼ���¶���һ��������У�
 *                  - ���Ϊ pdFALSE���򲻳�ʼ���ȴ��б�
 *                  - ���Ϊ pdTRUE�����ʼ���ȴ��б�
 * 
 * @return ���� pdPASS����������ǰ�汾������һ���ԡ�
 */
BaseType_t xQueueGenericReset(QueueHandle_t xQueue, BaseType_t xNewQueue)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // ȷ�����о������ NULL
    configASSERT(pxQueue);

    // �����ٽ�����ȷ���Զ���״̬���޸���ԭ�ӵ�
    taskENTER_CRITICAL();
    {
        // �����е�βָ������Ϊ���д洢����ĩβλ��
        pxQueue->pcTail = pxQueue->pcHead + (pxQueue->uxLength * pxQueue->uxItemSize);

        // �������е���Ϣ��������Ϊ 0
        pxQueue->uxMessagesWaiting = (UBaseType_t)0U;

        // ��дָ������Ϊ���д洢������ʼλ��
        pxQueue->pcWriteTo = pxQueue->pcHead;

        // ����ָ������Ϊ�������һ����ĺ��棨������г���Ϊ n�����ָ��ָ�� n * ���С��
        pxQueue->u.pcReadFrom = pxQueue->pcHead + ((pxQueue->uxLength - (UBaseType_t)1U) * pxQueue->uxItemSize);

        // �����պͷ��͵�������������Ϊδ����״̬
        pxQueue->cRxLock = queueUNLOCKED;
        pxQueue->cTxLock = queueUNLOCKED;

        // ���� xNewQueue ����ֵ�������Ƿ��ʼ���ȴ��б�
        if (xNewQueue == pdFALSE)
        {
            /* �����������Ϊ�ȴ��Ӷ��ж�ȡ���ݶ�����������ô��Щ���񽫱�������״̬��
             * ��Ϊ���˺����˳��󣬶�����Ȼ�ǿյġ�
             * �����������Ϊ�ȴ������д�����ݶ�����������ôӦ�ý������һ�������������
             * ��Ϊ���˺����˳��󣬽����������д�����ݡ� */
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
            /* ȷ���¼����п�ʼʱ������ȷ��״̬�� */
            vListInitialise(&(pxQueue->xTasksWaitingToSend));
            vListInitialise(&(pxQueue->xTasksWaitingToReceive));
        }
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    /* Ϊ������ǰ�汾�ĵ������屣��һ�£�����һ��ֵ�� */
    return pdPASS;
}
/*-----------------------------------------------------------*/

#if( configSUPPORT_STATIC_ALLOCATION == 1 )
/**
 * @brief ��̬����һ�����С�
 * 
 * �˺������ھ�̬����һ�����У�������ʼ�����еĸ����ֶΣ��������ṩ�Ĳ������ö��еĳ��Ⱥ����С��
 * 
 * @param uxQueueLength ���еĳ��ȣ�����Ϊ���п������ɵ�������
 * @param uxItemSize ÿ��������Ĵ�С�����ֽ�Ϊ��λ����
 * @param pucQueueStorage ָ��̬����Ķ��д洢�����ָ�롣
 * @param pxStaticQueue ָ��̬����Ķ��нṹ��ָ�롣
 * @param ucQueueType ���е����͡�
 * 
 * @return ����ָ���´������еľ����
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

    // ȷ�����г��ȴ�����
    configASSERT(uxQueueLength > (UBaseType_t)0);

    // ��̬���нṹ�Ͷ��д洢�������ṩ
    configASSERT(pxStaticQueue != NULL);

    // ������С��Ϊ�㣬��Ӧ�ṩ���д洢����������СΪ�㣬��Ӧ�ṩ���д洢��
    configASSERT(!(pucQueueStorage != NULL && uxItemSize == 0));
    configASSERT(!(pucQueueStorage == NULL && uxItemSize != 0));

    #if (configASSERT_DEFINED == 1)
    {
        // ȷ�� StaticQueue_t �ṹ�Ĵ�С����ʵ�ʶ��нṹ�Ĵ�С
        volatile size_t xSize = sizeof(StaticQueue_t);
        configASSERT(xSize == sizeof(Queue_t));
    }
    #endif /* configASSERT_DEFINED */

    // ʹ�ô��ݽ����ľ�̬����Ķ��е�ַ
    pxNewQueue = (Queue_t *)pxStaticQueue; /*lint !e740 ���������ת���Ǻ���ģ���Ϊ��Щ�ṹ��Ƴɾ�����ͬ�Ķ��뷽ʽ�����Ҵ�Сͨ�����Խ��м�顣 */

    if (pxNewQueue != NULL)
    {
        #if (configSUPPORT_DYNAMIC_ALLOCATION == 1)
        {
            // ���п��Ծ�̬��̬���䣬��˼�¼�ö����Ǿ�̬����ģ��Է��Ժ�ɾ������ʱ�����ͷ��ڴ�
            pxNewQueue->ucStaticallyAllocated = pdTRUE;
        }
        #endif /* configSUPPORT_DYNAMIC_ALLOCATION */

        // ��ʼ���µĶ��нṹ
        prvInitialiseNewQueue(uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue);
    }

    // �����´����Ķ��о��
    return pxNewQueue;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

#if( configSUPPORT_DYNAMIC_ALLOCATION == 1 )

/*
 * @brief xQueueGenericCreate ����һ�����С�
 *
 * �ú������ڴ���һ��ָ�����Ⱥ�Ԫ�ش�С�Ķ��У���������Ҫ��ʼ�����нṹ��
 *
 * @param uxQueueLength ���еĳ��ȣ������п������ɵ�Ԫ��������
 * @param uxItemSize ������ÿ��Ԫ�صĴ�С�����ֽ�Ϊ��λ����
 * @param ucQueueType ���е����ͣ��������ֲ�ͬ���͵Ķ��С�
 *
 * @return ���ش����Ķ��о�����������ʧ�ܣ��򷵻� NULL��
 */
QueueHandle_t xQueueGenericCreate(
    const UBaseType_t uxQueueLength, 
    const UBaseType_t uxItemSize, 
    const uint8_t ucQueueType )
{
    Queue_t *pxNewQueue;                     // ����ָ����нṹ��ָ��
    size_t xQueueSizeInBytes;                // �������������ֽ���
    uint8_t *pucQueueStorage;                // ָ����д洢����ָ��

    // ȷ�����г��ȴ���0
    configASSERT( uxQueueLength > ( UBaseType_t ) 0 );

    // �������������ֽ���
    if( uxItemSize == ( UBaseType_t ) 0 )
    {
        xQueueSizeInBytes = ( size_t ) 0;
    }
    else
    {
        xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize );
    }

    // ������нṹ���洢������Ŀռ�
    // ʹ�� pvPortMalloc �����ڴ棬����һ����Я���ڴ���亯��
    pxNewQueue = ( Queue_t * ) pvPortMalloc( sizeof( Queue_t ) + xQueueSizeInBytes );

    if( pxNewQueue != NULL )
    {
        // ������д洢������ʼ��ַ
        // ���д洢�������Ŷ��нṹ֮��
        pucQueueStorage = ( ( uint8_t * ) pxNewQueue ) + sizeof( Queue_t );

        // ��������˾�̬����֧�֣������ö��еı�־
        #if( configSUPPORT_STATIC_ALLOCATION == 1 )
        {
            pxNewQueue->ucStaticallyAllocated = pdFALSE;
        }
        #endif /* configSUPPORT_STATIC_ALLOCATION */

        // ��ʼ�����нṹ
        // ������г��ȡ�Ԫ�ش�С���洢����ַ�����������Լ����нṹָ��
        prvInitialiseNewQueue( uxQueueLength, uxItemSize, pucQueueStorage, ucQueueType, pxNewQueue );
    }

    // ���ض��о��
    return pxNewQueue;
}
#endif /* configSUPPORT_STATIC_ALLOCATION */
/*-----------------------------------------------------------*/

/**
 * @brief ��ʼ��һ���µĶ��С�
 * 
 * �˺������ڳ�ʼ��һ���µĶ��нṹ���������ö��еĻ������ԣ��糤�ȡ����С�ȣ�����������ѡ��ִ�ж���ĳ�ʼ�����衣
 * 
 * @param uxQueueLength ���еĳ��ȣ����п������ɵ���������
 * @param uxItemSize ÿ��������Ĵ�С�����ֽ�Ϊ��λ����
 * @param pucQueueStorage ָ����д洢�����ָ�롣
 * @param ucQueueType ���е����͡�
 * @param pxNewQueue ָ���¶��нṹ��ָ�롣
 */
static void prvInitialiseNewQueue(
    const UBaseType_t uxQueueLength,
    const UBaseType_t uxItemSize,
    uint8_t *pucQueueStorage,
    const uint8_t ucQueueType,
    Queue_t *pxNewQueue
)
{
    // �Ƴ�����������δʹ�ò����ľ��棬���������˸�����ʩ
    (void) ucQueueType;

    if (uxItemSize == (UBaseType_t)0)
    {
        // ���û��Ϊ���з���洢�������� pcHead ��������Ϊ NULL����Ϊ NULL ������ʾ������Ϊ������ʹ�á�
        // ��ˣ��� pcHead ����Ϊָ����б����һ���޺�ֵ�����ֵ���ڴ�ӳ��������֪�ġ�
        pxNewQueue->pcHead = (int8_t *)pxNewQueue;
    }
    else
    {
        // �� pcHead ����Ϊָ����д洢������ʼλ�á�
        pxNewQueue->pcHead = (int8_t *)pucQueueStorage;
    }

    // ��ʼ�����г�Ա������еĳ��Ⱥ����С�ȡ�
    pxNewQueue->uxLength = uxQueueLength;
    pxNewQueue->uxItemSize = uxItemSize;

    // ���ö��У���ʼ�����е�״̬��
    (void)xQueueGenericReset(pxNewQueue, pdTRUE);

    #if (configUSE_TRACE_FACILITY == 1)
    {
        // ��������˸�����ʩ�����¼�������͡�
        pxNewQueue->ucQueueType = ucQueueType;
    }
    #endif /* configUSE_TRACE_FACILITY */

    #if (configUSE_QUEUE_SETS == 1)
    {
        // ��������˶��м����ܣ����ʼ�����м�����ָ��Ϊ NULL��
        pxNewQueue->pxQueueSetContainer = NULL;
    }
    #endif /* configUSE_QUEUE_SETS */

    // ��¼���д����ĸ�����Ϣ��
    traceQUEUE_CREATE(pxNewQueue);
}
/*-----------------------------------------------------------*/

#if( configUSE_MUTEXES == 1 )

	/**
 * @brief ��ʼ��һ����������
 * 
 * �˺������ڽ�һ����ͨ����ת��Ϊ�������������û�����������ض���Ա��
 * 
 * @param pxNewQueue ָ���´����Ķ��нṹ��ָ�롣
 */
static void prvInitialiseMutex(Queue_t *pxNewQueue)
{
    if (pxNewQueue != NULL)
    {
        // ���д��������Ѿ�Ϊͨ�ö�����ȷ���������ж��нṹ��Ա�����˺������ڴ���һ����������
        // ��д��Щ��Ҫ��ͬ���õĳ�Ա���ر������ȼ��̳��������Ϣ��
        pxNewQueue->pxMutexHolder = NULL;
        pxNewQueue->uxQueueType = queueQUEUE_IS_MUTEX;

        // �������һ���ݹ黥������
        pxNewQueue->u.uxRecursiveCallCount = 0;

        // ��¼�����������ĸ�����Ϣ��
        traceCREATE_MUTEX(pxNewQueue);

        // ʹ�ź�������Ԥ��״̬��
        (void)xQueueGenericSend(pxNewQueue, NULL, (TickType_t)0U, queueSEND_TO_BACK);
    }
    else
    {
        // �������ָ��Ϊ NULL�����¼����������ʧ�ܵĸ�����Ϣ��
        traceCREATE_MUTEX_FAILED();
    }
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/*
 * @brief xQueueCreateMutex ����һ�������ź�����
 *
 * �ú��������ṩ�����ʹ���һ�����ڻ�����ʿ��ƵĶ��У���������г�ʼ����
 *
 * @param ucQueueType ָ�������ź��������ͣ�����������ź�����ݹ��ź����ȡ�
 *
 * @return ���ش����Ļ����ź���������������ʧ�ܣ��򷵻� NULL��
 */
QueueHandle_t xQueueCreateMutex( const uint8_t ucQueueType )
{
    Queue_t *pxNewQueue;                    // ����ָ����нṹ��ָ��
    const UBaseType_t uxMutexLength = ( UBaseType_t ) 1; // ���廥���ź����ĳ���Ϊ1
    const UBaseType_t uxMutexSize = ( UBaseType_t ) 0;   // ���廥���ź����Ĵ�СΪ0���������ռ䣩

    // ͨ��ͨ�ö��д����������������ź���
    // uxMutexLength ��ʾ�����ܴ�ŵ�Ԫ����������������1����Ϊ������ֻ����һ�������ߡ�
    // uxMutexSize ��ʾ������ÿ��Ԫ�صĴ�С����������0����Ϊ����������Ҫ�洢�κ����ݡ�
    pxNewQueue = ( Queue_t * ) xQueueGenericCreate( uxMutexLength, uxMutexSize, ucQueueType );

    // ��������ɹ������ʼ�������ź���
    // ��ʼ������ͨ����������һЩ�ڲ�״̬����������Ȩ��־�ȡ�
    if( pxNewQueue != NULL )
    {
        prvInitialiseMutex( pxNewQueue );
    }

    // ���ش����Ļ����ź������
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_MUTEXES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

/**
 * @brief ��̬����һ����������
 * 
 * �˺������ھ�̬����һ��������������ʼ����״̬��
 * 
 * @param ucQueueType �����������͡�
 * @param pxStaticQueue ָ��̬����Ķ��нṹ��ָ�롣
 * 
 * @return ����ָ���´����������ľ����
 */
QueueHandle_t xQueueCreateMutexStatic(const uint8_t ucQueueType, StaticQueue_t *pxStaticQueue)
{
    Queue_t *pxNewQueue;
    const UBaseType_t uxMutexLength = (UBaseType_t)1, uxMutexSize = (UBaseType_t)0;

    // ��ֹ�������� configUSE_TRACE_FACILITY ������ 1 ʱ����δʹ�õĲ������档
    (void)ucQueueType;

    // ʹ��ָ���ľ�̬���нṹ����һ������Ϊ 1�����СΪ 0 �Ķ��С�
    pxNewQueue = (Queue_t *)xQueueGenericCreateStatic(uxMutexLength, uxMutexSize, NULL, pxStaticQueue, ucQueueType);

    // ��ʼ����������
    prvInitialiseMutex(pxNewQueue);

    // �����´����Ļ����������
    return pxNewQueue;
}

#endif /* configUSE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( ( configUSE_MUTEXES == 1 ) && ( INCLUDE_xSemaphoreGetMutexHolder == 1 ) )

/**
 * @brief ��ȡ�������ĳ����ߡ�
 * 
 * �˺������ڻ�ȡָ���������ĳ����ߡ�ע�⣬�˺����� xSemaphoreGetMutexHolder() ���ã���Ӧ��ֱ�ӵ��á�
 * ע�⣺�����жϵ��������Ƿ�Ϊ�����������ߵ����÷�����������ȷ����������������ݵĺ÷�������Ϊ�����߿����������ٽ����˳��ͺ�������֮�䷢���仯��
 * 
 * @param xSemaphore ָ�򻥳����ľ����
 * 
 * @return ����ָ�򻥳��������ߵ�ָ�룬���û�г����ߣ��򷵻� NULL��
 */
void* xQueueGetMutexHolder(QueueHandle_t xSemaphore)
{
    void *pxReturn;

    // �����ٽ�������ȷ���Ի�����״̬�ķ�����ԭ�ӵ�
    taskENTER_CRITICAL();
    {
        // ��鴫��ľ���Ƿ����һ��������
        if (((Queue_t *)xSemaphore)->uxQueueType == queueQUEUE_IS_MUTEX)
        {
            // ��ȡ�����������ߵ�ָ��
            pxReturn = (void *)((Queue_t *)xSemaphore)->pxMutexHolder;
        }
        else
        {
            // ������ǻ��������򷵻� NULL
            pxReturn = NULL;
        }
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    // ���ػ����������ߵ�ָ��
    return pxReturn;
} /*lint !e818 xSemaphore ������ָ������ָ�룬��Ϊ����һ�����Ͷ��塣*/

#endif
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

/**
 * @brief �ݹ���ͷŻ�������
 * 
 * �˺������ڵݹ���ͷ�һ���������������ǰ������иû�����������ٵݹ����������ݹ������Ϊ�㣬�򽫻������黹�����С�
 * 
 * @param xMutex �������ľ����
 * 
 * @return ���� pdPASS ��ʾ�ɹ��ͷŻ����������� pdFAIL ��ʾ�ͷ�ʧ�ܣ���ǰ���񲻳��л���������
 */
BaseType_t xQueueGiveMutexRecursive(QueueHandle_t xMutex)
{
    BaseType_t xReturn; // ����ֵ
    Queue_t * const pxMutex = (Queue_t *)xMutex; // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxMutex);

    // ��鵱ǰ�����Ƿ��Ѿ����иû�����
    if (pxMutex->pxMutexHolder == (void *)xTaskGetCurrentTaskHandle()) /*lint !e961 Not a redundant cast as TaskHandle_t is a typedef.*/
    {
        // ��¼�ݹ��ͷŻ������Ĳ���
        traceGIVE_MUTEX_RECURSIVE(pxMutex);

        // ���ٵݹ���ü���
        (pxMutex->u.uxRecursiveCallCount)--;

        // ����ݹ���ü�����Ϊ0�����ʾ��ǰ�����ٳ��л�����
        if (pxMutex->u.uxRecursiveCallCount == (UBaseType_t)0)
        {
            // ���������黹������
            (void)xQueueGenericSend(pxMutex, NULL, queueMUTEX_GIVE_BLOCK_TIME, queueSEND_TO_BACK);
        }
        else
        {
            // ����ݹ���ü�����Ϊ0�����ʾ��ǰ������Ȼ���л�����
            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
        }

        // ���÷���ֵΪ�ɹ�
        xReturn = pdPASS;
    }
    else
    {
        // �����ǰ����û�г��л������������ͷ�
        xReturn = pdFAIL;

        // ��¼�ݹ��ͷŻ�����ʧ�ܵ���Ϣ
        traceGIVE_MUTEX_RECURSIVE_FAILED(pxMutex);
    }

    // �����ͷŻ������Ľ��
    return xReturn;
}

#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if ( configUSE_RECURSIVE_MUTEXES == 1 )

/**
 * @brief �ݹ�ػ�ȡ��������
 * 
 * �˺������ڵݹ�ػ�ȡһ���������������ǰ�����Ѿ����иû������������ӵݹ���������򣬳��ԴӶ����н������ݣ�ʵ�����ǳ��Ի�ȡ����������
 * 
 * @param xMutex �������ľ����
 * @param xTicksToWait �ȴ�ʱ�䣨�� tick Ϊ��λ����
 * 
 * @return ���� pdPASS ��ʾ�ɹ���ȡ������������ pdFAIL ��ʾ��ȡʧ�ܡ�
 */
BaseType_t xQueueTakeMutexRecursive(QueueHandle_t xMutex, TickType_t xTicksToWait)
{
    BaseType_t xReturn; // ����ֵ
    Queue_t * const pxMutex = (Queue_t *)xMutex; // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxMutex);

    // ��¼�ݹ��ȡ�������Ĳ���
    traceTAKE_MUTEX_RECURSIVE(pxMutex);

    // ��鵱ǰ�����Ƿ��Ѿ����иû�����
    if (pxMutex->pxMutexHolder == (void *)xTaskGetCurrentTaskHandle()) /*lint !e961 Cast is not redundant as TaskHandle_t is a typedef.*/
    {
        // �����ǰ�����Ѿ����иû������������ӵݹ���ü���
        (pxMutex->u.uxRecursiveCallCount)++;
        xReturn = pdPASS; // ���÷���ֵΪ�ɹ�
    }
    else
    {
        // ���ԴӶ����н������ݣ�ʵ�����ǳ��Ի�ȡ��������
        xReturn = xQueueGenericReceive(pxMutex, NULL, xTicksToWait, pdFALSE);

        if (xReturn != pdFAIL)
        {
            // ����ɹ���ȡ�������������ӵݹ���ü���
            (pxMutex->u.uxRecursiveCallCount)++;
        }
        else
        {
            // ���δ�ܻ�ȡ�����������¼��ȡʧ����Ϣ
            traceTAKE_MUTEX_RECURSIVE_FAILED(pxMutex);
        }
    }

    // ���ػ�ȡ�������Ľ��
    return xReturn;
}
#endif /* configUSE_RECURSIVE_MUTEXES */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_STATIC_ALLOCATION == 1 ) )

/**
 * @brief ��̬����һ�������ź�����
 * 
 * �˺������ھ�̬����һ�������ź���������ʼ����������ֵ�ͳ�ʼ����ֵ��
 * 
 * @param uxMaxCount �ź�����������ֵ��
 * @param uxInitialCount �ź����ĳ�ʼ����ֵ��
 * @param pxStaticQueue ָ��̬����Ķ��нṹ��ָ�롣
 * 
 * @return ����ָ���´����ļ����ź����ľ����
 */
QueueHandle_t xQueueCreateCountingSemaphoreStatic(
    const UBaseType_t uxMaxCount,
    const UBaseType_t uxInitialCount,
    StaticQueue_t *pxStaticQueue
)
{
    QueueHandle_t xHandle;

    // ���ԣ�ȷ��������ֵ��Ϊ��
    configASSERT(uxMaxCount != 0);

    // ���ԣ�ȷ����ʼ����ֵ������������ֵ
    configASSERT(uxInitialCount <= uxMaxCount);

    // ����һ�������ź���
    xHandle = xQueueGenericCreateStatic(
        uxMaxCount,          // ���г���
        queueSEMAPHORE_QUEUE_ITEM_LENGTH, // �������С
        NULL,                // ���д洢��
        pxStaticQueue,       // ��̬����Ķ��нṹ
        queueQUEUE_TYPE_COUNTING_SEMAPHORE // ��������
    );

    if (xHandle != NULL)
    {
        // �����ź����ĳ�ʼ����ֵ
        ((Queue_t *)xHandle)->uxMessagesWaiting = uxInitialCount;

        // ��¼�����ź��������ĸ�����Ϣ
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        // ��¼�����ź�������ʧ�ܵĸ�����Ϣ
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    // �����´����ļ����ź������
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

#if( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/**
 * @brief ����һ�������ź�����
 * 
 * �˺������ڴ���һ�������ź���������ʼ����������ֵ�ͳ�ʼ����ֵ��
 * 
 * @param uxMaxCount �ź�����������ֵ��
 * @param uxInitialCount �ź����ĳ�ʼ����ֵ��
 * 
 * @return ����ָ���´����ļ����ź����ľ����
 */
QueueHandle_t xQueueCreateCountingSemaphore(
    const UBaseType_t uxMaxCount,
    const UBaseType_t uxInitialCount
)
{
    QueueHandle_t xHandle;                   // ������о��

    // ȷ����������Ϊ��
    configASSERT(uxMaxCount != 0);

    // ȷ����ʼ����������������
    configASSERT(uxInitialCount <= uxMaxCount);

    // ͨ��ͨ�ö��д����������������ź���
    xHandle = xQueueGenericCreate(
        uxMaxCount,                          // ���г���
        queueSEMAPHORE_QUEUE_ITEM_LENGTH,    // �������С
        queueQUEUE_TYPE_COUNTING_SEMAPHORE   // ��������
    );

    if (xHandle != NULL)
    {
        // ���ö����е���Ϣ�ȴ����������������ĳ�ʼֵ��
        ((Queue_t *)xHandle)->uxMessagesWaiting = uxInitialCount;

        // ��¼���������ź�������Ϣ����ѡ�ĸ��ٹ��ܣ�
        traceCREATE_COUNTING_SEMAPHORE();
    }
    else
    {
        // ��¼���������ź���ʧ�ܵ���Ϣ����ѡ�ĸ��ٹ��ܣ�
        traceCREATE_COUNTING_SEMAPHORE_FAILED();
    }

    // ���ض��о��
    return xHandle;
}

#endif /* ( ( configUSE_COUNTING_SEMAPHORES == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) ) */
/*-----------------------------------------------------------*/

/*
 * @brief xQueueGenericSend ����������з������ݡ�
 *
 * �ú���������ָ���Ķ����з������ݣ���֧�ֳ�ʱ�͸��ǣ�overwrite��ģʽ��
 *
 * @param xQueue ��Ҫ���䷢�����ݵĶ��о����
 * @param pvItemToQueue Ҫ���͵������
 * @param xTicksToWait �ȴ��������ݵ����ʱ�䣨�Եδ���Ϊ��λ����
 * @param xCopyPosition ָ���Ƿ������Ƕ����е��������ݡ�
 *
 * @return ����pdPASS��ʾ�ɹ���errQUEUE_FULL��ʾ����������
 */
BaseType_t xQueueGenericSend(
    QueueHandle_t xQueue, 
    const void * const pvItemToQueue, 
    TickType_t xTicksToWait, 
    const BaseType_t xCopyPosition)
{
    BaseType_t xEntryTimeSet = pdFALSE, xYieldRequired; // ����Ƿ������˳�ʱʱ����Ƿ���Ҫ����
    TimeOut_t xTimeOut;                                 // ʱ�䳬ʱ�ṹ
    Queue_t * const pxQueue = ( Queue_t * ) xQueue;     // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT( pxQueue );

    // ���ԣ�ȷ����������ΪNULL�����Ƕ������СΪ0
    configASSERT( !( ( pvItemToQueue == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    // ���ԣ�ȷ���ڷǵ�Ԫ�ض����в��ᷢ������
    configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );

    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        // ���ԣ�ȷ������������ͣʱ���ȴ�ʱ��Ϊ0
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // ����ѭ����ֱ���ɹ��������ݻ�ʱ
    for( ;; )
    {
        taskENTER_CRITICAL(); // �����ٽ���
        {
            // �������Ƿ��пռ�����Ƿ�������
            if( ( pxQueue->uxMessagesWaiting < pxQueue->uxLength ) || ( xCopyPosition == queueOVERWRITE ) )
            {
                // ��¼���Ͷ��е���Ϣ
                traceQUEUE_SEND( pxQueue );

                // �����ݸ��Ƶ�����
                xYieldRequired = prvCopyDataToQueue( pxQueue, pvItemToQueue, xCopyPosition );

                #if ( configUSE_QUEUE_SETS == 1 )
                {
                    // �����������ĳ�����м�
                    if( pxQueue->pxQueueSetContainer != NULL )
                    {
                        // ֪ͨ���м�����
                        if( prvNotifyQueueSetContainer( pxQueue, xCopyPosition ) != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION(); // ���ʹ����ռʽ���ȣ������
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    else
                    {
                        // ����еȴ����յ������б�
                        if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            // �Ƴ��ȴ����յ�����
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                            {
                                queueYIELD_IF_USING_PREEMPTION(); // ���ʹ����ռʽ���ȣ������
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                            }
                        }
                        else if( xYieldRequired != pdFALSE )
                        {
                            // �����Ҫ����
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    // ����еȴ����յ������б�
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // �Ƴ��ȴ����յ�����
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION(); // ���ʹ����ռʽ���ȣ������
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    else if( xYieldRequired != pdFALSE )
                    {
                        // �����Ҫ����
                        queueYIELD_IF_USING_PREEMPTION();
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                    }
                }
                #endif /* configUSE_QUEUE_SETS */

                taskEXIT_CRITICAL(); // �˳��ٽ���
                return pdPASS; // �ɹ���������
            }
            else
            {
                // ����ȴ�ʱ��Ϊ0
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL(); // �˳��ٽ���

                    // ��¼����ʧ�ܵ���Ϣ
                    traceQUEUE_SEND_FAILED( pxQueue );
                    return errQUEUE_FULL; // ��������
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    // ���ó�ʱʱ��
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                }
            }
        }
        taskEXIT_CRITICAL(); // �˳��ٽ���

        // ��ͣ��������
        vTaskSuspendAll();
        // ��������
        prvLockQueue( pxQueue );

        // ����Ƿ�ʱ
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // �����������
            if( prvIsQueueFull( pxQueue ) != pdFALSE )
            {
                // ��¼�������͵���Ϣ
                traceBLOCKING_ON_QUEUE_SEND( pxQueue );

                // ����ǰ������ӵ��ȴ����͵������б�
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                // ��������
                prvUnlockQueue( pxQueue );

                // �ָ���������
                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API(); // �������
                }
            }
            else
            {
                // ��������
                prvUnlockQueue( pxQueue );
                // �ָ���������
                ( void ) xTaskResumeAll();
            }
        }
        else
        {
            // ��������
            prvUnlockQueue( pxQueue );
            // �ָ���������
            ( void ) xTaskResumeAll();

            // ��¼����ʧ�ܵ���Ϣ
            traceQUEUE_SEND_FAILED( pxQueue );
            return errQUEUE_FULL; // ��������
        }
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ�����������ݵ����С�
 *
 * �˺������ڴ��жϷ����������з������ݡ��������������������û�пռ䣬�򷵻ش���
 * ����и������ȼ������񱻻��ѣ��򷵻�һ����־��ָʾ�Ƿ���Ҫ�������л���
 *
 * @param xQueue ���о����
 * @param pvItemToQueue Ҫ���͵������
 * @param pxHigherPriorityTaskWoken ָ��һ������������ָ�룬����ָʾ�Ƿ��и������ȼ������񱻻��ѡ�
 * @param xCopyPosition ���ݸ��Ƶ�λ�ã�queueOVERWRITE ������λ�ã���
 *
 * @return ���� pdPASS ��ʾ�ɹ����ͣ����� errQUEUE_FULL ��ʾ��������
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

    // ����ȷ�����о����Ч
    configASSERT(pxQueue);

    // ����ȷ�������Ϊ�ջ��߶��д�СΪ��
    configASSERT(!( (pvItemToQueue == NULL) && (pxQueue->uxItemSize != (UBaseType_t)0U) ));

    // ����ȷ������λ�ò��� queueOVERWRITE ���߶��г���Ϊ 1
    configASSERT(!( (xCopyPosition == queueOVERWRITE) && (pxQueue->uxLength != 1) ));

    // ȷ���жϷ��������õ����ȼ���Ч��
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ��ȡ�жϽ��ñ�־
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        if ((pxQueue->uxMessagesWaiting < pxQueue->uxLength) || (xCopyPosition == queueOVERWRITE))
        {
            const int8_t cTxLock = pxQueue->cTxLock;

            // ��¼�� ISR �������ݵ����еĲ���
            traceQUEUE_SEND_FROM_ISR(pxQueue);

            // �������ݵ�����
            (void)prvCopyDataToQueue(pxQueue, pvItemToQueue, xCopyPosition);

            // �������δ�����������¼��б�
            if (cTxLock == queueUNLOCKED)
            {
                #if (configUSE_QUEUE_SETS == 1)
                {
                    if (pxQueue->pxQueueSetContainer != NULL)
                    {
                        if (prvNotifyQueueSetContainer(pxQueue, xCopyPosition) != pdFALSE)
                        {
                            // �������ڶ��м���������м��ķ��͵��¸������ȼ����������������Ҫ�������л�
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
                                // �ȴ����յĸ����ȼ�������Ҫ�������л�
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
                            // �ȴ����յĸ����ȼ�������Ҫ�������л�
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
                // �������������Ա�������е�����֪���������ڼ������ݷ���
                pxQueue->cTxLock = (int8_t)(cTxLock + 1);
            }

            xReturn = pdPASS;
        }
        else
        {
            // ��¼�� ISR �������ݵ�����ʧ��
            traceQUEUE_SEND_FROM_ISR_FAILED(pxQueue);
            xReturn = errQUEUE_FULL;
        }
    }

    // �ָ��ж�״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // ���ط��ͽ��
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ�������й黹�ź����򻥳�����
 *
 * �˺������ڴ��жϷ�������й黹֮ǰ��ȡ���ź����򻥳���������黹���еȴ����յ����񣬿��ܻᵼ����Щ���񱻻��ѡ�
 *
 * @param xQueue �ź����򻥳����ľ����
 * @param pxHigherPriorityTaskWoken ָ��һ������������ָ�룬����ָʾ�Ƿ��и������ȼ������񱻻��ѡ�
 *
 * @return ���� pdPASS ��ʾ�ɹ��黹������ errQUEUE_FULL ��ʾ����������
 */
BaseType_t xQueueGiveFromISR(
    QueueHandle_t xQueue,
    BaseType_t * const pxHigherPriorityTaskWoken
)
{
    BaseType_t xReturn; // ����ֵ
    UBaseType_t uxSavedInterruptStatus; // �����ж�����״̬
    Queue_t * const pxQueue = (Queue_t *)xQueue; // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxQueue);

    // ���ԣ�ȷ���������СΪ0��ͨ�������ź�����
    configASSERT(pxQueue->uxItemSize == 0);

    // ���ԣ�ȷ����ʹ�û�����ʱû�г����ߣ�ͨ�������ź�����
    configASSERT(!( (pxQueue->uxQueueType == queueQUEUE_IS_MUTEX) && (pxQueue->pxMutexHolder != NULL) ));

    // ���ԣ�ȷ���ж����ȼ���Ч
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ���浱ǰ�ж�����״̬
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting; // ��ȡ�����еȴ�����Ϣ����

        // ������л�û�дﵽ��󳤶�
        if (uxMessagesWaiting < pxQueue->uxLength)
        {
            const int8_t cTxLock = pxQueue->cTxLock; // ��ȡ������״̬

            // ��¼���жϷ�������Ͷ��е���Ϣ
            traceQUEUE_SEND_FROM_ISR(pxQueue);

            // ���ӵȴ�����Ϣ����
            pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

            // ���������״̬Ϊδ����
            if (cTxLock == queueUNLOCKED)
            {
                #if (configUSE_QUEUE_SETS == 1)
                {
                    // �����������ĳ�����м�
                    if (pxQueue->pxQueueSetContainer != NULL)
                    {
                        // ֪ͨ���м�����
                        if (prvNotifyQueueSetContainer(pxQueue, queueSEND_TO_BACK) != pdFALSE)
                        {
                            // ����ṩ�˸��ߵ����ȼ������ѱ�־
                            if (pxHigherPriorityTaskWoken != NULL)
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE; // ����и������ȼ������񱻻���
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    else
                    {
                        // ����еȴ����յ������б�
                        if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                        {
                            // �Ƴ��ȴ����յ�����
                            if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                            {
                                // ����ṩ�˸��ߵ����ȼ������ѱ�־
                                if (pxHigherPriorityTaskWoken != NULL)
                                {
                                    *pxHigherPriorityTaskWoken = pdTRUE; // ����и������ȼ������񱻻���
                                }
                                else
                                {
                                    mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                                }
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                }
                #else /* configUSE_QUEUE_SETS */
                {
                    // ����еȴ����յ������б�
                    if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                    {
                        // �Ƴ��ȴ����յ�����
                        if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                        {
                            // ����ṩ�˸��ߵ����ȼ������ѱ�־
                            if (pxHigherPriorityTaskWoken != NULL)
                            {
                                *pxHigherPriorityTaskWoken = pdTRUE; // ����и������ȼ������񱻻���
                            }
                            else
                            {
                                mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                            }
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                    }
                }
                #endif /* configUSE_QUEUE_SETS */
            }
            else
            {
                // ���ӷ�����״̬
                pxQueue->cTxLock = (int8_t)(cTxLock + 1);
            }

            xReturn = pdPASS; // ���÷���ֵΪ�ɹ�
        }
        else
        {
            // ��¼���жϷ�������Ͷ���ʧ�ܵ���Ϣ
            traceQUEUE_SEND_FROM_ISR_FAILED(pxQueue);
            xReturn = errQUEUE_FULL; // ���÷���ֵΪ��������
        }
    }

    // �ָ��ж�����״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // ���ط��ͽ��
    return xReturn;
}
/*-----------------------------------------------------------*/

/*
 * @brief xQueueGenericReceive ���ڴӶ����н������ݡ�
 *
 * �ú������ڴ�ָ���Ķ����н������ݣ���֧�ֳ�ʱ�ͽ�Ԥ����peeking��ģʽ��
 *
 * @param xQueue ��Ҫ���н������ݵĶ��о����
 * @param pvBuffer ���յ����ݽ�����ڸû������С�
 * @param xTicksToWait �ȴ��������ݵ����ʱ�䣨�Եδ���Ϊ��λ����
 * @param xJustPeeking ���ΪpdTRUE�����Ԥ�������Ƴ����ݡ�
 *
 * @return ����pdPASS��ʾ�ɹ���errQUEUE_EMPTY��ʾ����Ϊ�ա�
 */
BaseType_t xQueueGenericReceive(
    QueueHandle_t xQueue, 
    void * const pvBuffer, 
    TickType_t xTicksToWait, 
    const BaseType_t xJustPeeking)
{
    BaseType_t xEntryTimeSet = pdFALSE; // ����Ƿ������˳�ʱʱ��
    TimeOut_t xTimeOut;                 // ���ڳ�ʱ����ʱ��ṹ
    int8_t *pcOriginalReadPosition;     // ԭʼ��ȡλ��
    Queue_t * const pxQueue = ( Queue_t * ) xQueue; // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT( pxQueue );

    // ���ԣ�ȷ����������ΪNULL�����Ƕ������СΪ0
    configASSERT( !( ( pvBuffer == NULL ) && ( pxQueue->uxItemSize != ( UBaseType_t ) 0U ) ) );

    #if ( ( INCLUDE_xTaskGetSchedulerState == 1 ) || ( configUSE_TIMERS == 1 ) )
    {
        // ���ԣ�ȷ������������ͣʱ���ȴ�ʱ��Ϊ0
        configASSERT( !( ( xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED ) && ( xTicksToWait != 0 ) ) );
    }
    #endif

    // ����ѭ����ֱ���ɹ����յ����ݻ�ʱ
    for( ;; )
    {
        taskENTER_CRITICAL(); // �����ٽ���
        {
            const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting; // ��ȡ�����еȴ�����Ϣ����

            // �������������Ϣ
            if( uxMessagesWaiting > ( UBaseType_t ) 0 )
            {
                // ��¼ԭʼ��ȡλ��
                pcOriginalReadPosition = pxQueue->u.pcReadFrom;

                // �Ӷ����и������ݵ�������
                prvCopyDataFromQueue( pxQueue, pvBuffer );

                // �������ֻ�鿴��peek��
                if( xJustPeeking == pdFALSE )
                {
                    // ��¼���ն��е���Ϣ
                    traceQUEUE_RECEIVE( pxQueue );

                    // ���ٵȴ�����Ϣ����
                    pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

                    #if ( configUSE_MUTEXES == 1 )
                    {
                        // ����ǻ����ź���
                        if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                        {
                            // ���»����ź���������
                            pxQueue->pxMutexHolder = ( int8_t * ) pvTaskIncrementMutexHeldCount(); /*lint !e961 Cast is not redundant as TaskHandle_t is a typedef. */
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    #endif

                    // ����еȴ����͵������б�
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                    {
                        // �Ƴ��ȴ����͵�����
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                        {
                            queueYIELD_IF_USING_PREEMPTION(); // ���ʹ����ռʽ���ȣ������
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                    }
                }
                else
                {
                    // ��¼�鿴���е���Ϣ
                    traceQUEUE_PEEK( pxQueue );

                    // �ָ�ԭʼ��ȡλ��
                    pxQueue->u.pcReadFrom = pcOriginalReadPosition;

                    // ����еȴ����յ������б�
                    if( listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        // �Ƴ��ȴ����յ�����
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            // ���ʹ����ռʽ���ȣ������
                            queueYIELD_IF_USING_PREEMPTION();
                        }
                        else
                        {
                            mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                        }
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                    }
                }

                taskEXIT_CRITICAL(); // �˳��ٽ���
                return pdPASS; // �ɹ���������
            }
            else
            {
                // ����ȴ�ʱ��Ϊ0
                if( xTicksToWait == ( TickType_t ) 0 )
                {
                    taskEXIT_CRITICAL(); // �˳��ٽ���
                    traceQUEUE_RECEIVE_FAILED( pxQueue ); // ��¼����ʧ�ܵ���Ϣ
                    return errQUEUE_EMPTY; // ����Ϊ��
                }
                else if( xEntryTimeSet == pdFALSE )
                {
                    // ���ó�ʱʱ��
                    vTaskSetTimeOutState( &xTimeOut );
                    xEntryTimeSet = pdTRUE;
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                }
            }
        }
        taskEXIT_CRITICAL(); // �˳��ٽ���

        // ��ͣ��������
        vTaskSuspendAll();
        // ��������
        prvLockQueue( pxQueue );

        // ����Ƿ�ʱ
        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
        {
            // �������Ϊ��
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                traceBLOCKING_ON_QUEUE_RECEIVE( pxQueue ); // ��¼�������յ���Ϣ

                #if ( configUSE_MUTEXES == 1 )
                {
                    // ����ǻ����ź���
                    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
                    {
                        taskENTER_CRITICAL(); // �����ٽ���
                        {
                            // ���ȼ��̳�
                            vTaskPriorityInherit( ( void * ) pxQueue->pxMutexHolder );
                        }
                        taskEXIT_CRITICAL(); // �˳��ٽ���
                    }
                    else
                    {
                        mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                    }
                }
                #endif

                // ����ǰ������ӵ��ȴ����յ������б�
                vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                // ��������
                prvUnlockQueue( pxQueue );
                // �ָ���������
                if( xTaskResumeAll() == pdFALSE )
                {
                    portYIELD_WITHIN_API(); // �������
                }
                else
                {
                    mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
                }
            }
            else
            {
                // ��������
                prvUnlockQueue( pxQueue );
                // �ָ���������
                ( void ) xTaskResumeAll();
            }
        }
        else
        {
            // ��������
            prvUnlockQueue( pxQueue );
            // �ָ���������
            ( void ) xTaskResumeAll();

            // ���������ȻΪ��
            if( prvIsQueueEmpty( pxQueue ) != pdFALSE )
            {
                traceQUEUE_RECEIVE_FAILED( pxQueue ); // ��¼����ʧ�ܵ���Ϣ
                return errQUEUE_EMPTY; // ����Ϊ��
            }
            else
            {
                mtCOVERAGE_TEST_MARKER(); // ���ǲ��Ա��
            }
        }
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ��������ն����е����ݡ�
 *
 * �˺������ڴ��жϷ��������ն����е����ݡ�����������п������ݣ���Ὣ���ݸ��Ƶ��ṩ�Ļ�������
 * �����ܻ������ڵȴ��ö��еĸ������ȼ�����
 *
 * @param xQueue ���о����
 * @param pvBuffer �������ݵĻ�������
 * @param pxHigherPriorityTaskWoken ָ��һ������������ָ�룬����ָʾ�Ƿ��и������ȼ������񱻻��ѡ�
 *
 * @return ���� pdPASS ��ʾ�ɹ����գ����� pdFAIL ��ʾ����ʧ�ܡ�
 */
BaseType_t xQueueReceiveFromISR(
    QueueHandle_t xQueue,
    void * const pvBuffer,
    BaseType_t * const pxHigherPriorityTaskWoken
)
{
    BaseType_t xReturn;                         // ����ֵ
    UBaseType_t uxSavedInterruptStatus;         // �����ж�����״̬
    Queue_t * const pxQueue = (Queue_t *)xQueue; // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxQueue);

    // ���ԣ�ȷ����������ΪNULL�����Ƕ������СΪ0
    configASSERT(!( (pvBuffer == NULL) && (pxQueue->uxItemSize != (UBaseType_t)0U) ));

    // ���ԣ�ȷ���ж����ȼ���Ч
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ���浱ǰ�ж�����״̬
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        const UBaseType_t uxMessagesWaiting = pxQueue->uxMessagesWaiting; // ��ȡ�����еȴ�����Ϣ����

        // �������������Ϣ
        if (uxMessagesWaiting > (UBaseType_t)0)
        {
            const int8_t cRxLock = pxQueue->cRxLock; // ��ȡ������״̬

            // ��¼���жϷ��������ն��е���Ϣ
            traceQUEUE_RECEIVE_FROM_ISR(pxQueue);

            // �Ӷ����и������ݵ�������
            prvCopyDataFromQueue(pxQueue, pvBuffer);

            // ���ٵȴ�����Ϣ����
            pxQueue->uxMessagesWaiting = uxMessagesWaiting - 1;

            // ���������״̬Ϊδ����
            if (cRxLock == queueUNLOCKED)
            {
                // ����еȴ����͵������б�
                if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
                {
                    // �Ƴ��ȴ����͵�����
                    if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                    {
                        // ����ṩ�˸��ߵ����ȼ������ѱ�־
                        if (pxHigherPriorityTaskWoken != NULL)
                        {
                            // ����и������ȼ������񱻻���
                            *pxHigherPriorityTaskWoken = pdTRUE;
                        }
                        else
                        {
                            // ���ǲ��Ա��
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                    else
                    {
                        // ���ǲ��Ա��
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ���ӽ�����״̬
                pxQueue->cRxLock = (int8_t)(cRxLock + 1);
            }

            // ���÷���ֵΪ�ɹ�
            xReturn = pdPASS;
        }
        else
        {
            // ���÷���ֵΪʧ�ܣ�����¼���жϷ��������ն���ʧ�ܵ���Ϣ
            xReturn = pdFAIL;
            traceQUEUE_RECEIVE_FROM_ISR_FAILED(pxQueue);
        }
    }

    // �ָ��ж�����״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // ���ؽ��ս��
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ��������Ԥ�������е����ݶ����Ƴ�����
 *
 * �˺������ڴ��жϷ������Ԥ�������е����ݡ�����������п��õ����ݣ���Ὣ���ݸ��Ƶ��ṩ�Ļ�������
 * ������ı���е�״̬����������Ȼ�����ڶ����С�
 *
 * @param xQueue ���о����
 * @param pvBuffer ���Ԥ�����ݵĻ�������
 *
 * @return ���� pdPASS ��ʾԤ���ɹ������� pdFAIL ��ʾԤ��ʧ�ܡ�
 */
BaseType_t xQueuePeekFromISR(
    QueueHandle_t xQueue,
    void * const pvBuffer
)
{
    BaseType_t xReturn;                         // ����ֵ
    UBaseType_t uxSavedInterruptStatus;         // �����ж�����״̬
    int8_t *pcOriginalReadPosition;             // ԭʼ��ȡλ��
    Queue_t * const pxQueue = (Queue_t *)xQueue; // ���о��ת��Ϊ���нṹָ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxQueue);

    // ���ԣ�ȷ����������ΪNULL�����Ƕ������СΪ0
    configASSERT(!( (pvBuffer == NULL) && (pxQueue->uxItemSize != (UBaseType_t)0U) ));

    // ���ԣ�ȷ���������С��Ϊ0����Ϊ����Ԥ���ź���
    configASSERT(pxQueue->uxItemSize != 0); 

    // ���ԣ�ȷ���ж����ȼ���Ч
    portASSERT_IF_INTERRUPT_PRIORITY_INVALID();

    // ���浱ǰ�ж�����״̬
    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();

    {
        // ���������Ƿ�������
        if (pxQueue->uxMessagesWaiting > (UBaseType_t)0)
        {
            // ��¼���жϷ������Ԥ�����е���Ϣ
            traceQUEUE_PEEK_FROM_ISR(pxQueue);

            // ��¼ԭʼ��ȡλ���Ա�֮��ָ�
            pcOriginalReadPosition = pxQueue->u.pcReadFrom;

            // �Ӷ����и������ݵ�������
            prvCopyDataFromQueue(pxQueue, pvBuffer);

            // �ָ�ԭʼ��ȡλ��
            pxQueue->u.pcReadFrom = pcOriginalReadPosition;

            // ���÷���ֵΪ�ɹ�
            xReturn = pdPASS;
        }
        else
        {
            // ���÷���ֵΪʧ�ܣ�����¼���жϷ������Ԥ������ʧ�ܵ���Ϣ
            xReturn = pdFAIL;
            traceQUEUE_PEEK_FROM_ISR_FAILED(pxQueue);
        }
    }

    // �ָ��ж�����״̬
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    // ����Ԥ�����
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ��ȡָ�������е���Ϣ������
 *
 * �˺������ڻ�ȡָ�������е���Ϣ������Ϊ�˱�֤���ݵ�һ���ԣ��ڶ�ȡ����״̬ʱ��Ҫ�����ٽ�����
 *
 * @param xQueue ���о����
 *
 * @return ���ض����е���Ϣ������
 */
UBaseType_t uxQueueMessagesWaiting( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn; // ����ֵ�����ڴ洢��ǰ�����е���Ϣ����

    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // �����ٽ�������ֹ�������ʶ������ݽṹ
    taskENTER_CRITICAL();

    {
        // ��ȡ�����е�ǰ�ȴ�����Ϣ����
        uxReturn = ((Queue_t *)xQueue)->uxMessagesWaiting;
    }

    // �˳��ٽ�������������������жϼ���ִ��
    taskEXIT_CRITICAL();

    // ���ص�ǰ�����е���Ϣ����
    return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

/**
 * @brief ��ȡָ�������еĿ��ÿռ�������
 *
 * �˺������ڻ�ȡָ�������еĿ��ÿռ�������Ϊ�˱�֤���ݵ�һ���ԣ��ڼ�����п��ÿռ�ʱ��Ҫ�����ٽ�����
 *
 * @param xQueue ���о����
 *
 * @return ���ض����еĿ��ÿռ�������
 */
UBaseType_t uxQueueSpacesAvailable( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn; // ����ֵ�����ڴ洢��ǰ�����еĿ��ÿռ�����
    Queue_t *pxQueue;     // ���нṹָ��

    // �����о��ת��Ϊ���нṹָ��
    pxQueue = (Queue_t *)xQueue;

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxQueue);

    // �����ٽ�������ֹ�������ʶ������ݽṹ
    taskENTER_CRITICAL();

    {
        // ��������еĿ��ÿռ�����
        uxReturn = pxQueue->uxLength - pxQueue->uxMessagesWaiting;
    }

    // �˳��ٽ�������������������жϼ���ִ��
    taskEXIT_CRITICAL();

    // ���ص�ǰ�����еĿ��ÿռ�����
    return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

/**
 * @brief ��ȡָ���������жϷ�������еĵȴ���Ϣ������
 *
 * �˺����������жϷ�������л�ȡָ�������еĵȴ���Ϣ������
 *
 * @param xQueue ���о����
 *
 * @return ���ض����еĵȴ���Ϣ������
 */
UBaseType_t uxQueueMessagesWaitingFromISR( const QueueHandle_t xQueue )
{
    UBaseType_t uxReturn;

    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // ֱ�ӻ�ȡ�����еȴ�����Ϣ����
    uxReturn = ((Queue_t *)xQueue)->uxMessagesWaiting;

    return uxReturn;
} /*lint !e818 Pointer cannot be declared const as xQueue is a typedef not pointer. */
/*-----------------------------------------------------------*/

/**
 * @brief ɾ��һ�����С�
 *
 * �˺�������ɾ��һ�����С���������ѡ��Ĳ�ͬ�����п����Ƕ�̬�����Ҳ�����Ǿ�̬����ġ�
 *
 * @param xQueue ��Ҫɾ���Ķ��о����
 */
void vQueueDelete( QueueHandle_t xQueue )
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // ���ԣ�ȷ�����о����Ч
    configASSERT(pxQueue);

    // ��¼ɾ�����е���Ϣ
    traceQUEUE_DELETE(pxQueue);

    #if (configQUEUE_REGISTRY_SIZE > 0)
    {
        // ��������˶���ע����ܣ���ȡ��ע�����
        vQueueUnregisterQueue(pxQueue);
    }
    #endif

    #if (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 0)
    {
        // �����֧�ֶ�̬���䣬���ͷŶ����ڴ�
        vPortFree(pxQueue);
    }
    #elif (configSUPPORT_DYNAMIC_ALLOCATION == 1) && (configSUPPORT_STATIC_ALLOCATION == 1)
    {
        // ���ͬʱ֧�ֶ�̬�;�̬���䣬������䷽ʽ���ͷ��ڴ�
        if (pxQueue->ucStaticallyAllocated == (uint8_t)pdFALSE)
        {
            vPortFree(pxQueue);
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    #else
    {
        // ���ֻ֧�־�̬���䣬�����κβ���
        (void)pxQueue;
    }
    #endif /* configSUPPORT_DYNAMIC_ALLOCATION */
}
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief ��ȡ���еı�š�
 *
 * �˺������ڻ�ȡָ�����еı�š�ֻ�е�������׷����ʩʱ���˺����ſ��á�
 *
 * @param xQueue ���о����
 *
 * @return ���ض��еı�š�
 */
UBaseType_t uxQueueGetQueueNumber(QueueHandle_t xQueue)
{
    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // ��ȡ���еı��
    return ((Queue_t *)xQueue)->uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief ���ö��еı�š�
 *
 * �˺�����������ָ�����еı�š�ֻ�е�������׷����ʩʱ���˺����ſ��á�
 *
 * @param xQueue ���о����
 * @param uxQueueNumber ��Ҫ���õĶ��б�š�
 */
void vQueueSetQueueNumber(QueueHandle_t xQueue, UBaseType_t uxQueueNumber)
{
    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // ���ö��еı��
    ((Queue_t *)xQueue)->uxQueueNumber = uxQueueNumber;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

#if (configUSE_TRACE_FACILITY == 1)

/**
 * @brief ��ȡ���е����͡�
 *
 * �˺������ڻ�ȡָ�����е����͡�ֻ�е�������׷����ʩʱ���˺����ſ��á�
 *
 * @param xQueue ���о����
 *
 * @return ���ض��е����͡�
 */
uint8_t ucQueueGetQueueType(QueueHandle_t xQueue)
{
    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // ��ȡ���е�����
    return ((Queue_t *)xQueue)->ucQueueType;
}

#endif /* configUSE_TRACE_FACILITY */
/*-----------------------------------------------------------*/

/**
 * @brief �����ݸ��Ƶ������С�
 *
 * �˺������ڽ����ݸ��Ƶ������С��ú����������ʱ�Ѿ������ٽ�������˲���Ҫ����Ļ��Ᵽ����
 *
 * @param pxQueue ���нṹָ�롣
 * @param pvItemToQueue ��Ҫ���Ƶ����е����ݡ�
 * @param xPosition ���ݲ�����е�λ�ñ�־��
 *
 * @return ���� pdTRUE ��ʾ�ɹ���������������ʱ�������򷵻� pdFALSE��
 */
static BaseType_t prvCopyDataToQueue(Queue_t * const pxQueue, const void *pvItemToQueue, const BaseType_t xPosition)
{
    BaseType_t xReturn = pdFALSE; // Ĭ�Ϸ���ֵΪʧ��
    UBaseType_t uxMessagesWaiting; // ��ǰ�����е���Ϣ����

    // ��ȡ��ǰ�����е���Ϣ����
    uxMessagesWaiting = pxQueue->uxMessagesWaiting;

    if (pxQueue->uxItemSize == (UBaseType_t)0)
    {
        #if (configUSE_MUTEXES == 1)
        {
            if (pxQueue->uxQueueType == queueQUEUE_IS_MUTEX)
            {
                // ����ǻ��������У��ͷų��������ȼ��̳�
                xReturn = xTaskPriorityDisinherit((void *)pxQueue->pxMutexHolder);
                pxQueue->pxMutexHolder = NULL;
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        #endif /* configUSE_MUTEXES */
    }
    else if (xPosition == queueSEND_TO_BACK)
    {
        // �����ݸ��Ƶ�����β��
        (void)memcpy((void *)pxQueue->pcWriteTo, pvItemToQueue, (size_t)pxQueue->uxItemSize); /*lint !e961 !e418 MISRA exception as the casts are only redundant for some ports, plus previous logic ensures a null pointer can only be passed to memcpy() if the copy size is 0. */
        pxQueue->pcWriteTo += pxQueue->uxItemSize;
        if (pxQueue->pcWriteTo >= pxQueue->pcTail) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
        {
            // ���дָ�뵽��β���������õ�ͷ��
            pxQueue->pcWriteTo = pxQueue->pcHead;
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // �����ݸ��Ƶ�����ͷ��
        (void)memcpy((void *)pxQueue->u.pcReadFrom, pvItemToQueue, (size_t)pxQueue->uxItemSize); /*lint !e961 MISRA exception as the casts are only redundant for some ports. */
        pxQueue->u.pcReadFrom -= pxQueue->uxItemSize;
        if (pxQueue->u.pcReadFrom < pxQueue->pcHead) /*lint !e946 MISRA exception justified as comparison of pointers is the cleanest solution. */
        {
            // �����ָ��С��ͷ����������Ϊβ����ȥ���С
            pxQueue->u.pcReadFrom = (pxQueue->pcTail - pxQueue->uxItemSize);
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        if (xPosition == queueOVERWRITE)
        {
            if (uxMessagesWaiting > (UBaseType_t)0)
            {
                // �������������Ŀ�����ȼ�ȥһ�����е���Ŀ��
                --uxMessagesWaiting;
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // ���¶����е���Ϣ����
    pxQueue->uxMessagesWaiting = uxMessagesWaiting + 1;

    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief �Ӷ����и������ݵ�ָ����������
 *
 * �˺������ڴӶ����и������ݵ�ָ���Ļ��������ú����ٶ�����ʱ�Ѿ������ٽ�������˲���Ҫ����Ļ��Ᵽ����
 *
 * @param pxQueue ���нṹָ�롣
 * @param pvBuffer ��ŴӶ����ж�ȡ�����ݵĻ�������
 */
static void prvCopyDataFromQueue(Queue_t * const pxQueue, void * const pvBuffer)
{
    // ���������С�Ƿ����
    if (pxQueue->uxItemSize != (UBaseType_t)0)
    {
        // ���¶�ָ��
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;

        // �����ָ�뵽��򳬹�β����ַ��������Ϊͷ����ַ
        if (pxQueue->u.pcReadFrom >= pxQueue->pcTail) /*lint !e946 MISRA exception justified as use of the relational operator is the cleanest solutions. */
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        // �����ݴӶ����и��Ƶ�������
        (void)memcpy((void *)pvBuffer, (void *)pxQueue->u.pcReadFrom, (size_t)pxQueue->uxItemSize); /*lint !e961 !e418 MISRA exception as the casts are only redundant for some ports.  Also previous logic ensures a null pointer can only be passed to memcpy() when the count is 0. */
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief �������С�
 *
 * �˺������ڽ������С������б�����ʱ�������������ӻ��Ƴ���������¼��б�����¡�
 * ��������ʱ����Ҫ�����������ڼ���ӻ��Ƴ����������֪ͨ�ȴ�������
 *
 * ע�⣺�˺��������ڵ�������ͣ������µ��á�
 *
 * @param pxQueue ���нṹָ�롣
 */
static void prvUnlockQueue(Queue_t * const pxQueue)
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */

    /* �������������ڶ��б������ڼ������û��Ƴ���������������� */
    
    // �����ٽ���
    taskENTER_CRITICAL();
    {
        int8_t cTxLock = pxQueue->cTxLock;

        // ����Ƿ������ݱ���ӵ�������
        while (cTxLock > queueLOCKED_UNMODIFIED)
        {
            // �����ڶ��������ڼ䱻�������Ƿ����������ȴ����ݣ�
            #if (configUSE_QUEUE_SETS == 1)
            {
                if (pxQueue->pxQueueSetContainer != NULL)
                {
                    if (prvNotifyQueueSetContainer(pxQueue, queueSEND_TO_BACK) != pdFALSE)
                    {
                        // �����Ƕ��м���һ���֣���������м��������ݵ��¸������ȼ�����������������Ҫ�����������л���
                        vTaskMissedYield();
                    }
                    else
                    {
                        // ���ǲ��Ա��
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
                else
                {
                    // �ȴ��������ݵ����񽫱���ӵ�����������б��У���Ϊ��ʱ�������Դ�����ͣ״̬��
                    if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                    {
                        if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                        {
                            // �ȴ���������и��ߵ����ȼ�����˼�¼��Ҫ�������л���
                            vTaskMissedYield();
                        }
                        else
                        {
                            // ���ǲ��Ա��
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
                // �ȴ��������ݵ����񽫱���ӵ�����������б��У���Ϊ��ʱ�������Դ�����ͣ״̬��
                if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
                {
                    if (xTaskRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                    {
                        // �ȴ���������и��ߵ����ȼ�����˼�¼��Ҫ�������л���
                        vTaskMissedYield();
                    }
                    else
                    {
                        // ���ǲ��Ա��
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

        // ����������
        pxQueue->cTxLock = queueUNLOCKED;
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    // �Խ�������ͬ���Ĵ���
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
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }

                --cRxLock;
            }
            else
            {
                break;
            }
        }

        // ����������
        pxQueue->cRxLock = queueUNLOCKED;
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

/**
 * @brief �ж϶����Ƿ�Ϊ�ա�
 *
 * �˺��������ж�ָ�������Ƿ�Ϊ�ա�Ϊ�˱�֤���ݵ�һ���ԣ��ڼ�����״̬ʱ��Ҫ�����ٽ�����
 *
 * @param pxQueue ���нṹָ�롣
 *
 * @return ���� pdTRUE ��ʾ����Ϊ�գ����򷵻� pdFALSE��
 */
static BaseType_t prvIsQueueEmpty(const Queue_t *pxQueue)
{
    BaseType_t xReturn; // ����ֵ�����ڴ洢�����Ƿ�Ϊ�յĽ��

    // �����ٽ�������ֹ�������ʶ������ݽṹ
    taskENTER_CRITICAL();
    {
        // �������е���Ϣ�����Ƿ�Ϊ��
        if (pxQueue->uxMessagesWaiting == (UBaseType_t)0)
        {
            xReturn = pdTRUE; // ����Ϊ��
        }
        else
        {
            xReturn = pdFALSE; // ���в�Ϊ��
        }
    }
    // �˳��ٽ�������������������жϼ���ִ��
    taskEXIT_CRITICAL();

    // ���ض����Ƿ�Ϊ�յĽ��
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ���������ж϶����Ƿ�Ϊ�ա�
 *
 * �˺����������жϷ���������ж�ָ�������Ƿ�Ϊ�ա�
 *
 * @param xQueue ���о����
 *
 * @return ���� pdTRUE ��ʾ����Ϊ�գ����򷵻� pdFALSE��
 */
BaseType_t xQueueIsQueueEmptyFromISR(const QueueHandle_t xQueue)
{
    BaseType_t xReturn; // ����ֵ�����ڴ洢�����Ƿ�Ϊ�յĽ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // �������е���Ϣ�����Ƿ�Ϊ��
    if (((Queue_t *)xQueue)->uxMessagesWaiting == (UBaseType_t)0)
    {
        xReturn = pdTRUE; // ����Ϊ��
    }
    else
    {
        xReturn = pdFALSE; // ���в�Ϊ��
    }

    // ���ض����Ƿ�Ϊ�յĽ��
    return xReturn;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

/**
 * @brief �ж϶����Ƿ�������
 *
 * �˺��������ж�ָ�������Ƿ�������Ϊ�˱�֤���ݵ�һ���ԣ��ڼ�����״̬ʱ��Ҫ�����ٽ�����
 *
 * @param pxQueue ���нṹָ�롣
 *
 * @return ���� pdTRUE ��ʾ�������������򷵻� pdFALSE��
 */
static BaseType_t prvIsQueueFull(const Queue_t *pxQueue)
{
    BaseType_t xReturn; // ����ֵ�����ڴ洢�����Ƿ������Ľ��

    // �����ٽ�������ֹ�������ʶ������ݽṹ
    taskENTER_CRITICAL();
    {
        // �������е���Ϣ�����Ƿ���ڶ��е���󳤶�
        if (pxQueue->uxMessagesWaiting == pxQueue->uxLength)
        {
            xReturn = pdTRUE; // ��������
        }
        else
        {
            xReturn = pdFALSE; // ����δ��
        }
    }
    // �˳��ٽ�������������������жϼ���ִ��
    taskEXIT_CRITICAL();

    // ���ض����Ƿ������Ľ��
    return xReturn;
}
/*-----------------------------------------------------------*/

/**
 * @brief ���жϷ���������ж϶����Ƿ�������
 *
 * �˺����������жϷ���������ж�ָ�������Ƿ�������
 *
 * @param xQueue ���о����
 *
 * @return ���� pdTRUE ��ʾ�������������򷵻� pdFALSE��
 */
BaseType_t xQueueIsQueueFullFromISR(const QueueHandle_t xQueue)
{
    BaseType_t xReturn; // ����ֵ�����ڴ洢�����Ƿ������Ľ��

    // ���ԣ�ȷ�����о����Ч
    configASSERT(xQueue);

    // �������е���Ϣ�����Ƿ���ڶ��е���󳤶�
    if (((Queue_t *)xQueue)->uxMessagesWaiting == ((Queue_t *)xQueue)->uxLength)
    {
        xReturn = pdTRUE; // ��������
    }
    else
    {
        xReturn = pdFALSE; // ����δ��
    }

    // ���ض����Ƿ������Ľ��
    return xReturn;
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief ����з������ݡ�
 *
 * �˺���������ָ�����з������ݡ�����������������ݵȴ�ʱ��Ĳ�ͬ�������᷵�ش������������ǰЭ�̡�
 *
 * @param xQueue ���о����
 * @param pvItemToQueue Ҫ���͵�����ָ�롣
 * @param xTicksToWait �����ȴ���ʱ�䣨�Եδ�Ϊ��λ����
 *
 * @return ���� pdPASS ��ʾ�ɹ����ͣ�errQUEUE_FULL ��ʾ����������errQUEUE_BLOCKED ��ʾ��Ҫ������errQUEUE_YIELD ��ʾ��Ҫ���ȡ�
 */
BaseType_t xQueueCRSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // �����ٽ�������ֹ�ж��ڼ������Ƿ�����������֮���޸Ķ���
    portDISABLE_INTERRUPTS();
    {
        if (prvIsQueueFull(pxQueue) != pdFALSE)
        {
            // �������� - ������Ҫ��������ֱ�ӷ��أ�
            if (xTicksToWait > (TickType_t)0)
            {
                // ����������Э���е��õģ����ǲ���ֱ�����������Ƿ��ر�ʾ��Ҫ������״̬��
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
    // �˳��ٽ���
    portENABLE_INTERRUPTS();

    // �ٴν����ٽ�������ֹ�������ʶ������ݽṹ
    portDISABLE_INTERRUPTS();
    {
        if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
        {
            // �������пռ䣬�����ݸ��Ƶ�������
            prvCopyDataToQueue(pxQueue, pvItemToQueue, queueSEND_TO_BACK);
            xReturn = pdPASS;

            // �Ƿ���Э�����ڵȴ����ݣ�
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
            {
                // ����������£��������ٽ�����ֱ�ӽ�Э�̷�������б�
                // ��������������ж�����ͬ�ķ�ʽ����
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                {
                    // �ȴ���Э�����ȼ��ϸߣ���¼������Ҫ���ȡ�
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ��������
            xReturn = errQUEUE_FULL;
        }
    }
    // �˳��ٽ���
    portENABLE_INTERRUPTS();

    // ���ؽ��
    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief �Ӷ��н������ݡ�
 *
 * �˺������ڴ�ָ�����н������ݡ��������Ϊ�գ����ݵȴ�ʱ��Ĳ�ͬ�������᷵�ش������������ǰЭ�̡�
 *
 * @param xQueue ���о����
 * @param pvBuffer ���յ����ݽ�����ڸû������С�
 * @param xTicksToWait �����ȴ���ʱ�䣨�Եδ�Ϊ��λ����
 *
 * @return ���� pdPASS ��ʾ�ɹ����գ�pdFAIL ��ʾ����Ϊ�գ�errQUEUE_BLOCKED ��ʾ��Ҫ������errQUEUE_YIELD ��ʾ��Ҫ���ȡ�
 */
BaseType_t xQueueCRReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // �����ٽ�������ֹ�ж��ڼ������Ƿ�Ϊ��������֮���޸Ķ���
    portDISABLE_INTERRUPTS();
    {
        if (pxQueue->uxMessagesWaiting == (UBaseType_t)0)
        {
            // ������û����Ϣ����Ҫ��������ֱ�ӷ��أ�
            if (xTicksToWait > (TickType_t)0)
            {
                // ����������Э���е��õģ����ǲ���ֱ�����������Ƿ��ر�ʾ��Ҫ������״̬��
                vCoRoutineAddToDelayedList(xTicksToWait, &(pxQueue->xTasksWaitingToReceive));
                portENABLE_INTERRUPTS();
                return errQUEUE_BLOCKED;
            }
            else
            {
                portENABLE_INTERRUPTS();
                return pdFAIL; // Ӧ���� errQUEUE_EMPTY ������ errQUEUE_FULL
            }
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    // �˳��ٽ���
    portENABLE_INTERRUPTS();

    // �ٴν����ٽ�������ֹ�������ʶ������ݽṹ
    portDISABLE_INTERRUPTS();
    {
        if (pxQueue->uxMessagesWaiting > (UBaseType_t)0)
        {
            // �����������ݿ���
            pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
            if (pxQueue->u.pcReadFrom >= pxQueue->pcTail)
            {
                pxQueue->u.pcReadFrom = pxQueue->pcHead;
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
            --(pxQueue->uxMessagesWaiting);
            (void)memcpy((void *)pvBuffer, (void *)pxQueue->u.pcReadFrom, (unsigned)pxQueue->uxItemSize);

            xReturn = pdPASS;

            // �Ƿ���Э�����ڵȴ����пռ䣿
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
            {
                // ����������£��������ٽ�����ֱ�ӽ�Э�̷�������б�
                // ��������������ж�����ͬ�ķ�ʽ����
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                {
                    xReturn = errQUEUE_YIELD;
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            xReturn = pdFAIL;
        }
    }
    // �˳��ٽ���
    portENABLE_INTERRUPTS();

    // ���ؽ��
    return xReturn;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief ���жϷ������������з������ݡ�
 *
 * �˺����������жϷ����������ָ�����з������ݡ����������������ִ���κβ���ֱ�ӷ��ء�
 *
 * @param xQueue ���о����
 * @param pvItemToQueue Ҫ���͵�����ָ�롣
 * @param xCoRoutinePreviouslyWoken ���֮ǰ�Ѿ���Э�̱����ѣ�������Ϊ pdTRUE������Ϊ pdFALSE��
 *
 * @return ���� pdTRUE ������ε��û�����һ��Э�̣����򷵻ش���� xCoRoutinePreviouslyWoken ֵ��
 */
BaseType_t xQueueCRSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue, BaseType_t xCoRoutinePreviouslyWoken)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // ��������л��пռ�
    if (pxQueue->uxMessagesWaiting < pxQueue->uxLength)
    {
        // �����ݸ��Ƶ�����
        prvCopyDataToQueue(pxQueue, pvItemToQueue, queueSEND_TO_BACK);

        // �жϷ��������ֻ�ܻ���һ��Э�̣���˼���Ƿ�����Э�̱�����
        if (xCoRoutinePreviouslyWoken == pdFALSE)
        {
            // �����Э�����ڵȴ���������
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToReceive)) == pdFALSE)
            {
                // �Ƴ�Э��ʹ��׼������
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToReceive)) != pdFALSE)
                {
                    // ���� pdTRUE ��ʾ���ε��û�����һ��Э��
                    return pdTRUE;
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
    else
    {
        // ���������������κβ���
        mtCOVERAGE_TEST_MARKER();
    }

    // ���ش���� xCoRoutinePreviouslyWoken ֵ
    return xCoRoutinePreviouslyWoken;
}

#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configUSE_CO_ROUTINES == 1 )

/**
 * @brief ���жϷ�������дӶ��н������ݡ�
 *
 * �˺����������жϷ�������д�ָ�����н������ݡ��������Ϊ�գ���ִ���κβ���ֱ�ӷ��ء�
 *
 * @param xQueue ���о����
 * @param pvBuffer ���յ����ݽ�����ڸû������С�
 * @param pxCoRoutineWoken ָ�룬����ָʾ�Ƿ���Э�̱����ѡ����������������Ϊ pdTRUE������Ϊ pdFALSE��
 *
 * @return ���� pdPASS ����ɹ��������ݣ����򷵻� pdFAIL��
 */
BaseType_t xQueueCRReceiveFromISR(QueueHandle_t xQueue, void *pvBuffer, BaseType_t *pxCoRoutineWoken)
{
    BaseType_t xReturn;
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // ��������������ݿ���
    if (pxQueue->uxMessagesWaiting > (UBaseType_t)0)
    {
        // �Ӷ����ж�ȡ����
        pxQueue->u.pcReadFrom += pxQueue->uxItemSize;
        if (pxQueue->u.pcReadFrom >= pxQueue->pcTail)
        {
            pxQueue->u.pcReadFrom = pxQueue->pcHead;
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
        --(pxQueue->uxMessagesWaiting);
        (void)memcpy((void *)pvBuffer, (void *)pxQueue->u.pcReadFrom, (unsigned)pxQueue->uxItemSize);

        // ����Ƿ���Э�����ڵȴ����Ϳռ�
        if (*pxCoRoutineWoken == pdFALSE)
        {
            if (listLIST_IS_EMPTY(&(pxQueue->xTasksWaitingToSend)) == pdFALSE)
            {
                // �����Э�����ڵȴ����пռ�
                if (xCoRoutineRemoveFromEventList(&(pxQueue->xTasksWaitingToSend)) != pdFALSE)
                {
                    // ���ñ�־��ʾ��Э�̱�����
                    *pxCoRoutineWoken = pdTRUE;
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }

        // ���� pdPASS ��ʾ�ɹ���������
        xReturn = pdPASS;
    }
    else
    {
        // ����Ϊ�գ����� pdFAIL
        xReturn = pdFAIL;
    }

    // ���ؽ��ս��
    return xReturn;
}
#endif /* configUSE_CO_ROUTINES */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

/**
 * @brief ��������ӵ�ע���
 *
 * �˺������ڽ�ָ���Ķ��м���������ӵ�����ע����С�ע������ڸ���ϵͳ�����ж��е�ʹ�������
 *
 * @param xQueue ���о����
 * @param pcQueueName ���е����ơ�
 */
void vQueueAddToRegistry(QueueHandle_t xQueue, const char *pcQueueName) /*lint !e971 Unqualified char types are allowed for strings and single characters only.*/
{
    UBaseType_t ux;

    // ����ע����еĿ���λ�á�NULL ���Ʊ�ʾ���в�λ��
    for (ux = (UBaseType_t)0U; ux < (UBaseType_t)configQUEUE_REGISTRY_SIZE; ux++)
    {
        if (xQueueRegistry[ux].pcQueueName == NULL)
        {
            // �洢���е���Ϣ
            xQueueRegistry[ux].pcQueueName = pcQueueName;
            xQueueRegistry[ux].xHandle = xQueue;

            // ��ѡ����¼����ע����Ϣ
            traceQUEUE_REGISTRY_ADD(xQueue, pcQueueName);
            
            break;
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
}
#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

/**
 * @brief ��ȡ���е����ơ�
 *
 * �˺������ڴӶ���ע����в��Ҹ������о����Ӧ�����ơ�
 *
 * @param xQueue ���о����
 *
 * @return ���ض��е����ƣ����δ�ҵ��򷵻� NULL��
 */
const char *pcQueueGetName(QueueHandle_t xQueue) /*lint !e971 Unqualified char types are allowed for strings and single characters only.*/
{
    UBaseType_t ux;
    const char *pcReturn = NULL; /*lint !e971 Unqualified char types are allowed for strings and single characters only. */

    // ע�⣺�˴�û���κα�����ʩ����ֹ��������������ע���ʱ��ӻ�ɾ����Ŀ��
    for (ux = (UBaseType_t)0U; ux < (UBaseType_t)configQUEUE_REGISTRY_SIZE; ux++)
    {
        if (xQueueRegistry[ux].xHandle == xQueue)
        {
            // �ҵ��˶�Ӧ�Ķ�������
            pcReturn = xQueueRegistry[ux].pcQueueName;
            break;
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }

    // ���ض�������
    return pcReturn;
}

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configQUEUE_REGISTRY_SIZE > 0 )

/**
 * @brief �Ӷ���ע�����ע�����С�
 *
 * �˺������ڴӶ���ע������Ƴ�ָ�����е���Ϣ��
 *
 * @param xQueue ��Ҫע���Ķ��о����
 */
void vQueueUnregisterQueue(QueueHandle_t xQueue)
{
    UBaseType_t ux;

    // ���Ҷ��о���Ƿ���ע����д��ڡ�
    for (ux = (UBaseType_t)0U; ux < (UBaseType_t)configQUEUE_REGISTRY_SIZE; ux++)
    {
        if (xQueueRegistry[ux].xHandle == xQueue)
        {
            // ����������Ϊ NULL����ʾ�ò�λ�ٴο��á�
            xQueueRegistry[ux].pcQueueName = NULL;

            // ���������Ϊ NULL����ȷ����ͬ�Ķ��о�����������ע��������Ρ�
            xQueueRegistry[ux].xHandle = (QueueHandle_t)NULL;
            
            break;
        }
        else
        {
            // ���ǲ��Ա��
            mtCOVERAGE_TEST_MARKER();
        }
    }
} /*lint !e818 xQueue could not be pointer to const because it is a typedef. */

#endif /* configQUEUE_REGISTRY_SIZE */
/*-----------------------------------------------------------*/

#if ( configUSE_TIMERS == 1 )

/**
 * @brief ʹ����ȴ������е���Ϣ�������ƣ���
 *
 * �˺������ǹ���API��һ���֣�רΪ�ں˴�����ơ�����������ĵ���Ҫ�󣬲���Ӧ������������������µ��ã�
 * ���������ٽ����е��á������Ե�����ֻ����һ����Ŀ���б��ϵ��� vListInsert()������б����ٶȺܿ죬
 * ������С��ʹ�á�
 *
 * @param xQueue ��Ҫ�ȴ���Ϣ�Ķ��о����
 * @param xTicksToWait �����ȴ���ʱ�䣨�Եδ�Ϊ��λ����
 * @param xWaitIndefinitely �Ƿ������ڵȴ���
 */
void vQueueWaitForMessageRestricted(QueueHandle_t xQueue, TickType_t xTicksToWait, const BaseType_t xWaitIndefinitely)
{
    Queue_t * const pxQueue = (Queue_t *)xQueue;

    // �������У���ֹ�������ʶ������ݽṹ
    prvLockQueue(pxQueue);

    // ֻ���ڶ�����û����Ϣʱ����Щ���顣
    if (pxQueue->uxMessagesWaiting == (UBaseType_t)0U)
    {
        // ������û����Ϣ������ָ����ʱ����������
        vTaskPlaceOnEventListRestricted(&(pxQueue->xTasksWaitingToReceive), xTicksToWait, xWaitIndefinitely);
    }
    else
    {
        // ���ǲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }

    // ��������
    prvUnlockQueue(pxQueue);
}

#endif /* configUSE_TIMERS */
/*-----------------------------------------------------------*/

#if( ( configUSE_QUEUE_SETS == 1 ) && ( configSUPPORT_DYNAMIC_ALLOCATION == 1 ) )

/**
 * @brief �������м���
 *
 * �˺������ڴ���һ���µĶ��м������м����԰���������У�����ʵ�������ź�����Ĺ��ܡ�
 *
 * @param uxEventQueueLength ���м����¼����еĳ��ȣ������м��������ɵ�����¼�������
 *
 * @return ���ش����Ķ��м��ľ�����������ʧ���򷵻� NULL��
 */
QueueSetHandle_t xQueueCreateSet(const UBaseType_t uxEventQueueLength)
{
    QueueSetHandle_t pxQueue;

    // ʹ��ͨ�ö��д��������������м�
    pxQueue = xQueueGenericCreate(uxEventQueueLength, sizeof(Queue_t *), queueQUEUE_TYPE_SET);

    // ���ش����Ķ��м����
    return pxQueue;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief �����л��ź�����ӵ����м���
 *
 * �˺������ڽ�һ�����л��ź�����ӵ�ָ���Ķ��м��С����м��������ڹ���һ����л��ź�����
 *
 * @param xQueueOrSemaphore ��Ҫ��ӵ����м��Ķ��л��ź����ľ����
 * @param xQueueSet Ŀ����м��ľ����
 *
 * @return ���� pdPASS �����ӳɹ������򷵻� pdFAIL��
 */
BaseType_t xQueueAddToSet(QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet)
{
    BaseType_t xReturn;

    // �����ٽ�������ֹ�������ʶ������ݽṹ
    taskENTER_CRITICAL();
    {
        // �����л��ź����Ƿ��Ѿ�������һ�����м�
        if (((Queue_t *)xQueueOrSemaphore)->pxQueueSetContainer != NULL)
        {
            // ���ܽ�ͬһ�����л��ź�����ӵ�������м���
            xReturn = pdFAIL;
        }
        else if (((Queue_t *)xQueueOrSemaphore)->uxMessagesWaiting != (UBaseType_t)0)
        {
            // ������л��ź������Ѿ�����Ŀ�����ܽ�����ӵ����м���
            xReturn = pdFAIL;
        }
        else
        {
            // �����л��ź�����ӵ�ָ���Ķ��м���
            ((Queue_t *)xQueueOrSemaphore)->pxQueueSetContainer = xQueueSet;
            xReturn = pdPASS;
        }
    }
    // �˳��ٽ���
    taskEXIT_CRITICAL();

    // ���ز������
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

	/**
 * @brief �Ӷ��м����Ƴ����л��ź�����
 *
 * �˺������ڽ�һ�����л��ź�����ָ���Ķ��м����Ƴ������м��������ڹ���һ����л��ź�����
 *
 * @param xQueueOrSemaphore ��Ҫ�Ӷ��м����Ƴ��Ķ��л��ź����ľ����
 * @param xQueueSet ��Ҫ�Ƴ����л��ź����Ķ��м������
 *
 * @return ���� pdPASS ����Ƴ��ɹ������򷵻� pdFAIL��
 */
BaseType_t xQueueRemoveFromSet(QueueSetMemberHandle_t xQueueOrSemaphore, QueueSetHandle_t xQueueSet)
{
    BaseType_t xReturn;
    Queue_t * const pxQueueOrSemaphore = (Queue_t *)xQueueOrSemaphore;

    // �����л��ź����Ƿ�����ָ���Ķ��м�
    if (pxQueueOrSemaphore->pxQueueSetContainer != xQueueSet)
    {
        // ������л��ź���������ָ���Ķ��м�
        xReturn = pdFAIL;
    }
    else if (pxQueueOrSemaphore->uxMessagesWaiting != (UBaseType_t)0)
    {
        // ������л��ź����л�����Ϣ���Ƴ���Σ�յģ���Ϊ���м���Ȼ����ж��еĴ������¼�
        xReturn = pdFAIL;
    }
    else
    {
        // �����ٽ�������ֹ�������ʶ������ݽṹ
        taskENTER_CRITICAL();
        {
            // �����л��ź����Ӷ��м����Ƴ�
            pxQueueOrSemaphore->pxQueueSetContainer = NULL;
        }
        // �˳��ٽ���
        taskEXIT_CRITICAL();
        xReturn = pdPASS;
    }

    // ���ز������
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief �Ӷ��м���ѡ��һ���ɽ��յĶ��л��ź�����
 *
 * �˺������ڴ�ָ���Ķ��м���ѡ��һ�����Խ������ݵĶ��л��ź����������ָ��ʱ����û�ж��л��ź�����Ϊ�ɽ���״̬���򷵻� NULL��
 *
 * @param xQueueSet ���м��ľ����
 * @param xTicksToWait �ȴ�ʱ�䣨�Եδ�Ϊ��λ�����������Ϊ `portMAX_DELAY` ���ʾ���޵ȴ���
 *
 * @return ���ؿ��Խ������ݵĶ��л��ź����ľ�������û���ҵ��򷵻� NULL��
 */
QueueSetMemberHandle_t xQueueSelectFromSet(QueueSetHandle_t xQueueSet, TickType_t const xTicksToWait)
{
    QueueSetMemberHandle_t xReturn = NULL;

    // ʹ��ͨ�ö��н��պ������ԴӶ��м���ѡ��һ���ɽ��յĶ��л��ź���
    (void)xQueueGenericReceive((QueueHandle_t)xQueueSet, &xReturn, xTicksToWait, pdFALSE); /*lint !e961 Casting from one typedef to another is not redundant.*/

    // ����ѡ�еĶ��л��ź������
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief �Ӷ��м���ѡ��һ���ɽ��յĶ��л��ź������жϷ�����򣩡�
 *
 * �˺������ڴ�ָ���Ķ��м���ѡ��һ�����Խ������ݵĶ��л��ź��������Ҵ˺���Ӧ�����жϷ�������е��á�
 * ���û���ҵ����Խ������ݵĶ��л��ź������򷵻� NULL��
 *
 * @param xQueueSet ���м��ľ����
 *
 * @return ���ؿ��Խ������ݵĶ��л��ź����ľ�������û���ҵ��򷵻� NULL��
 */
QueueSetMemberHandle_t xQueueSelectFromSetFromISR(QueueSetHandle_t xQueueSet)
{
    QueueSetMemberHandle_t xReturn = NULL;

    // ʹ���жϰ�ȫ�Ķ��н��պ������ԴӶ��м���ѡ��һ���ɽ��յĶ��л��ź���
    (void)xQueueReceiveFromISR((QueueHandle_t)xQueueSet, &xReturn, NULL); /*lint !e961 Casting from one typedef to another is not redundant.*/

    // ����ѡ�еĶ��л��ź������
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */
/*-----------------------------------------------------------*/

#if ( configUSE_QUEUE_SETS == 1 )

/**
 * @brief ֪ͨ���м�������
 *
 * �˺�������������м���������ʱ���¶��м���״̬����֪ͨ�ȴ��������ݵ�����
 * �ú����������ٽ����е��á�
 *
 * @param pxQueue �������ݵĶ��С�
 * @param xCopyPosition ���ݸ��Ƶ�λ�ñ�־��
 *
 * @return ���� pdTRUE ����и������ȼ�������׼�������У����򷵻� pdFALSE��
 */
static BaseType_t prvNotifyQueueSetContainer(const Queue_t * const pxQueue, const BaseType_t xCopyPosition)
{
    Queue_t *pxQueueSetContainer = pxQueue->pxQueueSetContainer;
    BaseType_t xReturn = pdFALSE;

    // ����ȷ�����м�������Ϊ�ղ��Ҷ��м��е���Ϣ����С�ڶ��м��ĳ���
    configASSERT(pxQueueSetContainer);
    configASSERT(pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength);

    // �����м��е���Ϣ�����Ƿ�С�ڶ��м��ĳ���
    if (pxQueueSetContainer->uxMessagesWaiting < pxQueueSetContainer->uxLength)
    {
        const int8_t cTxLock = pxQueueSetContainer->cTxLock;

        // ��¼���з�����Ϣ�����ڵ��Ի���٣�
        traceQUEUE_SEND(pxQueueSetContainer);

        // �������ݵ����м�������Ϊ�������ݵĶ��еľ��
        xReturn = prvCopyDataToQueue(pxQueueSetContainer, &pxQueue, xCopyPosition);

        // �����м��ķ�����״̬
        if (cTxLock == queueUNLOCKED)
        {
            // ����Ƿ����������ڵȴ���������
            if (listLIST_IS_EMPTY(&(pxQueueSetContainer->xTasksWaitingToReceive)) == pdFALSE)
            {
                // �Ƴ��ȴ��������ݵ�����
                if (xTaskRemoveFromEventList(&(pxQueueSetContainer->xTasksWaitingToReceive)) != pdFALSE)
                {
                    // �ȴ���������и��ߵ����ȼ�
                    xReturn = pdTRUE;
                }
                else
                {
                    // ���ǲ��Ա��
                    mtCOVERAGE_TEST_MARKER();
                }
            }
            else
            {
                // ���ǲ��Ա��
                mtCOVERAGE_TEST_MARKER();
            }
        }
        else
        {
            // ���·�����״̬
            pxQueueSetContainer->cTxLock = (int8_t)(cTxLock + 1);
        }
    }
    else
    {
        // ���ǲ��Ա��
        mtCOVERAGE_TEST_MARKER();
    }

    // ���ز������
    return xReturn;
}

#endif /* configUSE_QUEUE_SETS */












