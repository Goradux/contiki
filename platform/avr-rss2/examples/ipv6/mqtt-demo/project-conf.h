/*
 * Copyright (c) 2012, Texas Instruments Incorporated - http://www.ti.com/
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
 *
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
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup cc2538-mqtt-demo
 * @{
 *
 * \file
 * Project specific configuration defines for the MQTT demo
 */
/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* User configuration */
#define MQTT_DEMO_STATUS_LED      LEDS_YELLOW
#define MQTT_DEMO_PUBLISH_TRIGGER &button_right_sensor

#define MQTT_WATCHDOG
#define MQTT_DEMO_TOPIC_BASE 	"KTH/avr-rss2"

/* If undefined, the demo will attempt to connect to IBM's quickstart */
//#define MQTT_DEMO_BROKER_IP_ADDR "aaaa::1"
//#define MQTT_DEMO_BROKER_IP_ADDR "::ffff:c010:7dea" 
//#define MQTT_DEMO_BROKER_IP_ADDR "::ffff:c010:7dea" 
//#define MQTT_DEMO_BROKER_IP_ADDR "0064:ff9b:0000:0000:0000:0000:c010:7dea"
#define MQTT_DEMO_BROKER_IP_ADDR "FD02::1"
#define MQTT_CONF_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)

#define NETSTACK_CONF_RDC nullrdc_driver
#define NETSTACK_CONF_MAC nullmac_driver

//#define NETSTACK_CONF_MAC         csma_driver
//#define NETSTACK_CONF_RDC         contikimac_driver
#define NETSTACK_CONF_FRAMER      framer_802154
#define NETSTACK_CONF_RADIO       rf230_driver

#define BOARD_STRING  "greeniot"

#define RS232_BAUDRATE USART_BAUD_38400

#define RPL_CONF_ACCEPT_DEFAULT_INSTANCE_ONLY 1
#define RPL_CONF_DEFAULT_INSTANCE 0x1d
#define IEEE802154_CONF_PANID 0xFEED
#define CHANNEL_CONF_802_15_4 25
#define NULLRDC_CONF_802154_AUTOACK_HW  1

#define RPL_CONF_STATS 1

/* PMSx003 sensors -- I2C and/or UART*/
#define PMS_CONF_SERIAL_I2C 1
#define PMS_CONF_SERIAL_UART 0

/* cli config */
#define CLI_CONF_COMMAND_PROMPT  "KTH-MQTT> "
#define CLI_CONF_PROJECT  "GreenIoT V1.0 2017-03-13"


/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */