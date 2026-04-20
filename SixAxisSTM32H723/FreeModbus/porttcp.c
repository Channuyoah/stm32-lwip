/*
 * FreeModbus Libary: lwIP Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

/* ----------------------- System includes ----------------------------------*/
#include <stdio.h>
#include <string.h>

#include "port.h"

/* ----------------------- lwIP includes ------------------------------------*/
#include "lwip/api.h"
#include "lwip/tcp.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- MBAP Header --------------------------------------*/
#define MB_TCP_UID          6
#define MB_TCP_LEN          4
#define MB_TCP_FUNC         7

/* ----------------------- Defines  -----------------------------------------*/
#define MB_TCP_DEFAULT_PORT 502 /* TCP listening port. */
#define MB_TCP_BUF_SIZE     ( 256 + 7 ) /* Must hold a complete Modbus TCP frame. */

// #define MB_TCP_DEBUG 1

/* ----------------------- Prototypes ---------------------------------------*/
void            vMBPortEventClose( void );
void            vMBPortLog( eMBPortLogLevel eLevel, const CHAR * szModule,
                            const CHAR * szFmt, ... );

/* ----------------------- Static variables ---------------------------------*/
static struct netconn *pxNetConnListen = NULL;  // 监听连接
static struct netconn *pxNetConnClient = NULL;  // 客户端连接

static UCHAR    aucTCPBuf[MB_TCP_BUF_SIZE];     // TCP接收缓冲区
static USHORT   usTCPBufPos;                    // 缓冲区当前位置

static BOOL     bClientConnected = FALSE;       // 客户端连接状态
static BOOL     bFrameReceived = FALSE;         // 完整帧接收标志
static USHORT   usFrameLength = 0;              // 接收到的帧长度


osThreadId_t modbusTcpTaskHandle;
const osThreadAttr_t modbusTcpTask_attributes = {
  .name = "modbusTcpTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh7,
};


/* ----------------------- Static functions ---------------------------------*/
static void     prvvMBPortReleaseClient( void );
static void     prvvMBTCPReceiveTask( void );


/* ----------------------- Begin implementation -----------------------------*/
/**
 * @brief 初始化Modbus TCP端口 (使用Netconn API)
 * @param usTCPPort: TCP端口号，0表示使用默认端口502
 * @return TRUE: 成功, FALSE: 失败
 * @note 此函数仅创建监听连接，实际的accept操作在任务中执行
 */
BOOL
xMBTCPPortInit( USHORT usTCPPort )
{
    err_t err;
    USHORT usPort;

    if( usTCPPort == 0 )
    {
        usPort = MB_TCP_DEFAULT_PORT;
    }
    else
    {
        usPort = ( USHORT ) usTCPPort;
    }

    // 创建TCP连接
    pxNetConnListen = netconn_new( NETCONN_TCP );
    if( pxNetConnListen == NULL )
    {
      vMBPortLog( MB_LOG_ERROR, "MBTCP-INIT", "Failed to create netconn\r\n" );
      return FALSE;
    }

    // 绑定端口
    err = netconn_bind( pxNetConnListen, IP_ADDR_ANY, ( u16_t ) usPort );
    if( err != ERR_OK )
    {
      vMBPortLog( MB_LOG_ERROR, "MBTCP-INIT", "Bind failed with error: %d\r\n", err );
      netconn_delete( pxNetConnListen );
      pxNetConnListen = NULL;
      return FALSE;
    }

    // 开始监听
    err = netconn_listen( pxNetConnListen );
    if( err != ERR_OK )
    {
      vMBPortLog( MB_LOG_ERROR, "MBTCP-INIT", "Listen failed with error: %d\r\n", err );
      netconn_delete( pxNetConnListen );
      pxNetConnListen = NULL;
      return FALSE;
    }

#ifdef MB_TCP_DEBUG
    vMBPortLog( MB_LOG_DEBUG, "MBTCP-INIT", "Modbus TCP server ready on port %d\r\n", usPort );
#endif

    // 初始化状态变量
    bClientConnected = FALSE;
    bFrameReceived = FALSE;
    usTCPBufPos = 0;
    usFrameLength = 0;

    return TRUE;
}


/**
 * @brief 释放客户端连接 (Netconn API)
 * @note 关闭并删除客户端netconn连接
 */
void
prvvMBPortReleaseClient( void )
{
    if( pxNetConnClient != NULL )
    {
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-CLOSE", "Closing client connection.\r\n" );
#endif
        netconn_close( pxNetConnClient );
        netconn_delete( pxNetConnClient );
        pxNetConnClient = NULL;
    }
    
    bClientConnected = FALSE;
    bFrameReceived = FALSE;
    usTCPBufPos = 0;
    usFrameLength = 0;
}


