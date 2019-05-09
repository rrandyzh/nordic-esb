/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdk_common.h"
#include "nrf.h"
#include "nrf_esb.h"
#include "nrf_error.h"
#include "nrf_esb_error_codes.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "nrf_delay.h"
#include "app_util.h"
#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#include "app_config.h"
#include "app_common.h"
#include "nrf_drv_timer.h"

#define MODE_NORMAL					0
#define MODE_PAIRING				1

#define PAIR_STATE_NONE				0
#define PAIR_STATE_SEND_REQ			1
#define PAIR_STATE_WAIT_FOR_INFO	2

#define MAXIMUM_PAIRING_TIMEOUT_MS				60000UL	//1 min
#define BEACON_SCAN_SHORT_TIMEOUT_MS			(INTERVAL_TIMER_INTERVAL_10MS/10)
#define BEACON_SCAN_LONG_TIMEOUT_MS 			(INTERVAL_TIMER_INTERVAL_10MS/10 * (MAXIMUM_CHANNEL_LIST_SIZE + 1))

#define APP_PACKET_DELAY_US						350

typedef struct {
	
	uint32_t signature;
	uint8_t chlist[MAXIMUM_CHANNEL_LIST_SIZE];
	uint8_t sys_address_32[4];
	uint8_t dev_idx;
	
} ds_data_t;

void interval_timer_init(void);
void interval_timer_start(void);
void interval_timer_stop(void);
uint32_t esb_init(bool is_ptx);
void do_pairing(void);
void enter_normal_mode(void);

// 2 payload buffer system.
static nrf_esb_payload_t        tx_data_payload[] = {
													APP_CREATE_PAYLOAD(1, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
																		0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
																		0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
																		0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0),

													APP_CREATE_PAYLOAD(1, 0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21,
																		0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21,
																		0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21,
																		0x0f, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21)};	

static nrf_esb_payload_t	rx_payload;
																		
const uint8_t gca_pairing_chlist[MAXIMUM_CHANNEL_LIST_SIZE] = DEFAULT_PAIRING_CHANNEL_LIST;
uint8_t ga_chlist[MAXIMUM_CHANNEL_LIST_SIZE] = {0};			
uint8_t g_mode = MODE_NORMAL;
uint32_t g_pairing_timeout = 0;						
uint8_t g_cur_ch_idx = 0;
uint8_t g_base_addr_1[4];
ds_data_t g_ds = {.dev_idx = 0xff};
uint8_t g_dev_type = DEV_TYPE_DISPLAY;
uint8_t g_pair_state;
uint32_t g_scan_timeout = 0;
uint8_t g_cur_payload_idx = 0;
bool g_force_hop_channel = false;
uint32_t g_sync_timeout = 0;

void nrf_esb_error_handler(uint32_t err_code, uint32_t line)
{
    NRF_LOG_ERROR("App failed at line %d with error code: 0x%08x\r\n",
                   line, err_code);
	
#if DEBUG //lint -e553
    while (true);
#else
    NVIC_SystemReset();
#endif

}

#ifdef APP_ERROR_CHECK
#undef APP_ERROR_CHECK
#endif

#define APP_ERROR_CHECK(err_code) if (err_code) nrf_esb_error_handler(err_code, __LINE__);

static void hop_channel(){

	g_cur_ch_idx++;
	if(g_cur_ch_idx >= MAXIMUM_CHANNEL_LIST_SIZE){
		g_cur_ch_idx = 0;
	}
	while(!nrf_esb_is_idle());
	APP_ERROR_CHECK(nrf_esb_set_rf_channel(ga_chlist[g_cur_ch_idx]));
	
}

static bool is_beacon_packet(nrf_esb_payload_t *p_pkt){
	
	if(p_pkt->pipe != 0) return false;
	if(p_pkt->length != 10) return false;

	if(p_pkt->data[0] != BEACON_BYTE1) return false;
	if(p_pkt->data[1] != BEACON_BYTE2) return false;
	
	return true;
}

static void send_pairing_req(){

	nrf_esb_payload_t tx_pl;
	
	nrf_esb_flush_tx();

	tx_pl.data[0] = ID_PAIR_REQ;
	tx_pl.data[1] = g_dev_type;
	tx_pl.length = 2;
	tx_pl.pipe = 0;
	tx_pl.noack = false;
	
	nrf_esb_write_payload(&tx_pl);
	
}

static void send_get_info_req(){
	
	nrf_esb_payload_t tx_pl;
	
	nrf_esb_flush_tx();

	tx_pl.data[0] = ID_PAIR_INFO_GET;
	tx_pl.data[1] = 0;
	tx_pl.length = 2;
	tx_pl.pipe = 0;
	tx_pl.noack = false;
	
	nrf_esb_write_payload(&tx_pl);
	
}

static void send_device_data(bool is_retransmit){

	uint8_t idx = g_cur_payload_idx;
	
	nrf_esb_flush_tx();
	
	if(is_retransmit){
		if(idx == 0) idx = 1;
		else idx = 0;
	}
	tx_data_payload[idx].noack = false;
	nrf_esb_write_payload(&tx_data_payload[idx]);
	
	if(!is_retransmit){
		if(g_cur_payload_idx == 0) g_cur_payload_idx = 1;
		else g_cur_payload_idx = 0;
	}
	
	nrf_gpio_pin_clear(LED_2);
}

void nrf_esb_event_handler(nrf_esb_evt_t const * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:

			if(g_mode == MODE_PAIRING){
				
				switch(g_pair_state){
					
					case PAIR_STATE_SEND_REQ:
						//Get ACK from box. Delay a while and send get info packet to receive pair info.
						{
							g_pair_state = PAIR_STATE_WAIT_FOR_INFO;
							
							nrf_delay_us(800);
							send_get_info_req();
							
						}					
						break;
					
				}
			}
			else if(g_mode == MODE_NORMAL){
				
				//data packet sent successfully. 
				nrf_gpio_pin_set(LED_2);
				
				g_scan_timeout = BEACON_SCAN_SHORT_TIMEOUT_MS;
				hop_channel();
				esb_init(false);
				nrf_esb_start_rx();
				interval_timer_start();
			}
            break;
		
        case NRF_ESB_EVENT_TX_FAILED:
            
            (void) nrf_esb_flush_tx();
            
			if(g_mode == MODE_PAIRING){
			
				hop_channel();	
				
				if(g_pair_state == PAIR_STATE_WAIT_FOR_INFO){
					send_get_info_req();
				}else{
					send_pairing_req();
				}
			}
			else if(g_mode == MODE_NORMAL){
				
				//data packet sent failed. pulse LED_4 for 20us.
				nrf_gpio_pin_set(LED_2);

				g_scan_timeout = BEACON_SCAN_SHORT_TIMEOUT_MS;
				hop_channel();
				esb_init(false);
				nrf_esb_start_rx();
				interval_timer_start();

				nrf_gpio_pin_clear(LED_4);
				nrf_delay_us(20);
				nrf_gpio_pin_set(LED_4);
				
			}
            break;
        
		case NRF_ESB_EVENT_RX_RECEIVED:
            
			nrf_esb_read_rx_payload(&rx_payload);
		
			if(g_mode == MODE_PAIRING && g_pair_state == PAIR_STATE_WAIT_FOR_INFO){

				//Pair info received.
				if(rx_payload.length == sizeof(pair_info_t)){
					pair_info_t *p_pairInfo = (pair_info_t *)rx_payload.data;
					memcpy(g_ds.chlist, p_pairInfo->chlist, MAXIMUM_CHANNEL_LIST_SIZE);
					memcpy(g_ds.sys_address_32, p_pairInfo->system_address_32, 4);
					g_ds.dev_idx = p_pairInfo->dev_idx;
					
					ds_update((uint32_t*)&g_ds, sizeof(ds_data_t));
					
					enter_normal_mode();
				}
				
			}
			else if(g_mode == MODE_NORMAL){
				
				if(is_beacon_packet(&rx_payload)){
				
					//beacon received.
					g_sync_timeout = BEACON_SCAN_LONG_TIMEOUT_MS;
					
#if USE_SCHEME_2
					bool send_pkt = false;
					bool is_resend = false;
					
					if(rx_payload.data[2] == BEACON_BYTE3_NEW_DATA){
						
						//Got beacon to send new packet.
						send_pkt = true;
					}						
					else if((rx_payload.data[2] == BEACON_BYTE3_RESEND) && ((rx_payload.data[3] & (1 << (6 - g_ds.dev_idx))) == (1 << (6 - g_ds.dev_idx)))){
						
						//Got beacon to re-send previous packet. Toggle LED_3.
						send_pkt = true;
						is_resend = true;
						nrf_gpio_pin_toggle(LED_3);
					}
					
					if(send_pkt){
						interval_timer_stop();
						nrf_esb_stop_rx();
						esb_init(true);
						
						//delay for specific time before sending packet based on the device index.
						nrf_delay_us((APP_PACKET_DELAY_US) * (g_ds.dev_idx-1));
						send_device_data(is_resend);
					}
					else{
						//No need to resend packet. We will hop to next channel and scan next beacon.
						interval_timer_stop();
						nrf_esb_stop_rx();
						esb_init(false);
						
						g_scan_timeout = BEACON_SCAN_SHORT_TIMEOUT_MS;
						g_force_hop_channel = true;
						
						interval_timer_start();
					}						
#else				
							
					interval_timer_stop();
					nrf_esb_stop_rx();
					esb_init(true);
					
					//delay for specific time before sending packet based on the device index.
					nrf_delay_us((350) * (g_ds.dev_idx-1));
					send_device_data(false);
#endif					
				}
			}
				
            break;
		
    }
}

