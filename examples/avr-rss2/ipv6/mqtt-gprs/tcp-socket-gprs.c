/*
 * Copyright (c) 2012-2014, Thingsquare, http://www.thingsquare.com/.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

#include "contiki.h"
#include "sys/cc.h"
#include "contiki-net.h"

#include "lib/list.h"

#include "tcp-socket-gprs.h"

#include <string.h>

static void relisten(struct tcp_socket_gprs *s);

LIST(socketlist_gprs);
/*---------------------------------------------------------------------------*/
PROCESS(tcp_socket_gprs_process, "TCP socket process");
/*---------------------------------------------------------------------------*/

static void
call_event(struct tcp_socket_gprs *s, tcp_socket_gprs_event_t event)
{
  if(s != NULL && s->event_callback != NULL) {
    s->event_callback(s, s->ptr, event);
  }
}
/*---------------------------------------------------------------------------*/
#if 0
static void
senddata(struct tcp_socket_gprs *s)
{
  int len = MIN(s->output_data_max_seg, uip_mss());

  if(s->output_senddata_len > 0) {
    len = MIN(s->output_senddata_len, len);
    s->output_data_send_nxt = len;
    uip_send(s->output_data_ptr, len);
  }
}
#endif
/* 
 * Unlike tcp-socket.c:senddata(), gprs_send() will send all data before 
 * generating a callback() confirming data sent. 
 */