/**
 * @brief 关闭TCP端口 (Netconn API)
 * @note 关闭客户端连接和监听连接，释放所有资源
 */
void
vMBTCPPortClose( void )
{
    /* 关闭客户端连接 */
    prvvMBPortReleaseClient();

    /* 关闭监听连接 */
    if( pxNetConnListen != NULL )
    {
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-CLOSE", "Closing listening socket.\r\n" );
#endif
        netconn_close( pxNetConnListen );
        netconn_delete( pxNetConnListen );
        pxNetConnListen = NULL;
    }

    /* 释放事件队列资源 */
    vMBPortEventClose();
}


/**
 * @brief 禁用TCP端口 (Netconn API)
 * @note 断开当前客户端连接，但保持监听状态
 */
void
vMBTCPPortDisable( void )
{
    prvvMBPortReleaseClient();
}


/**
 * @brief 获取接收到的Modbus TCP请求帧
 * @param ppucMBTCPFrame: 返回接收到的Modbus TCP帧指针
 * @param usTCPLength: 返回接收到的数据长度
 * @return TRUE: 有完整帧可用, FALSE: 无帧可用
 * @note 此函数由eMBPoll()调用，从缓冲区获取已接收的完整帧
 */
BOOL
xMBTCPPortGetRequest( UCHAR ** ppucMBTCPFrame, USHORT * usTCPLength )
{
    BOOL bReturn = FALSE;

    /* 检查是否有完整帧可用 */
    if( bFrameReceived == TRUE )
    {
        *ppucMBTCPFrame = &aucTCPBuf[0];
        *usTCPLength = usFrameLength;
        
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-GET", "Returning frame, length = %d\r\n", usFrameLength );
#endif
        
        /* 清除标志，准备接收下一帧 */
        bFrameReceived = FALSE;
        usFrameLength = 0;
        usTCPBufPos = 0;
        
        bReturn = TRUE;
    }

    return bReturn;
}


/**
 * @brief 发送Modbus TCP响应 (Netconn API)
 * @param pucMBTCPFrame: Modbus TCP帧数据
 * @param usTCPLength: 帧长度
 * @return TRUE: 发送成功, FALSE: 发送失败
 * @note 使用netconn_write发送响应数据
 */
BOOL
xMBTCPPortSendResponse( const UCHAR * pucMBTCPFrame, USHORT usTCPLength )
{
    err_t err;
    BOOL bFrameSent = FALSE;

    if( pxNetConnClient != NULL && bClientConnected )
    {
        /* 使用netconn_write发送数据 */
        err = netconn_write( pxNetConnClient, pucMBTCPFrame, usTCPLength, NETCONN_COPY );
        
        if( err == ERR_OK )
        {
#ifdef MB_TCP_DEBUG
            vMBPortLog( MB_LOG_DEBUG, "MBTCP-SENT", "Sent response, length = %d\r\n", usTCPLength );
#endif
            bFrameSent = TRUE;
        }
        else
        {
            /* 发送失败，断开连接 */
#ifdef MB_TCP_DEBUG
            vMBPortLog( MB_LOG_ERROR, "MBTCP-ERROR", "Send error: %d. Closing connection.\r\n", err );
#endif
            prvvMBPortReleaseClient();
        }
    }
    
    return bFrameSent;
}


void vMBPortEventClose( void ) {
  /* 占位函数，用于事件队列清理 */
}


/**
 * @brief TCP接收任务的内部处理函数（阻塞方式）
 * @note 此函数在xq_modbus_tcp_task任务中被调用
 *       使用阻塞方式接受客户端连接和接收TCP数据
 */
