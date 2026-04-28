#include "ht_client_task.h"
#include "ht_udp_server.h"
#include "stdio.h"
#include "api.h"
#include "cmsis_os2.h"
#include "string.h"
#include "main.h"

/* UDP服务连接状态（是否被远程主机连接） */
err_t udp_link_status = ERR_ABRT;

/**
 * @brief UDP服务器发送数据队列
 * 
 */
osMessageQueueId_t  udpServerSendDataQueueHandle = NULL;
osMessageQueueAttr_t udpServerSendDataQueue_attributes = {
  .name = "udpServerSendDataQueue"
};

/**
 * @brief UDP服务器线程，处理服务器收发数据
 * 
 */
osThreadId_t udpServerTaskHandle;
const osThreadAttr_t udpServerTask_attributes = {
  .name = "udpServerTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/**
 * @brief 新建一个服务器数据存储对象
 * 
 * @param len 
 * @return ServerData* 
 */
ServerData*
ht_udp_server_data_new (uint16_t len) {

  ServerData *d = pvPortMalloc (sizeof(ServerData));
	uint8_t* m = pvPortMalloc (len);

  memset (d, 0, sizeof(ServerData));
  memset (m, 0, len);

  d->len = len;
	d->buf = m;

  return d;
}


/**
 * @brief 使用数据新建一个服务器数据存储对象
 * 
 * @param buf 
 * @param len 
 * @return ServerData* 
 */
ServerData*
ht_udp_server_data_new_with_data (uint8_t *buf, uint16_t len) {

	ServerData *d = pvPortMalloc (sizeof(ServerData));
	uint8_t* m = pvPortMalloc (len);

  memset (d, 0, sizeof(ServerData));
  memset (m, 0, len);

	memcpy (m, buf, len);

	d->len = len;
	d->buf = m;
	
	return d;
}


/**
 * @brief 释放 server 占用的内存
 * 
 * @param d 
 */
void
ht_udp_server_data_free (ServerData *server) {
	if (server == NULL) return;

	if (server->buf == NULL) return;

	vPortFree (server->buf);
	vPortFree (server);
}
/**
 * @brief 需要发送的网络数据入队列，等待udp服务线程发送
 * 
 * @param server 
 * @return int8_t 
 */
int8_t
ht_udp_server_data_send (ServerData *server) {
	
	int8_t status = 0;

  if (udp_link_status != ERR_OK) {
    // printf ("\nudp_link_status != ERR_OK, not send data\n");
    ht_udp_server_data_free (server);
    return -1;
  }

  if (osMessageQueueGetSpace (udpServerSendDataQueueHandle) > 0) {
    osMessageQueuePut (udpServerSendDataQueueHandle, &server, 0, 0);
  } else {
    printf ("\nudpServerSendDataQueueHandle full, not send data\n");
    status = -1;
    ht_udp_server_data_free (server);
  }
	
	return status;
}


int8_t
ht_udp_server_data_new_and_send (uint8_t *buf, uint16_t len) {

  ServerData *data = ht_udp_server_data_new_with_data (buf, len);
  ht_udp_server_data_send (data);

  return 0;
}



static void 
ht_udp_server_thread (void *arg) {

  err_t recv_err;
	struct netbuf *recv_buf = NULL;  /* 从conn中读取获得的数据 */
  struct netbuf *send_buf = NULL;
  struct pbuf *q; /* 从 netbuf *recvbuf 中读取 pbuf */
  uint32_t recv_buf_len = 0; /* 记录接收到的数据的总长度 */

  struct netconn *conn = NULL;

  ServerData *serverData = NULL;
  ServerData *sendServerData = NULL;

  /* 远程主机目标IP、端口 */
	ip_addr_t *remote_ip;
	uint16_t remote_port;
		
	/* 创建一个UDP链接 */
	conn = netconn_new(NETCONN_UDP);

	/* 监听任何IP消息，在本地的 UDP_SERVER_CONTROL_PORT 端口 */
	netconn_bind(conn, IP_ADDR_ANY, UDP_SERVER_CONTROL_PORT);

	// IP4_ADDR(&remote_ip, 192, 168, 10, 206);
	// remote_port = 8080;

  /* 一直阻塞等待远程IP消息，收到消息后绑定远程IP和端口 */
  recv_err = netconn_recv(conn, &recv_buf);

  printf ("link..\n");

  if (recv_err == ERR_OK) {

    /* 获取远程地址和端口 */
    remote_ip = netbuf_fromaddr(recv_buf);
    remote_port = netbuf_fromport(recv_buf);

    printf("Received UDP from %s:%d\n", ipaddr_ntoa(remote_ip), remote_port);
  }
	
	/* 与其UDP通讯建立联系 */
	udp_link_status = netconn_connect(conn, remote_ip, remote_port);

  /* 禁止接收阻塞线程，超时时间设置 */
	netconn_set_recvtimeout(conn, 1);

	for(;;) {

    if (recv_err == ERR_OK) {

      /* 先计算所有链表中数据的总长度，以便接下来创建 */
      for(q = recv_buf->p; q != NULL; q = q->next) {
        recv_buf_len += q->len;
      }

      /* 确定 recv_buf_len 是有数据 */
      if (recv_buf_len) {

        serverData = ht_udp_server_data_new (recv_buf_len);

        uint16_t len = 0;

        /* 拷贝数据到 udpData */
        for(q = recv_buf->p; q != NULL; q = q->next) {
          
          memcpy(serverData->buf + len, q->payload, q->len);

          len += q->len;

        }
        
        /**
         * @note 注意：udp服务器线程只负责接收服务器数据，服务器的数据释放由客户端处理完后释放，必须释放！！！
         */
        ht_client_data_frame_process (serverData);

      }

      if (recv_buf) {
        netbuf_delete(recv_buf);
        recv_buf = NULL;
      }
  
      recv_buf_len = 0;

    }


    /**
     * 检查队列中是否有需要给客户端发送的消息，非阻塞检查队列中是否有数据
     * 
     * fixme: 队列数据连续发送，申请内存，可能会超过lwip内存池。根据发送数据的固定大小，连续发送达到最大值后，暂停发送。
     */
    while(osMessageQueueGet (udpServerSendDataQueueHandle, &sendServerData, 0, 0) == osOK) {
      
      uint8_t *p_buf = NULL;
      send_buf = netbuf_new ();

      if (send_buf == NULL) {
        printf ("%s netbuf_new() fail\n", __func__);
        goto exit;
      }

      p_buf = netbuf_alloc (send_buf, sendServerData->len);

      if (p_buf == NULL) {
        printf ("%s netbuf_alloc() fail\n", __func__);
        goto exit;
      }
    
      memcpy (p_buf, sendServerData->buf, sendServerData->len);
  
      /* 直接调用lwip底层发送数据函数 */
      netconn_send (conn, send_buf);
  
      ht_udp_server_data_free (sendServerData);

exit:

      /* 释放netbuf内存 */
      if (send_buf) {
        netbuf_delete(send_buf);
        send_buf = NULL;
      }

    }

    /* 阻塞等待远程消息 */
    recv_err = netconn_recv(conn, &recv_buf);
	}
}


/**
 * @brief 创建UDP服务器线程
 * 
 * @return uint8_t   0:  UDP服务器创建成功
 *                other：UDP服务器创建失败
 */
int8_t 
ht_udp_server_init(void) {

	udpServerSendDataQueueHandle = osMessageQueueNew (200, sizeof (ServerData *), &udpServerSendDataQueue_attributes);

	udpServerTaskHandle = osThreadNew(ht_udp_server_thread, NULL, &udpServerTask_attributes);
	
	return 0;
}
