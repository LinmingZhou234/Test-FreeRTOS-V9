#include "FreeRTOS.h"
#include "task.h"
#include "croutine.h"

/* ���δʹ��Э�̣����Ƴ������ļ��� */
#if( configUSE_CO_ROUTINES != 0 )


/*
 * һЩ֧���ں˵ĵ�����Ҫ�󱻲鿴��������ȫ�ֵģ��������ļ�������ġ�
 */
#ifdef portREMOVE_STATIC_QUALIFIER
    #define static
#endif



/* ����������Э�̵��б�. -------------------- */
static List_t pxReadyCoRoutineLists[ configMAX_CO_ROUTINE_PRIORITIES ];	/*< ���ȼ�����Э���б�. */
static List_t xDelayedCoRoutineList1;									/*< �ӳ�Э���б�. */
static List_t xDelayedCoRoutineList2;									/*< �ӳ�Э���б�ʹ�������б� - һ�����ڳ�����ǰ�δ�������ӳ�Э��. */
static List_t * pxDelayedCoRoutineList;									/*< ָ��ǰ����ʹ�õ��ӳ�Э���б�. */
static List_t * pxOverflowDelayedCoRoutineList;							/*< ָ��ǰ����ʹ�õ��ӳ�Э���б����ڱ�����Щ������ǰ�δ������Э��. */
static List_t xPendingReadyCoRoutineList;								/*< �������ⲿ�¼���׼���õ�Э��. ��ЩЭ�̲���ֱ�Ӽ�������б���Ϊ�����б��ܱ��жϷ���. */


/* �����ļ�˽�б���. ----------------------------------------------- */
CRCB_t * pxCurrentCoRoutine = NULL;              /*< ָ��ǰ���е�Э��. */
static UBaseType_t uxTopCoRoutineReadyPriority = 0;  /*< ��ǰ����״̬��������ȼ�Э��. */
static TickType_t xCoRoutineTickCount = 0;       /*< Э�̵δ����. */
static TickType_t xLastTickCount = 0;            /*< ��һ�εδ����. */
static TickType_t xPassedTicks = 0;              /*< �Ѿ����ĵδ���. */


/* Э�̴���ʱ�ĳ�ʼ״̬. */
#define corINITIAL_STATE ( 0 )


/*
 * ���� pxCRCB ��ʾ��Э�̷�����Ӧ���ȼ��ľ���������
 * ���������뵽�б��ĩβ��
 *
 * �˺����Э�̾����б���˲�Ӧ�� ISR���жϷ������̣���ʹ�á�
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
 * ���ڳ�ʼ��������ʹ�õ������б��ʵ�ó��� 
 * �⽫�ڵ�һ��Э�̴���ʱ�Զ����á�
 */
static void prvInitialiseCoRoutineLists( void );


/*
 * ���ж�׼���õ�Э�̲���ֱ�ӷ�������б���Ϊû�л��Ᵽ������
 * ��ˣ����Ǳ������ڴ������б��У��Ա��������ͨ��Э�̵������ƶ��������б�
 */
static void prvCheckPendingReadyList( void );


/*
 * �꺯�������ڼ�鵱ǰ�ӳٵ�Э���б��Բ鿴�Ƿ�����Ҫ���ѵ�Э�̡�
 *
 * Э�̸����份��ʱ��洢�ڶ����� - ����ζ��һ���ҵ�һ���䶨ʱ����δ���ڵ�Э�̣�
 * ���Ǿ������һ������б��е������
 */
static void prvCheckDelayedList( void );


/*-----------------------------------------------------------*/

/**
 * @brief ����һ���µ�Э�̲���ʼ������ƿ顣
 *
 * �˺��������ڴ��Դ洢Э�̿��ƿ飨CRCB���������ݴ���Ĳ�������Э�̵� 
 * ��ʼ״̬�����ȼ���ִ�к�����Э�̽���ӵ��ʵ��ľ����б�׼�����е��ȡ�
 *
 * @param pxCoRoutineCode ָ��Э��ִ�к�����ָ�롣
 * @param uxPriority Э�̵����ȼ�����ΧΪ 0 �� configMAX_CO_ROUTINE_PRIORITIES-1��
 * @param uxIndex �������ֶ��ʹ����ͬЭ�̹��ܵ�Э�̵�����ֵ��
 *
 * @return ����ɹ�����Э�̣��򷵻� pdPASS������ڴ����ʧ�ܣ��򷵻� 
 * errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY��
 */
