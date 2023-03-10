/* -----------------------------------------------------------------------------
 * Copyright (c) 2023 WIZnet Co., Ltd. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * -------------------------------------------------------------------------- */
 
#include "TCP_Task.h"
#include "string.h"
#include "cmsis_os.h"

#define SOCKET_NUM 0
#define TARGET_PORT 5000

extern osSemaphoreId_t send_semHandle;
extern osSemaphoreId_t recv_semHandle;
extern osSemaphoreId_t coremark_start_semHandle;

static uint8_t eth_tx_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t eth_rx_buf[ETHERNET_BUF_MAX_SIZE];
static uint8_t target_ip[4] = {192, 168, 2, 100};

uint8_t send_ok_flag = 0;

void tcp_task_main (void *argument) {
  int ret;
  uint32_t send_count = 0;

  printf("%s\r\n", __func__);
  memset(eth_tx_buf, NULL, ETHERNET_BUF_MAX_SIZE);

  ret = socket(SOCKET_NUM, Sn_MR_TCP, TARGET_PORT, SF_TCP_NODELAY);
  if (ret != SOCKET_NUM)
  {
    printf(" Socket failed %d\n", ret);
    while (1);
  }

  while(1)
  {
    ret = connect(SOCKET_NUM, target_ip, TARGET_PORT);
    if (ret == SOCK_OK)
      break;
    osDelay(5000);
  }
  osSemaphoreRelease(coremark_start_semHandle);

  while(1)
  {
    send_count++;    
    sprintf(eth_tx_buf, "send count = %d\r\n\0", send_count);
    ret = send(SOCKET_NUM, eth_tx_buf, strlen(eth_tx_buf));
    osSemaphoreAcquire(send_semHandle, osWaitForever);
  }
}


void recv_task_main(void *argument)
{
  uint8_t socket_num;
  uint16_t reg_val;
  uint16_t ir_reg_val;
  uint16_t recv_len;

  while (1)
  {
    osSemaphoreAcquire(recv_semHandle, osWaitForever);
    ctlwizchip(CW_GET_INTERRUPT, (void *)&reg_val);
    reg_val = (reg_val >> 8) & 0x00FF;

    for (socket_num = 0; socket_num < _WIZCHIP_SOCK_NUM_; socket_num++)
    {
      if (reg_val & (1 << socket_num))
      {
        break;
      }
    }
    
    ctlsocket(socket_num, CS_GET_INTERRUPT, (void *)&ir_reg_val);
    if (socket_num == SOCKET_NUM)
    {
      reg_val = SIK_ALL;
      ctlsocket(socket_num, CS_CLR_INTERRUPT, (void *)&reg_val);

      if (ir_reg_val & SIK_SENT)
        send_ok_flag |= (1 << socket_num);

      if (ir_reg_val & SIK_RECEIVED)
      {
        getsockopt(socket_num, SO_RECVBUF, (void *)&recv_len);
        if (recv_len > 0)
        {
          memset(eth_rx_buf, 0, ETHERNET_BUF_MAX_SIZE);
          recv(SOCKET_NUM, eth_rx_buf, recv_len);
          printf("%.*s", recv_len, eth_rx_buf);
          osSemaphoreRelease(send_semHandle);
        }
      }
    }
  }
}



