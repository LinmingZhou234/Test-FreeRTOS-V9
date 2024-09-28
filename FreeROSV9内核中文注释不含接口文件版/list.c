
#include <stdlib.h>
#include "FreeRTOS.h"
#include "list.h"

/*-----------------------------------------------------------
 * PUBLIC LIST API documented in list.h
 *----------------------------------------------------------*/
/**
 * ��ʼ���б�
 * @ingroup ListInitialization
 *
 * ���б�����Ϊ��״̬��ʹ��׼�������ڴ洢�����ʼ�����̰��������б�Ľ�����ǣ���ȷ���б�Ľ�����ǳ�Ϊ�б��е�ΨһԪ�ء�
 *
 * @param[in,out] pxList Ҫ��ʼ�����б�
 */
void vListInitialise( List_t * const pxList )
{
    /* �б�ṹ����һ���б�����б��������б�Ľ�����ǡ���ʼ���б�ʱ��
       �б�Ľ��������ΪΨһ���б���Ŀ���롣 */
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );			/*lint !e826 !e740 ʹ�������б�ṹ��Ϊ�б�Ľ�������Խ�ʡRAM�����Ǿ�����鲢��Ч�ġ� */

    /* �б������ǵ�ֵ����ߵĿ���ֵ����ȷ����ʼ��λ���б��ĩβ�� */
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* �б������ǵ���һ����ǰһ��ָ��ָ�������Ա�����֪���б��ʱΪ�ա� */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );	/*lint !e826 !e740 ʹ�������б�ṹ��Ϊ�б�Ľ�������Խ�ʡRAM�����Ǿ�����鲢��Ч�ġ� */
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );/*lint !e826 !e740 ʹ�������б�ṹ��Ϊ�б�Ľ�������Խ�ʡRAM�����Ǿ�����鲢��Ч�ġ� */

    /* �б��е�������Ϊ�㡣 */
    pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

    /* ��������� configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES ����Ϊ 1����
       ����ֵ֪д���б��У���������������ԡ� */
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );
}
/*-----------------------------------------------------------*/

/**
 * ��ʼ���б���
 * @ingroup ListInitialization
 *
 * ���б�������Ϊδ���ӵ��κ��б��״̬���������������
 * configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES ����Ϊ 1��������б�����д����ֵ֪��
 * ��������������Լ�顣
 *
 * @param[in,out] pxItem Ҫ��ʼ�����б��
 */