BaseType_t xCoRoutineCreate( crCOROUTINE_CODE pxCoRoutineCode, UBaseType_t uxPriority, UBaseType_t uxIndex )
{
    BaseType_t xReturn;
    CRCB_t *pxCoRoutine;

    /* ����洢Э�̿��ƿ���ڴ�. */
    pxCoRoutine = ( CRCB_t * ) pvPortMalloc( sizeof( CRCB_t ) );
    if( pxCoRoutine )
    {
        /* ��� pxCurrentCoRoutine Ϊ NULL����ô���ǵ�һ��Э�̵Ĵ�����
        ��Ҫ��ʼ��Э�����ݽṹ. */
        if( pxCurrentCoRoutine == NULL )
        {
            pxCurrentCoRoutine = pxCoRoutine;
            prvInitialiseCoRoutineLists(); // ��ʼ��Э���б�
        }

        /* ������ȼ��Ƿ������Ʒ�Χ��. */
        if( uxPriority >= configMAX_CO_ROUTINE_PRIORITIES )
        {
            uxPriority = configMAX_CO_ROUTINE_PRIORITIES - 1; // �������ȼ������ֵ
        }

        /* ���ݺ����������Э�̿��ƿ�. */
        pxCoRoutine->uxState = corINITIAL_STATE; // ���ó�ʼ״̬
        pxCoRoutine->uxPriority = uxPriority; // �������ȼ�
        pxCoRoutine->uxIndex = uxIndex; // ��������
        pxCoRoutine->pxCoRoutineFunction = pxCoRoutineCode; // ����Э�̺���

        /* ��ʼ��Э�̿��ƿ����������. */
        vListInitialiseItem( &( pxCoRoutine->xGenericListItem ) ); // ��ʼ��ͨ���б���
        vListInitialiseItem( &( pxCoRoutine->xEventListItem ) ); // ��ʼ���¼��б���

        /* ��Э�̿��ƿ�����Ϊ ListItem_t ������������.
        �������Դ��б��е�ͨ����ص������� CRCB. */
        listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xGenericListItem ), pxCoRoutine );
        listSET_LIST_ITEM_OWNER( &( pxCoRoutine->xEventListItem ), pxCoRoutine );

        /* �¼��б�ʼ�հ������ȼ�˳������. */
        listSET_LIST_ITEM_VALUE( &( pxCoRoutine->xEventListItem ), ( ( TickType_t ) configMAX_CO_ROUTINE_PRIORITIES - ( TickType_t ) uxPriority ) );

        /* Э�̳�ʼ����ɺ󣬿�������ȷ�����ȼ�������ӵ������б�. */
        prvAddCoRoutineToReadyQueue( pxCoRoutine );

        xReturn = pdPASS; // ���سɹ�
    }
    else
    {
        xReturn = errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; // �����ڴ�ʧ��
    }

    return xReturn; // ���ؽ��
}

/*-----------------------------------------------------------*/

/**
 * @brief ����ǰЭ����ӵ��ӳ��б��Ա���ָ�����ӳ�֮���ѡ�
 *
 * �˺������Ƚ���ǰЭ�̴Ӿ����б����Ƴ���Ȼ��������ӳ��б�
 * �����ݼ�����Ļ���ʱ���������������ʱ����������������б�
 * �ú�������ѡ��Э����ӵ��¼��б��С�
 *
 * @param xTicksToDelay ָ���ӳٵĵδ�������ʾЭ���ڴ˺�Ӧ�����ѡ�
 * @param pxEventList ָ���¼��б��ָ�룬�������Ҫ��ӵ��¼��б����� NULL��
 *
 * @note �˺���ֻ����Э���������е��ã���Ӧ���ж��е��á�
 */

