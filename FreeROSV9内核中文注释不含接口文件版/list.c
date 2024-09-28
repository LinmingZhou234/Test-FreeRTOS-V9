
#include <stdlib.h>
#include "FreeRTOS.h"
#include "list.h"

/*-----------------------------------------------------------
 * PUBLIC LIST API documented in list.h
 *----------------------------------------------------------*/
/**
 * 初始化列表
 * @ingroup ListInitialization
 *
 * 将列表设置为空状态，使其准备好用于存储新项。初始化过程包括设置列表的结束标记，并确保列表的结束标记成为列表中的唯一元素。
 *
 * @param[in,out] pxList 要初始化的列表。
 */
void vListInitialise( List_t * const pxList )
{
    /* 列表结构包含一个列表项，该列表项用作列表的结束标记。初始化列表时，
       列表的结束标记作为唯一的列表条目插入。 */
    pxList->pxIndex = ( ListItem_t * ) &( pxList->xListEnd );			/*lint !e826 !e740 使用迷你列表结构作为列表的结束标记以节省RAM。这是经过检查并有效的。 */

    /* 列表结束标记的值是最高的可能值，以确保它始终位于列表的末尾。 */
    pxList->xListEnd.xItemValue = portMAX_DELAY;

    /* 列表结束标记的下一个和前一个指针指向自身，以便我们知道列表何时为空。 */
    pxList->xListEnd.pxNext = ( ListItem_t * ) &( pxList->xListEnd );	/*lint !e826 !e740 使用迷你列表结构作为列表的结束标记以节省RAM。这是经过检查并有效的。 */
    pxList->xListEnd.pxPrevious = ( ListItem_t * ) &( pxList->xListEnd );/*lint !e826 !e740 使用迷你列表结构作为列表的结束标记以节省RAM。这是经过检查并有效的。 */

    /* 列表中的项数设为零。 */
    pxList->uxNumberOfItems = ( UBaseType_t ) 0U;

    /* 如果配置项 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则
       将已知值写入列表中，以提高数据完整性。 */
    listSET_LIST_INTEGRITY_CHECK_1_VALUE( pxList );
    listSET_LIST_INTEGRITY_CHECK_2_VALUE( pxList );
}
/*-----------------------------------------------------------*/

/**
 * 初始化列表项
 * @ingroup ListInitialization
 *
 * 将列表项设置为未附加到任何列表的状态，并且如果配置项
 * configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则会在列表项中写入已知值，
 * 以提高数据完整性检查。
 *
 * @param[in,out] pxItem 要初始化的列表项。
 */
void vListInitialiseItem( ListItem_t * const pxItem )
{
    /* 确保列表项不会被记录为属于某个列表。 */
    pxItem->pvContainer = NULL;

    /* 如果配置项 configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES 设置为 1，则
       在列表项中写入已知值，以提高数据完整性。 */
    listSET_FIRST_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
    listSET_SECOND_LIST_ITEM_INTEGRITY_CHECK_VALUE( pxItem );
}
/*-----------------------------------------------------------*/

/**
 * 在列表末尾添加一个新项
 * @ingroup ListItemManipulation
 *
 * 将一个新的列表项添加到列表的末尾，而不是排序列表，使新列表项成为通过
 * listGET_OWNER_OF_NEXT_ENTRY() 函数调用时最后一个被移除的项。
 *
 * @param[in,out] pxList 要添加项的列表。
 * @param[in] pxNewListItem 要添加的新列表项。
 */