void vListInitialiseItem( ListItem_t * const pxItem )
{
    /* ȷ���б���ᱻ��¼Ϊ����ĳ���б� */
    pxItem->pvContainer = NULL;

    /* ��������� configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES ����Ϊ 1����
       ���б�����д����ֵ֪����������������ԡ� */
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
/*-----------------------------------------------------------*/

/**
 * ���б�ĩβ���һ������
 * @ingroup ListItemManipulation
 *
 * ��һ���µ��б�����ӵ��б��ĩβ�������������б�ʹ���б����Ϊͨ��
 * listGET_OWNER_OF_NEXT_ENTRY() ��������ʱ���һ�����Ƴ����
 *
 * @param[in,out] pxList Ҫ�������б�
 * @param[in] pxNewListItem Ҫ��ӵ����б��
 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t * const pxIndex = pxList->pxIndex;

    /* �� configASSERT() Ҳ������ʱ����Щ���Կ��ܻᲶ׽���б����ݽṹ���ڴ���
       �����ǵ���������ǲ��Ჶ׽���ڴ������û�ʹ�� FreeRTOS ��������ݴ��� */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* �����б������ pxList�������������б����������б����Ϊͨ��
       listGET_OWNER_OF_NEXT_ENTRY() ��������ʱ���һ�����Ƴ���� */
    pxNewListItem->pxNext = pxIndex;  /* �������һ��ָ��ָ���б������� */
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;  /* �����ǰһ��ָ��ָ��ǰ�б�����һ���� */

    /* ֻ�ھ��߸��ǲ����ڼ�ʹ�á� */
    mtCOVERAGE_TEST_DELAY();

    /* ���µ�ǰ�б�����һ�������һ��ָ�룬ʹ��ָ������ */
    pxIndex->pxPrevious->pxNext = pxNewListItem;
    /* �����б�Ľ�����ǵ�ǰһ��ָ�룬ʹ��ָ������ */
    pxIndex->pxPrevious = pxNewListItem;

    /* ��¼�������ڵ��б� */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* �����б��е������� */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

/**
 * ���� xItemValue ˳�����һ�����б���
 * @ingroup ListItemManipulation
 *
 * ��һ���µ��б������ `xItemValue` ��ֵ�����б��С�����б����Ѿ�����һ��������ͬ `xItemValue` ���б�������б���Ӧ�����������ȷ���˴洢�ھ����б��е� TCB��������ͬ�� `xItemValue` ֵ���ܹ����� CPU ʱ�䡣
 *
 * @param[in,out] pxList Ҫ��������б�
 * @param[in] pxNewListItem Ҫ��������б��
 */
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t *pxIterator;
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;

    /* ��������� configASSERT()��������б������Ժ��б���������Լ�顣 */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* ���� xItemValue ��˳�����б�������б��С�

    ����б����Ѿ�����һ��������ͬ xItemValue ���б�������б���Ӧ���������
    ��������ȷ���洢�ھ����б��е� TCB��������ͬ�� xItemValue ֵ���ܹ����� CPU ʱ�䡣
    ���ǣ���� xItemValue ���б������ǵ�ֵ��ͬ��������ĵ���ѭ�������������
    ������ȼ��ֵ�����ڱ�Ҫʱ��΢�޸��㷨�� */
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        /* *** ע�� ***********************************************************
        ������Ӧ�ó����ڴ˴����������ܵ�ԭ�����¡����⣬�����
        http://www.freertos.org/FAQHelp.html ��ȡ������ʾ����ȷ�������� configASSERT()��

            1) ջ��� -
               �μ� http://www.freertos.org/Stacks-and-stack-overflow-checking.html
            2) �ж����ȼ����䲻��ȷ���������� Cortex-M ϵ�д������ϣ���ֵ�ϸߵ����ȼ�ֵ��ʾ�ϵ͵�ʵ���ж����ȼ���������Ե÷�ֱ�����μ�
               http://www.freertos.org/RTOS-Cortex-M3-M4.html �Ͷ���
               configMAX_SYSCALL_INTERRUPT_PRIORITY �ĵط���
            3) ���ٽ����ڲ�����ȳ�����ͣʱ���� API ���������ߴ��ж��е���û���� "FromISR" ��β�� API ������
            4) �ڳ�ʼ�����л��ź���֮ǰ�����������ȳ���֮ǰʹ�ö��л��ź������ж��Ƿ��ڵ��� vTaskStartScheduler() ֮ǰ����������
        **********************************************************************/

        /* Ѱ�Ҿ���С�ڵ��� xValueOfInsertion �� xItemValue �����һ���б�� */
        for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext ) /*lint !e826 !e740 ʹ�������б�ṹ��Ϊ�б�Ľ�������Խ�ʡRAM�����Ǿ�����鲢��Ч�ġ� */
        {
            /* ���ﲻ��Ҫ��ʲô��ֻ�ǵ�������Ҫ�Ĳ���λ�á� */
        }
    }

    /* �������б��� */
    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;

    /* ��¼�������ڵ��б��������Ժ�����Ƴ���� */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* �����б��е������� */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

/**
 * ���б����Ƴ�һ����
 * @ingroup ListItemManipulation
 *
 * ���б����Ƴ�ָ�����б���������Ƴ�����б���������
 *
 * @param[in,out] pxItemToRemove Ҫ�Ƴ����б��
 *
 * @return �Ƴ�����б���������
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
    /* �б���֪���������ĸ��б����б����л�ȡ�������б� */
    List_t * const pxList = ( List_t * ) pxItemToRemove->pvContainer;

    /* �����б����ǰһ���ͺ�һ��ָ�룬ʹ���ƹ�Ҫ�Ƴ����б�� */
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* ֻ�ھ��߸��ǲ����ڼ�ʹ�á� */
    mtCOVERAGE_TEST_DELAY();

    /* ȷ��������Ȼָ��һ����Ч���б�� */
    if( pxList->pxIndex == pxItemToRemove )
    {
        /* ���Ҫ�Ƴ���������������ָ�������������ָ���Ƴ����ǰһ��� */
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    else
    {
        /* �����ھ��߸��ǲ��ԡ� */
        mtCOVERAGE_TEST_MARKER();
    }

    /* �����Ƴ��������ָ��Ϊ NULL����ʾ�����������κ��б� */
    pxItemToRemove->pvContainer = NULL;

    /* �����б��е������� */
    ( pxList->uxNumberOfItems )--;

    /* �����Ƴ�����б��������� */
    return pxList->uxNumberOfItems;
}
/*-----------------------------------------------------------*/