void vCoRoutineAddToDelayedList( TickType_t xTicksToDelay, List_t *pxEventList )
{
    TickType_t xTimeToWake;

    /* ���㻽��ʱ�� - ����ܻ���������Ⲣ��������. */
    xTimeToWake = xCoRoutineTickCount + xTicksToDelay;

    /* �ڽ��Լ���ӵ������б�֮ǰ�������ȴӾ����б����Ƴ��Լ���
     * ��Ϊ�����б�ʹ�õ�����ͬ���б���. */
    ( void ) uxListRemove( ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );

    /* �б���ڻ���ʱ��˳���в���. */
    listSET_LIST_ITEM_VALUE( &( pxCurrentCoRoutine->xGenericListItem ), xTimeToWake );

    if( xTimeToWake < xCoRoutineTickCount )
    {
        /* ����ʱ���Ѿ����.
         * �������������б�. */
        vListInsert( ( List_t * ) pxOverflowDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
    }
    else
    {
        /* ����ʱ��û�������������ǿ���ʹ�õ�ǰ�������б�. */
        vListInsert( ( List_t * ) pxDelayedCoRoutineList, ( ListItem_t * ) &( pxCurrentCoRoutine->xGenericListItem ) );
    }

    if( pxEventList )
    {
        /* ��Ҫ��Э����ӵ��¼��б�.
         * �����������������ڽ����жϵ�����µ��ô˺���. */
        vListInsert( pxEventList, &( pxCurrentCoRoutine->xEventListItem ) );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief ���������б��鿴�Ƿ����κ�Э����Ҫ�ƶ��������б��С�
 *
 * �ú������ڴ������жϷ������̣�ISR��׼���õ�Э�̡�
 * ��Ϊ ISR ����ֱ�ӷ��ʾ����б�������ЩЭ�������ڴ������б��С�
 */
static void prvCheckPendingReadyList( void )
{
    /* ����Ƿ����κ�Э�̵ȴ����ƶ��������б��С�
       ��ЩЭ������ ISR ׼���õģ�ISR �����ܷ��ʾ����б�. */
    while( listLIST_IS_EMPTY( &xPendingReadyCoRoutineList ) == pdFALSE )
    {
        CRCB_t *pxUnblockedCRCB;

        /* �������б���Ա� ISR ����. */
        portDISABLE_INTERRUPTS(); // �����ж�
        {
            /* ��ȡ�������б�ͷ����Э�̿��ƿ飨CRCB��. */
            pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( (&xPendingReadyCoRoutineList) );
            ( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) ); // �Ƴ���Ӧ���¼��б���
        }
        portENABLE_INTERRUPTS(); // �ָ��ж�

        /* �Ӵ������б����Ƴ�Э��. */
        ( void ) uxListRemove( &( pxUnblockedCRCB->xGenericListItem ) );

        /* ����Э����ӵ������б�. */
        prvAddCoRoutineToReadyQueue( pxUnblockedCRCB );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief ����ӳ��б��鿴�Ƿ���Э�̳�ʱ��
 *
 * �˺����������ӳٵ�Э�̣�����Щ��ʱ��Э���ƶ��������б�
 * ���������δ���������δ�������ʱ���ύ���ӳ��б�
 */
static void prvCheckDelayedList( void )
{
    CRCB_t *pxCRCB;

    // �������ϴμ�����������ĵδ���
    xPassedTicks = xTaskGetTickCount() - xLastTickCount;
    
    while( xPassedTicks ) 
    {
        xCoRoutineTickCount++; // ���ӵ�ǰ�δ����
        xPassedTicks--; // ���پ����ĵδ���
        
        // ����δ���������������Ҫ�����ӳ��б�
        if( xCoRoutineTickCount == 0 ) 
        {
            List_t * pxTemp;

            // �δ��������������Ҫ�����ӳ��б�
            // ��� pxDelayedCoRoutineList �л����κ���Ŀ�����д���
            pxTemp = pxDelayedCoRoutineList;
            pxDelayedCoRoutineList = pxOverflowDelayedCoRoutineList;
            pxOverflowDelayedCoRoutineList = pxTemp;
        }

        // �鿴�Ƿ��г�ʱ
        while( listLIST_IS_EMPTY( pxDelayedCoRoutineList ) == pdFALSE ) 
        {
            // ��ȡ�ӳ��б�ͷ����Э�̿��ƿ�
            pxCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedCoRoutineList );

            // ����Ƿ�ʱ
            if( xCoRoutineTickCount < listGET_LIST_ITEM_VALUE( &( pxCRCB->xGenericListItem ) ) ) 
            {
                // ��ʱ��δ���ڣ��˳�ѭ��
                break;
            }

            portDISABLE_INTERRUPTS(); // �����ж�
            {
                // �¼������ڹؼ�����֮ǰ����
                // ��������������ͨ���б���������������б�
                ( void ) uxListRemove( &( pxCRCB->xGenericListItem ) ); // ���ӳ��б����Ƴ�

                // ���Э���Ƿ�Ҳ�ڵȴ��¼�
                if( pxCRCB->xEventListItem.pvContainer ) 
                {
                    ( void ) uxListRemove( &( pxCRCB->xEventListItem ) ); // ���¼��б����Ƴ�
                }
            }
            portENABLE_INTERRUPTS(); // �ָ��ж�

            // ��Э����ӵ������б�
            prvAddCoRoutineToReadyQueue( pxCRCB );
        }
    }

    // ������һ�μ��ĵδ����
    xLastTickCount = xCoRoutineTickCount;
}

/*-----------------------------------------------------------*/

/**
 * @brief ���ȵ�ǰ׼��������Э�̲�������ִ�к�����
 *
 * �˺�������Ƿ������¼�׼����Э����Ҫ�ƶ��������б���
 * ����Ƿ����ӳٵ�Э�̳�ʱ��Ȼ��������ȼ��ҵ�������ȼ���
 * ����Э�̣����������Ӧ��ִ�к�����
 */
void vCoRoutineSchedule( void )
{
    /* ����Ƿ������¼�׼���õ�Э����Ҫ�ƶ��������б���. */
    prvCheckPendingReadyList();

    /* ����Ƿ����ӳٵ�Э���ѳ�ʱ. */
    prvCheckDelayedList();

    /* �ҵ���������Э�̵�������ȼ�����. */
    while( listLIST_IS_EMPTY( &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) ) )
    {
        if( uxTopCoRoutineReadyPriority == 0 )
        {
            /* ���û�и����Э�̿ɹ���飬����. */
            return;
        }
        --uxTopCoRoutineReadyPriority; // �������ȼ��Լ����һ�����ȼ�
    }

    /* listGET_OWNER_OF_NEXT_ENTRY �����б�
       ȷ����ͬ���ȼ���Э���ܹ�ƽ��������ʱ��. */
    listGET_OWNER_OF_NEXT_ENTRY( pxCurrentCoRoutine, &( pxReadyCoRoutineLists[ uxTopCoRoutineReadyPriority ] ) );

    /* ����Э�̵�ִ�к���. */
    ( pxCurrentCoRoutine->pxCoRoutineFunction )( pxCurrentCoRoutine, pxCurrentCoRoutine->uxIndex );

    return;
}

/*-----------------------------------------------------------*/

/**
 * @brief ��ʼ��Э�̹�������ĸ����б�
 *
 * �˺�������ΪЭ�̵����������ͳ�ʼ����Ҫ���б�����
 * ����Э���б���ӳ�Э���б�������ʱ���������ȼ���
 * �����б���ӳ��б���г�ʼ������ȷ��Э�̹������ȷ�ԡ�
 */
static void prvInitialiseCoRoutineLists( void )
{
    UBaseType_t uxPriority;

    // ��ʼ���������ȼ��ľ���Э���б�
    for( uxPriority = 0; uxPriority < configMAX_CO_ROUTINE_PRIORITIES; uxPriority++ )
    {
        vListInitialise( ( List_t * ) &( pxReadyCoRoutineLists[ uxPriority ] ) ); // ��ʼ���ض����ȼ��ľ����б�
    }

    // ��ʼ���ӳ�Э���б�
    vListInitialise( ( List_t * ) &xDelayedCoRoutineList1 ); // ��ʼ����һ���ӳ��б�
    vListInitialise( ( List_t * ) &xDelayedCoRoutineList2 ); // ��ʼ���ڶ����ӳ��б�
    vListInitialise( ( List_t * ) &xPendingReadyCoRoutineList ); // ��ʼ���������б�

    /* ��ʼʱʹ�� list1 ��Ϊ pxDelayedCoRoutineList��
       ʹ�� list2 ��Ϊ pxOverflowDelayedCoRoutineList. */
    pxDelayedCoRoutineList = &xDelayedCoRoutineList1; // ָ��ǰ�ӳ��б�1
    pxOverflowDelayedCoRoutineList = &xDelayedCoRoutineList2; // ָ��ǰ�ӳ��б�2
}

/*-----------------------------------------------------------*/

/**
 * @brief ���¼��б����Ƴ�Э�̣���������ӵ��������б�
 *
 * �˺������ڴ�����¼��б��Ƴ���Э�̲��������������б�
 * �ú���ͨ�����жϷ��������е��ã�ֻ�ܷ����¼��б�ʹ������б�
 *
 * @param pxEventList ָ���¼��б��ָ�룬Э�̴����Ƴ���
 * @return ������Ƴ���Э�̵����ȼ����ڻ���ڵ�ǰЭ�̵����ȼ����򷵻� pdTRUE�����򷵻� pdFALSE��
 */
BaseType_t xCoRoutineRemoveFromEventList( const List_t *pxEventList )
{
    CRCB_t *pxUnblockedCRCB;
    BaseType_t xReturn;

    /* �ú������ж��ڲ������ã�ֻ�ܷ����¼��б�ʹ������б�
       �����������Ѽ�� pxEventList ��Ϊ��. */
    pxUnblockedCRCB = ( CRCB_t * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList ); // ��ȡ�¼��б�ͷ����Э�̿��ƿ�
    ( void ) uxListRemove( &( pxUnblockedCRCB->xEventListItem ) ); // ���¼��б����Ƴ���Э�̵��¼��б���
    vListInsertEnd( ( List_t * ) &( xPendingReadyCoRoutineList ), &( pxUnblockedCRCB->xEventListItem ) ); // �������������б�

    /* ������ȼ������ؽ��. */
    if( pxUnblockedCRCB->uxPriority >= pxCurrentCoRoutine->uxPriority )
    {
        xReturn = pdTRUE; // ���� pdTRUE ��ʾ���ȼ��ϸ�
    }
    else
    {
        xReturn = pdFALSE; // ���� pdFALSE ��ʾ���ȼ��ϵ�
    }

    return xReturn; // �������ȼ��ȽϵĽ��
}


#endif /* configUSE_CO_ROUTINES == 0 */