void interval_timer_event_handler(){
	
	if(g_scan_timeout || g_sync_timeout || g_force_hop_channel){
		
		if(g_scan_timeout){
			g_scan_timeout--;
		}
		
		if(g_sync_timeout){
			g_sync_timeout--;
		}

		if(g_scan_timeout == 0 || g_force_hop_channel){
			
			g_force_hop_channel = false;

			g_scan_timeout = BEACON_SCAN_SHORT_TIMEOUT_MS;
			if(g_sync_timeout == 0){
				//We lost sync. Switch to long scan interval.
				g_scan_timeout = BEACON_SCAN_LONG_TIMEOUT_MS;
			}
			nrf_esb_stop_rx();
			hop_channel();
			nrf_esb_start_rx();
		}
	}		
}

void enter_normal_mode(){

	//enter normal mode.
	nrf_gpio_pin_set(LED_1);
	
	g_mode = MODE_NORMAL;
	g_pairing_timeout = 0;
	g_pair_state = PAIR_STATE_NONE;
	
	interval_timer_stop();
	
	esb_init(false);
	nrf_esb_set_base_address_1(g_ds.sys_address_32);
	nrf_esb_update_prefix(1, g_ds.dev_idx);
	
	//change to system channel list.
	memcpy(ga_chlist, g_ds.chlist, MAXIMUM_CHANNEL_LIST_SIZE);
	g_cur_ch_idx = MAXIMUM_CHANNEL_LIST_SIZE;
	
	//Set to long scan interval (hold channel for more than 1 channel list cycle) to try and sync with the box.
	g_scan_timeout = BEACON_SCAN_LONG_TIMEOUT_MS;
	g_sync_timeout = 0;
	
	g_force_hop_channel = true;
	interval_timer_start();
	
}

void do_pairing(){
	
	//enter pairing mode
	nrf_gpio_pin_clear(LED_1);
	
	esb_init(true);
	
	//change to pairing channel list.
	memcpy(ga_chlist, gca_pairing_chlist, MAXIMUM_CHANNEL_LIST_SIZE);
	g_cur_ch_idx = 0;
	
	APP_ERROR_CHECK(nrf_esb_set_rf_channel(ga_chlist[g_cur_ch_idx]));
	
	g_pairing_timeout = MAXIMUM_PAIRING_TIMEOUT_MS;
	g_mode = MODE_PAIRING;
	g_pair_state = PAIR_STATE_SEND_REQ;
			
	//start sending pairing request.
	send_pairing_req();
}

void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}