static void
prvvMBTCPReceiveTask( void )
{
    struct netbuf *pxRxBuffer;
    err_t err;
    UCHAR *pucRxData;
    u16_t usRxDataLen;
    USHORT usLength;

    /* 1. 如果没有客户端连接，阻塞等待新连接 */
    if( pxNetConnClient == NULL )
    {
        /* 阻塞方式接受连接，无超时，一直等待 */
        netconn_set_recvtimeout( pxNetConnListen, 0 );  // 0 = 阻塞模式
        
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-WAIT", "Waiting for client connection...\r\n" );
#endif
        
        err = netconn_accept( pxNetConnListen, &pxNetConnClient );
        
        if( err == ERR_OK && pxNetConnClient != NULL )
        {
#ifdef MB_TCP_DEBUG
            vMBPortLog( MB_LOG_DEBUG, "MBTCP-ACCEPT", "Accepted new client connection.\r\n" );
#endif
            bClientConnected = TRUE;
            usTCPBufPos = 0;
            bFrameReceived = FALSE;
            
            /* 设置接收为阻塞模式 */
            netconn_set_recvtimeout( pxNetConnClient, 0 );  // 0 = 阻塞模式
        }
        else
        {
#ifdef MB_TCP_DEBUG
            vMBPortLog( MB_LOG_ERROR, "MBTCP-ERROR", "Accept error: %d\r\n", err );
#endif
        }
        return;
    }

    /* 2. 如果已经有完整帧待处理，不接收新数据 */
    if( bFrameReceived == TRUE )
    {
        return;  // 等待eMBPoll()处理当前帧
    }

    /* 3. 阻塞接收TCP数据，一直等待直到有数据 */
    err = netconn_recv( pxNetConnClient, &pxRxBuffer);
    
    if( err != ERR_OK )
    {
        /* 连接错误，断开客户端 */
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_DEBUG, "MBTCP-ERROR", "Receive error: %d. Closing connection.\r\n", err );
#endif
        prvvMBPortReleaseClient();
        return;
    }

    /* 4. 获取接收到的数据 */
    netbuf_data( pxRxBuffer, (void **)&pucRxData, &usRxDataLen );

    /* 5. 检查缓冲区溢出 */
    if( ( usTCPBufPos + usRxDataLen ) >= MB_TCP_BUF_SIZE )
    {
#ifdef MB_TCP_DEBUG
        vMBPortLog( MB_LOG_ERROR, "MBTCP-ERROR", "Buffer overflow. Dropping client.\r\n" );
#endif
        netbuf_delete( pxRxBuffer );
        prvvMBPortReleaseClient();
        return;
    }

    /* 6. 复制数据到内部缓冲区 */
    memcpy( &aucTCPBuf[usTCPBufPos], pucRxData, usRxDataLen );
    usTCPBufPos += usRxDataLen;

    /* 释放netbuf */
    netbuf_delete( pxRxBuffer );

    /* 7. 检查是否接收到完整的MBAP头 */
    if( usTCPBufPos >= MB_TCP_FUNC )
    {
        /* 获取Modbus PDU长度 (功能码 + 数据 + 单元标识符) */
        usLength = aucTCPBuf[MB_TCP_LEN] << 8U;
        usLength |= aucTCPBuf[MB_TCP_LEN + 1];

        /* 检查是否接收到完整帧 */
        if( usTCPBufPos >= ( MB_TCP_UID + usLength ) )
        {
            if( usTCPBufPos == ( MB_TCP_UID + usLength ) )
            {
                /* 完整帧已接收 */
                usFrameLength = usTCPBufPos;
                bFrameReceived = TRUE;

#ifdef MB_TCP_DEBUG
                vMBPortLog( MB_LOG_DEBUG, "MBTCP-RECV", "Complete frame received, length = %d\r\n", usFrameLength );
#endif

                /* 发送事件通知eMBPoll()处理 */
                (void)xMBPortEventPost( EV_FRAME_RECEIVED );
            }
            else
            {
                /* 接收到的数据太多，协议错误 */
#ifdef MB_TCP_DEBUG
                vMBPortLog( MB_LOG_ERROR, "MBTCP-ERROR", "Received too many bytes. Dropping client.\r\n" );
#endif
                prvvMBPortReleaseClient();
            }
        }
        /* else: 需要继续接收更多数据 */
    }
}


void 
xq_modbus_tcp_task (void *argument) {

  eMBTCPInit (MB_TCP_DEFAULT_PORT);

  eMBEnable ();

  for (;;) {

    /* 1. 处理TCP接收（接受连接、接收数据）- 阻塞模式 */
    prvvMBTCPReceiveTask();
    
    /* 2. 处理Modbus协议（响应请求） */
    eMBPoll ();

    /* 因为 prvvMBTCPReceiveTask 阻塞的， eMBPoll状态机是先接收，然后再执行发送，如何这里再调用一次eMBPoll */
    eMBPoll ();

    /* 注意：不需要延时，因为接收是阻塞的 */
  }

  // eMBDisable ();
  // eMBClose  ();

}

int8_t
xq_modbus_tcp_task_init (void) {

  int8_t status = 0;

  modbusTcpTaskHandle = osThreadNew(xq_modbus_tcp_task, NULL, &modbusTcpTask_attributes);

  return status;
}