void vListInsertEnd( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t * const pxIndex = pxList->pxIndex;

    /* 当 configASSERT() 也被定义时，这些测试可能会捕捉到列表数据结构在内存中
       被覆盖的情况。它们不会捕捉由于错误配置或使用 FreeRTOS 引起的数据错误。 */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* 将新列表项插入 pxList，而不是排序列表，而是让新列表项成为通过
       listGET_OWNER_OF_NEXT_ENTRY() 函数调用时最后一个被移除的项。 */
    pxNewListItem->pxNext = pxIndex;  /* 新项的下一个指针指向列表结束标记 */
    pxNewListItem->pxPrevious = pxIndex->pxPrevious;  /* 新项的前一个指针指向当前列表的最后一个项 */

    /* 只在决策覆盖测试期间使用。 */
    mtCOVERAGE_TEST_DELAY();

    /* 更新当前列表的最后一个项的下一个指针，使其指向新项 */
    pxIndex->pxPrevious->pxNext = pxNewListItem;
    /* 更新列表的结束标记的前一个指针，使其指向新项 */
    pxIndex->pxPrevious = pxNewListItem;

    /* 记录新项所在的列表。 */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* 增加列表中的项数。 */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

/**
 * 按照 xItemValue 顺序插入一个新列表项
 * @ingroup ListItemManipulation
 *
 * 将一个新的列表项按照其 `xItemValue` 的值插入列表中。如果列表中已经包含一个具有相同 `xItemValue` 的列表项，则新列表项应放置在其后。这确保了存储在就绪列表中的 TCB（具有相同的 `xItemValue` 值）能够共享 CPU 时间。
 *
 * @param[in,out] pxList 要插入项的列表。
 * @param[in] pxNewListItem 要插入的新列表项。
 */
void vListInsert( List_t * const pxList, ListItem_t * const pxNewListItem )
{
    ListItem_t *pxIterator;
    const TickType_t xValueOfInsertion = pxNewListItem->xItemValue;

    /* 如果定义了 configASSERT()，则进行列表完整性和列表项的完整性检查。 */
    listTEST_LIST_INTEGRITY( pxList );
    listTEST_LIST_ITEM_INTEGRITY( pxNewListItem );

    /* 按照 xItemValue 的顺序将新列表项插入列表中。

    如果列表中已经包含一个具有相同 xItemValue 的列表项，则新列表项应放置在其后。
    这样可以确保存储在就绪列表中的 TCB（具有相同的 xItemValue 值）能够共享 CPU 时间。
    但是，如果 xItemValue 与列表结束标记的值相同，则下面的迭代循环将不会结束。
    因此首先检查值，并在必要时稍微修改算法。 */
    if( xValueOfInsertion == portMAX_DELAY )
    {
        pxIterator = pxList->xListEnd.pxPrevious;
    }
    else
    {
        /* *** 注意 ***********************************************************
        如果你的应用程序在此处崩溃，可能的原因如下。另外，请参阅
        http://www.freertos.org/FAQHelp.html 获取更多提示，并确保定义了 configASSERT()！

            1) 栈溢出 -
               参见 http://www.freertos.org/Stacks-and-stack-overflow-checking.html
            2) 中断优先级分配不正确，尤其是在 Cortex-M 系列处理器上，数值较高的优先级值表示较低的实际中断优先级，这可能显得反直觉。参见
               http://www.freertos.org/RTOS-Cortex-M3-M4.html 和定义
               configMAX_SYSCALL_INTERRUPT_PRIORITY 的地方。
            3) 在临界区内部或调度程序暂停时调用 API 函数，或者从中断中调用没有以 "FromISR" 结尾的 API 函数。
            4) 在初始化队列或信号量之前或在启动调度程序之前使用队列或信号量（中断是否在调用 vTaskStartScheduler() 之前触发？）。
        **********************************************************************/

        /* 寻找具有小于等于 xValueOfInsertion 的 xItemValue 的最后一个列表项。 */
        for( pxIterator = ( ListItem_t * ) &( pxList->xListEnd ); pxIterator->pxNext->xItemValue <= xValueOfInsertion; pxIterator = pxIterator->pxNext ) /*lint !e826 !e740 使用迷你列表结构作为列表的结束标记以节省RAM。这是经过检查并有效的。 */
        {
            /* 这里不需要做什么，只是迭代到想要的插入位置。 */
        }
    }

    /* 插入新列表项 */
    pxNewListItem->pxNext = pxIterator->pxNext;
    pxNewListItem->pxNext->pxPrevious = pxNewListItem;
    pxNewListItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewListItem;

    /* 记录新项所在的列表。这允许稍后快速移除该项。 */
    pxNewListItem->pvContainer = ( void * ) pxList;

    /* 增加列表中的项数。 */
    ( pxList->uxNumberOfItems )++;
}
/*-----------------------------------------------------------*/

/**
 * 从列表中移除一个项
 * @ingroup ListItemManipulation
 *
 * 从列表中移除指定的列表项，并返回移除后的列表项数量。
 *
 * @param[in,out] pxItemToRemove 要移除的列表项。
 *
 * @return 移除后的列表项数量。
 */
UBaseType_t uxListRemove( ListItem_t * const pxItemToRemove )
{
    /* 列表项知道它属于哪个列表。从列表项中获取所属的列表。 */
    List_t * const pxList = ( List_t * ) pxItemToRemove->pvContainer;

    /* 更新列表项的前一个和后一个指针，使其绕过要移除的列表项。 */
    pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;
    pxItemToRemove->pxPrevious->pxNext = pxItemToRemove->pxNext;

    /* 只在决策覆盖测试期间使用。 */
    mtCOVERAGE_TEST_DELAY();

    /* 确保索引仍然指向一个有效的列表项。 */
    if( pxList->pxIndex == pxItemToRemove )
    {
        /* 如果要移除的项正好是索引指向的项，则更新索引指向移除项的前一个项。 */
        pxList->pxIndex = pxItemToRemove->pxPrevious;
    }
    else
    {
        /* 仅用于决策覆盖测试。 */
        mtCOVERAGE_TEST_MARKER();
    }

    /* 设置移除项的容器指针为 NULL，表示它不再属于任何列表。 */
    pxItemToRemove->pvContainer = NULL;

    /* 减少列表中的项数。 */
    ( pxList->uxNumberOfItems )--;

    /* 返回移除后的列表项数量。 */
    return pxList->uxNumberOfItems;
}
/*-----------------------------------------------------------*/