void gpio_init( void )
{
	//LED to indicate current mode. 'L' => pairing mode, 'H' => normal mode.
	nrf_gpio_cfg_output(LED_1);
	nrf_gpio_pin_set(LED_1);

	//LED to indicate data packet transmission. 'L' => start Tx, 'H' => end Tx.
	nrf_gpio_cfg_output(LED_2);
	nrf_gpio_pin_set(LED_2);

	//LED to indicate data packet re-transmission. Toggle when received re-transmit beacon. (For scheme 2 only)
	nrf_gpio_cfg_output(LED_3);
	nrf_gpio_pin_set(LED_3);

	//LED to indicate data packet transmission failed. active low pulse for 20us.
	nrf_gpio_cfg_output(LED_4);
	nrf_gpio_pin_set(LED_4);
	
	//Press and hold BUTTON 1 during power up to activate pairing.
	nrf_gpio_cfg_input(BUTTON_1, NRF_GPIO_PIN_PULLUP);
	
	//Press and hold BUTTON 2 during power up to switch device type to "Controller".
	//**You need to press this together with BUTTON 1 so that the device type can be correctly registered to the box.
	nrf_gpio_cfg_input(BUTTON_2, NRF_GPIO_PIN_PULLUP);
	
}


uint32_t esb_init( bool is_ptx )
{
    uint32_t err_code;

    nrf_esb_config_t nrf_esb_config         = NRF_ESB_DEFAULT_CONFIG;
    nrf_esb_config.protocol                 = NRF_ESB_PROTOCOL_ESB_DPL;
	nrf_esb_config.retransmit_count			= 0;
    nrf_esb_config.bitrate                  = NRF_ESB_BITRATE_2MBPS;
    nrf_esb_config.event_handler            = nrf_esb_event_handler;
    nrf_esb_config.mode                     = (is_ptx ? NRF_ESB_MODE_PTX : NRF_ESB_MODE_PRX);
    nrf_esb_config.selective_auto_ack       = true;//false;

    err_code = nrf_esb_init(&nrf_esb_config);

    VERIFY_SUCCESS(err_code);

    return err_code;
}

void interval_timer_init(){

	//configure to 1ms timer.
	
	NRF_TIMER0->TASKS_CLEAR = 1;
	
    NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer;       // Set the timer in Timer Mode.
    NRF_TIMER0->PRESCALER   = 4;                           // Prescaler 4 produces 1000000 Hz timer frequency.
    NRF_TIMER0->BITMODE     = TIMER_BITMODE_BITMODE_24Bit; // 16 bit mode.
    NRF_TIMER0->CC[0]       = 1000UL;
	
	NRF_TIMER0->SHORTS		= TIMER_SHORTS_COMPARE0_CLEAR_Msk;
	NRF_TIMER0->INTENSET    = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
	
    NVIC_EnableIRQ(TIMER0_IRQn);
	
}

void interval_timer_start(){
	
	NRF_TIMER0->TASKS_CLEAR = 1;
	NRF_TIMER0->EVENTS_COMPARE[0] = 0;
	NRF_TIMER0->TASKS_START = 1;
}

void interval_timer_stop(){
	
	NRF_TIMER0->TASKS_STOP = 1;
}

int main(void)
{
    ret_code_t err_code;
    uint8_t base_addr_0[4] = DEFAULT_PAIRING_ADDRESS_32;
    uint8_t addr_prefix[2] = {PIPE_0_PREFIX, 0xff};

    gpio_init();

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    clocks_start();
	interval_timer_init();
	
	//Retrieve pairing info from flash if any.
	ds_get((uint32_t*)&g_ds, sizeof(ds_data_t));
	
	if(g_ds.signature != DS_SIGNATURE){
		
		g_ds.signature = DS_SIGNATURE;
	}
	
	if(nrf_gpio_pin_read(BUTTON_2) == 0){
		//set pairing device type info to controller type.
		g_dev_type = DEV_TYPE_CONTROLLER;
	}

    err_code = nrf_esb_set_base_address_0(base_addr_0);
    VERIFY_SUCCESS(err_code);

	err_code = nrf_esb_set_base_address_1(g_ds.sys_address_32);
	VERIFY_SUCCESS(err_code);
	
	if(g_ds.signature == DS_SIGNATURE && g_ds.dev_idx != 0xff){
		addr_prefix[1] = g_ds.dev_idx;
	}
	
    err_code = nrf_esb_set_prefixes(addr_prefix, g_ds.dev_idx == 0xff ? 1 : 2);
    VERIFY_SUCCESS(err_code);

	
	if(g_ds.dev_idx == 0xff || nrf_gpio_pin_read(BUTTON_1) == 0){
		do_pairing();
	}else{
		enter_normal_mode();
	}
	__enable_irq();
	interval_timer_start();
	
    while (true)
    {
    }
}

/*lint -restore */