static void
senddata(struct tcp_socket_gprs *s)
{
  struct gprs_connection *gprsconn;
  //int len = MIN(s->output_data_max_seg, uip_mss());
  int len = GPRS_MAX_SEND_LEN; /* Not really MSS...*/

  if(s->output_senddata_len > 0) {
    len = MIN(s->output_senddata_len, len);
    len = s->output_senddata_len;
    s->output_data_send_nxt = len;
    gprsconn = s->g_c;
    gprsconn->output_data_ptr = s->output_data_ptr;
    gprsconn->output_data_len = len;
    gprs_send(gprsconn);
  }
}
/*---------------------------------------------------------------------------*/
static void
acked(struct tcp_socket_gprs *s)
{
  if(s->output_senddata_len > 0) {
    /* Copy the data in the outputbuf down and update outputbufptr and
       outputbuf_lastsent */

    if(s->output_data_send_nxt > 0) {
      memcpy(&s->output_data_ptr[0],
             &s->output_data_ptr[s->output_data_send_nxt],
             s->output_data_maxlen - s->output_data_send_nxt);
    }
    if(s->output_data_len < s->output_data_send_nxt) {
      PRINTF("tcp-gprs: acked assertion failed s->output_data_len (%d) < s->output_data_send_nxt (%d)\n",
             s->output_data_len,
             s->output_data_send_nxt);
      //tcp_markconn(uip_conn, NULL);
      //uip_abort();
      call_event(s, TCP_SOCKET_ABORTED);
      relisten(s);
      return;
    }
    s->output_data_len -= s->output_data_send_nxt;
    s->output_senddata_len = s->output_data_len;
    s->output_data_send_nxt = 0;

    call_event(s, TCP_SOCKET_DATA_SENT);
    /* Unlike tcp-socket.c, there is no polling from "below", so
     * activate sending of any remaining data from here.
     */
    if (s->output_senddata_len > 0) {
      senddata(s);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
newdata(struct tcp_socket_gprs *s)
{
  uint16_t len, copylen, bytesleft;
  uint8_t *dataptr;
  len = uip_datalen();
  dataptr = uip_appdata;

  /* We have a segment with data coming in. We copy as much data as
     possible into the input buffer and call the input callback
     function. The input callback returns the number of bytes that
     should be retained in the buffer, or zero if all data should be
     consumed. If there is data to be retained, the highest bytes of
     data are copied down into the input buffer. */
  do {
    copylen = MIN(len, s->input_data_maxlen);
    memcpy(s->input_data_ptr, dataptr, copylen);
    if(s->input_callback) {
      bytesleft = s->input_callback(s, s->ptr,
				    s->input_data_ptr, copylen);
    } else {
      bytesleft = 0;
    }
    if(bytesleft > 0) {
      PRINTF("tcp: newdata, bytesleft > 0 (%d) not implemented\n", bytesleft);
    }
    dataptr += copylen;
    len -= copylen;

  } while(len > 0);
}
/*---------------------------------------------------------------------------*/
static void
relisten(struct tcp_socket_gprs *s)
{
  if(s != NULL && s->listen_port != 0) {
    s->flags |= TCP_SOCKET_FLAGS_LISTENING;
  }
}
/*---------------------------------------------------------------------------*/
static void
appcall(void *state)
{
  struct tcp_socket_gprs *s = state;

  if(s != NULL && s->c != NULL && s->c != uip_conn) {
    /* Safe-guard: this should not happen, as the incoming event relates to
     * a previous connection */
    return;
  }
  if(uip_connected()) {
    /* Check if this connection originated in a local listen
       socket. We do this by checking the state pointer - if NULL,
       this is an incoming listen connection. If so, we need to
       connect the socket to the uip_conn and call the event
       function. */
    if(s == NULL) {
      for(s = list_head(socketlist_gprs);
	  s != NULL;
	  s = list_item_next(s)) {
	if((s->flags & TCP_SOCKET_FLAGS_LISTENING) != 0 &&
	   s->listen_port != 0 &&
	   s->listen_port == uip_htons(uip_conn->lport)) {
	  s->flags &= ~TCP_SOCKET_FLAGS_LISTENING;
          s->output_data_max_seg = uip_mss();
	  tcp_markconn(uip_conn, s);
	  call_event(s, TCP_SOCKET_CONNECTED);
	  break;
	}
      }
    } else {
      s->output_data_max_seg = uip_mss();
      call_event(s, TCP_SOCKET_CONNECTED);
    }

    if(s == NULL) {
      uip_abort();
    } else {
      if(uip_newdata()) {
        newdata(s);
      }
      senddata(s);
    }
    return;
  }

  if(uip_timedout()) {
    call_event(s, TCP_SOCKET_TIMEDOUT);
    relisten(s);
  }

  if(uip_aborted()) {
    tcp_markconn(uip_conn, NULL);
    call_event(s, TCP_SOCKET_ABORTED);
    relisten(s);

  }

  if(s == NULL) {
    uip_abort();
    return;
  }

  if(uip_acked()) {
    acked(s);
  }
  if(uip_newdata()) {
    newdata(s);
  }

  if(uip_rexmit() ||
     uip_newdata() ||
     uip_acked()) {
    senddata(s);
  } else if(uip_poll()) {
    senddata(s);
  }

  if(s->output_data_len == 0 && s->flags & TCP_SOCKET_FLAGS_CLOSING) {
    s->flags &= ~TCP_SOCKET_FLAGS_CLOSING;
    uip_close();
    s->c = NULL;
    tcp_markconn(uip_conn, NULL);
    s->c = NULL;
    /*call_event(s, TCP_SOCKET_CLOSED);*/
    relisten(s);
  }

  if(uip_closed()) {
    tcp_markconn(uip_conn, NULL);
    s->c = NULL;
    call_event(s, TCP_SOCKET_CLOSED);
    relisten(s);
  }
}
/*---------------------------------------------------------------------------*/
static void 
gprs_callback() {
  return;
}
/*---------------------------------------------------------------------------*/
static void
gprs_input_callback(struct gprs_connection *gprsconn, void *callback_arg,
                   const uint8_t *input_data_ptr, int input_data_len)
{
  struct tcp_socket_gprs *s;
  uint16_t len, copylen, bytesleft;
  const uint8_t *dataptr;

  len = input_data_len;
  dataptr = input_data_ptr;
  printf("HERE IS newdata %d @0x%x: '", len, (unsigned) dataptr);

  /* We have a segment with data coming in. We copy as much data as
     possible into the input buffer and call the input callback
     function. The input callback returns the number of bytes that
     should be retained in the buffer, or zero if all data should be
     consumed. If there is data to be retained, the highest bytes of
     data are copied down into the input buffer. */
  s = callback_arg;
  if (0){
    int i;
    printf("newdata %d @0x%x: '", len, (unsigned) dataptr);
    for (i = 0; i < len; i++)
      printf("%c", (char ) dataptr[i]);
    printf("'\n");
  }
  do {
    copylen = MIN(len, s->input_data_maxlen);
    memcpy(s->input_data_ptr, dataptr, copylen);
    if(s->input_callback) {
      bytesleft = s->input_callback(s, s->ptr,
				    s->input_data_ptr, copylen);
    } else {
      bytesleft = 0;
    }
    if(bytesleft > 0) {
      printf("tcp: newdata, bytesleft > 0 (%d) not implemented\n", bytesleft);
    }
    dataptr += copylen;
    len -= copylen;

  } while(len > 0);

  return;
}
/*---------------------------------------------------------------------------*/
static void
gprs_event_callback(struct gprs_connection *gprsconn, void *callback_arg,
                    gprs_conn_event_t event)
{
  struct tcp_socket_gprs *s;
  switch (event) {
  case GPRS_CONN_CONNECTED:
    break;
  case GPRS_CONN_SOCKET_CLOSED:
    break;
  case GPRS_CONN_SOCKET_TIMEDOUT:
    break;
  case GPRS_CONN_ABORTED:
    break;
  case GPRS_CONN_DATA_SENT:
    s = (struct tcp_socket_gprs *) callback_arg;
    acked(s);
    break;
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(tcp_socket_gprs_process, ev, data)
{
  PROCESS_BEGIN();
  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == tcpip_event) {
      appcall(data);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  static uint8_t inited = 0;
  if(!inited) {
    list_init(socketlist_gprs);
    process_start(&tcp_socket_gprs_process, NULL);
    inited = 1;
  }
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_register(struct tcp_socket_gprs *s, void *ptr,
		    uint8_t *input_databuf, int input_databuf_len,
		    uint8_t *output_databuf, int output_databuf_len,
		    tcp_socket_gprs_data_callback_t input_callback,
		    tcp_socket_gprs_event_callback_t event_callback)
{
  struct gprs_connection *gprsconn;

  init();

  gprsconn = alloc_gprs_connection();
  if (gprsconn == NULL){
    return -1;
  }
  if(s == NULL) {
    return -1;
  }
  s->ptr = ptr;
  s->input_data_ptr = input_databuf;
  s->input_data_maxlen = input_databuf_len;
  s->output_data_len = 0;
  s->output_data_ptr = output_databuf;
  s->output_data_maxlen = output_databuf_len;
  s->output_senddata_len = 0;
  s->output_data_send_nxt = 0;

  s->input_callback = input_callback;
  s->event_callback = event_callback;

  s->listen_port = 0;
  s->flags = TCP_SOCKET_FLAGS_NONE;

  list_add(socketlist_gprs, s);
  if (gprs_register(gprsconn, s, gprs_callback,
                    gprs_input_callback, gprs_event_callback) < 0) {
    return -1;
  }
  else {
    s->g_c = gprsconn;
    return 1;
  }
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_connect_strhost(struct tcp_socket_gprs *s,
                                const char *host,
                                uint16_t port)
{
  if(s == NULL) {
    return -1;
  }
#if 0
  if(s->c != NULL) {
    tcp_markconn(s->c, NULL);
  }
#endif

  PROCESS_CONTEXT_BEGIN(&tcp_socket_gprs_process);
  //s->g_c = gprs_connection("TCP", host, uip_htons(port), s);
  gprs_connection(s->g_c, "TCP", host, uip_htons(port), s);
  PROCESS_CONTEXT_END();
  if(s->g_c == NULL) {
    return -1;
  } else {
    return 1;
  }
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_connect(struct tcp_socket_gprs *s,
                   const uip_ipaddr_t *ipaddr,
                   uint16_t port)
{
  char hoststr[sizeof("255.255.255.255")];
  uint8_t *hip4;
  
  printf("Here is GPRS_CONNECT (%d): ", sizeof(*ipaddr));
  {
    int i;
    for (i = 0; i < sizeof(*ipaddr); i++) {
      printf(" %02x", ((unsigned char *) (ipaddr))[i]);
    }
    printf("\n");
  }
  hip4 = ((uint8_t *) ipaddr)+12;
  snprintf(hoststr, sizeof(hoststr), "%u.%u.%u.%u", hip4[0], hip4[1], hip4[2], hip4[3]); 
  printf("hoststr %s\n", hoststr);
  return tcp_socket_gprs_connect_strhost(s, hoststr, port);
  return -1;  /* Not supported for hprs */
  /* Convert ipaddr to string and call tcp_socket_gprs_connect_strhost() */
  if(s == NULL) {
    return -1;
  }
#if 0
  if(s->c != NULL) {
    tcp_markconn(s->c, NULL);
  }
#endif

  PROCESS_CONTEXT_BEGIN(&tcp_socket_gprs_process);
  s->c = tcp_connect(ipaddr, uip_htons(port), s);
  PROCESS_CONTEXT_END();
  if(s->c == NULL) {
    return -1;
  } else {
    return 1;
  }
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_listen(struct tcp_socket_gprs *s,
           uint16_t port)
{
  if(s == NULL) {
    return -1;
  }

  s->listen_port = port;
  PROCESS_CONTEXT_BEGIN(&tcp_socket_gprs_process);
  tcp_listen(uip_htons(port));
  PROCESS_CONTEXT_END();
  s->flags |= TCP_SOCKET_FLAGS_LISTENING;
  return 1;
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_unlisten(struct tcp_socket_gprs *s)
{
  if(s == NULL) {
    return -1;
  }

  PROCESS_CONTEXT_BEGIN(&tcp_socket_gprs_process);
  tcp_unlisten(uip_htons(s->listen_port));
  PROCESS_CONTEXT_END();
  s->listen_port = 0;
  s->flags &= ~TCP_SOCKET_FLAGS_LISTENING;
  return 1;
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_send(struct tcp_socket_gprs *s,
                const uint8_t *data, int datalen)
{
  int len;
  
  if(s == NULL) {
    return -1;
  }

  printf("socket send. len %d maxlen %d curlen %d\n", datalen, s->output_data_maxlen, s->output_data_len);
  len = MIN(datalen, s->output_data_maxlen - s->output_data_len);

  memcpy(&s->output_data_ptr[s->output_data_len], data, len);
  s->output_data_len += len;

  if(s->output_senddata_len == 0) {
    s->output_senddata_len = s->output_data_len;
    senddata(s);
  }

  //gprs_send(s);
  //tcpip_poll_tcp(s->c);
  return len;
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_send_str(struct tcp_socket_gprs *s,
             const char *str)
{
  return tcp_socket_gprs_send(s, (const uint8_t *)str, strlen(str));
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_close(struct tcp_socket_gprs *s)
{
  if(s == NULL) {
    return -1;
  }

  gprs_close(s);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_unregister(struct tcp_socket_gprs *s)
{
  if(s == NULL) {
    return -1;
  }

  tcp_socket_gprs_unlisten(s);

#if 0
  if(s->c != NULL) {
    tcp_attach(s->c, NULL);
  }
#endif
  if (s->g_c != NULL) {
    gprs_unregister(s->g_c);
  }
  list_remove(socketlist_gprs, s);
  return 1;
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_max_sendlen(struct tcp_socket_gprs *s)
{
  return s->output_data_maxlen - s->output_data_len;
}
/*---------------------------------------------------------------------------*/
int
tcp_socket_gprs_queuelen(struct tcp_socket_gprs *s)
{
  return s->output_data_len;
}
/*---------------------------------------------------------------------------*/
