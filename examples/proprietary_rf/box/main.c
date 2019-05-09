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

#include "nrf_esb.h"

#include <stdbool.h>
#include <stdint.h>
#include "sdk_common.h"
#include "nrf.h"
#include "nrf_esb_error_codes.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_error.h"
#include "boards.h"
#include "nrf_drv_common.h"
#include "nrf_drv_timer.h"
#include "app_config.h"
#include "app_util_platform.h"
#include "nrf_nvmc.h"
#include "app_common.h"

#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#define MODE_NORMAL					0
#define MODE_CHANNEL_PICKING		1
#define MODE_PAIRING				2

#define MAXIMUM_PAIRING_TIMEOUT_MS				30000	//0.5 min

#define M_ESB_STOP_RX_WAIT_IDLE()				do{\
												if(!nrf_esb_is_idle()){\
												  if(nrf_esb_stop_rx() != NRF_SUCCESS){\
													  esb_init(false);\
												  }\
												}\
												}while(0)


typedef struct {
	
	uint32_t signature;
	uint8_t chlist[MAXIMUM_CHANNEL_LIST_SIZE];
	uint8_t display_slot_idx;
	uint8_t controller_slot_idx;
	
} ds_data_t;

/* function prototype */
void interval_timer_init(void);
void interval_timer_start(void);
void interval_timer_stop(void);
uint32_t esb_init( bool is_ptx );
void enter_setup_mode(void);
void enter_normal_mode(void);
void interval_timer_event_handler(void);

const uint8_t gca_pairing_chlist[MAXIMUM_CHANNEL_LIST_SIZE] = DEFAULT_PAIRING_CHANNEL_LIST;
#if USE_SCHEME_2
const uint8_t gca_available_chlist[MAXIMUM_CHANNEL_LIST_SIZE][MAXIMUM_CHANNELS_PER_REGION] = {
									REGION1_CHANNEL_LIST,
									REGION3_CHANNEL_LIST,
									REGION5_CHANNEL_LIST};
#else
const uint8_t gca_available_chlist[MAXIMUM_CHANNEL_LIST_SIZE][MAXIMUM_CHANNELS_PER_REGION] = {
									REGION1_CHANNEL_LIST,
									REGION2_CHANNEL_LIST,				
									REGION3_CHANNEL_LIST,
									REGION4_CHANNEL_LIST,
									REGION5_CHANNEL_LIST};
#endif
									
uint8_t ga_chlist[MAXIMUM_CHANNEL_LIST_SIZE] = {0};				
uint8_t g_mode = MODE_NORMAL;
bool g_esb_init = false;
uint32_t g_pairing_timeout = 0;						
uint8_t g_cur_ch_idx = 0;
static nrf_esb_payload_t  g_beacon = NRF_ESB_CREATE_PAYLOAD(0, BEACON_BYTE1, BEACON_BYTE2, 0xee, 0xdd, 0xee, 0xdd, 0xee, 0xdd, 0xee, 0xdd);
nrf_esb_payload_t rx_payload;
uint8_t g_base_addr_1[4];
ds_data_t g_ds;
uint8_t g_cur_pairing_dev_type;

#ifdef USE_SCHEME_2
uint8_t g_devs_paired_mask = 0;
uint8_t g_devs_data_recv_mask = 0;
#endif

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

/*lint -save -esym(40, BUTTON_1) -esym(40, BUTTON_2) -esym(40, BUTTON_3) -esym(40, BUTTON_4) -esym(40, LED_1) -esym(40, LED_2) -esym(40, LED_3) -esym(40, LED_4) */


static void hop_channel(){

	g_cur_ch_idx++;
	if(g_cur_ch_idx >= MAXIMUM_CHANNEL_LIST_SIZE){
		g_cur_ch_idx = 0;
	}
	M_ESB_STOP_RX_WAIT_IDLE();
	APP_ERROR_CHECK(nrf_esb_set_rf_channel(ga_chlist[g_cur_ch_idx]));
}

static void send_beacon(){
	
#if USE_SCHEME_2
	//finish 1 frame cycle and not received data from all paired device. Toggle LED_3.
	if(g_cur_ch_idx == 0 && (g_devs_data_recv_mask != g_devs_paired_mask)){
		nrf_gpio_pin_toggle(LED_3);
	}
		
	if(g_cur_ch_idx == 0){
		//New frame cycle. Send beacon to get new data from all paired devices.
		g_devs_data_recv_mask = 0;
		g_beacon.data[2] = BEACON_BYTE3_NEW_DATA;
	}
	else{
		//Send re-transmit beacon. Indicate those devices that have not yet receive their data packet.
		g_beacon.data[2] = BEACON_BYTE3_RESEND;
		g_beacon.data[3] = ~g_devs_data_recv_mask & g_devs_paired_mask;
	}
#endif
	
	g_beacon.noack = true;
	nrf_esb_write_payload(&g_beacon);
	nrf_gpio_pin_clear(LED_2);
}

void nrf_esb_event_handler(nrf_esb_evt_t const * p_event)
{
	switch(p_event->evt_id){
		
		case NRF_ESB_EVENT_TX_SUCCESS:

			nrf_gpio_pin_set(LED_2);
		
			(void) nrf_esb_flush_rx();
			(void) nrf_esb_flush_tx();		
		
			if(g_mode == MODE_NORMAL){
				//switch to PRX mode.
				esb_init(false);
				nrf_esb_start_rx();
			}
			break;
		
		case NRF_ESB_EVENT_TX_FAILED:

			(void) nrf_esb_flush_rx();
			(void) nrf_esb_flush_tx();		
		
			if(g_mode == MODE_NORMAL){
				//switch to PRX mode.
				esb_init(false);
				nrf_esb_start_rx();
			}
			
			break;
		
		case NRF_ESB_EVENT_RX_RECEIVED:
			
			if (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS)
            {
				nrf_esb_flush_rx();
				
				if(g_mode == MODE_PAIRING && rx_payload.length == 2){
					
					switch(rx_payload.data[0]){
						
						case ID_PAIR_REQ:
							{
								//Got a pairing request. Retrieve the device type and prepare pairing info.
								nrf_esb_payload_t pair_info;
								pair_info_t info;
								
								g_cur_pairing_dev_type = 0;
								
								memcpy(info.system_address_32, g_base_addr_1, 4);
								memcpy(info.chlist, g_ds.chlist, MAXIMUM_CHANNEL_LIST_SIZE);
								info.dev_idx = 0xff;
								
								if(rx_payload.data[1] == DEV_TYPE_DISPLAY){
									if(g_ds.display_slot_idx < MAXIMUM_DISPLAY_DEV){
										g_cur_pairing_dev_type = DEV_TYPE_DISPLAY;
										info.dev_idx = g_ds.display_slot_idx + 1;
									}
								}
								else if(rx_payload.data[1] == DEV_TYPE_CONTROLLER){
									if(g_ds.controller_slot_idx < MAXIMUM_CONTROLLER_DEV){
										g_cur_pairing_dev_type = DEV_TYPE_CONTROLLER;
										info.dev_idx = MAXIMUM_DISPLAY_DEV + g_ds.controller_slot_idx + 1;
									}
								}
								
								if(info.dev_idx != 0xff){
									
									//device slot is available. Set the pairing info as ack payload.
									memcpy(pair_info.data, (uint8_t *)&info, sizeof(pair_info_t));
									pair_info.length = sizeof(pair_info_t);
									pair_info.pipe = 0;

									nrf_esb_flush_tx();
									nrf_esb_write_payload(&pair_info);
								}
								
							}
							break;
						
						case ID_PAIR_INFO_GET:
						
							//Got the INFO_GET request. Pairing info is sent over to the device.
							//Increament the device type slot.
						
							nrf_esb_flush_tx();
						
							if(g_cur_pairing_dev_type == DEV_TYPE_DISPLAY && g_ds.display_slot_idx < MAXIMUM_DISPLAY_DEV){
								g_ds.display_slot_idx++;
#if USE_SCHEME_2								
								g_devs_paired_mask |= (uint8_t)(0x01 << (6 - g_ds.display_slot_idx));
#endif								
							}
							else if(g_cur_pairing_dev_type == DEV_TYPE_CONTROLLER && g_ds.controller_slot_idx < MAXIMUM_CONTROLLER_DEV){
								g_ds.controller_slot_idx++;
#if USE_SCHEME_2								
								g_devs_paired_mask |= (uint8_t)(0x01 << (2 - g_ds.controller_slot_idx));
#endif								
							}
							g_cur_pairing_dev_type = 0;
							
							if(g_ds.controller_slot_idx == MAXIMUM_CONTROLLER_DEV && g_ds.display_slot_idx == MAXIMUM_DISPLAY_DEV){
								
								ds_update((uint32_t *)&g_ds, sizeof(ds_data_t));
								enter_normal_mode();	
								return;								
							}
							break;
						
					}
				}
				else if(g_mode == MODE_NORMAL){
					
					if(rx_payload.pipe != 0 && rx_payload.length == 32){
#if USE_SCHEME_2						
						g_devs_data_recv_mask |= (uint8_t)(0x01 << (6 - rx_payload.pipe));
#endif						
					}
				}
				
            }
			
			break;
	}
	
}

void interval_timer_event_handler(){
	
	hop_channel();
	
	//If new frame, toggle LED_4.
	
#if USE_SCHEME_2
	if(g_cur_ch_idx == 0){
		nrf_gpio_pin_toggle(LED_4);
	}
#else
	nrf_gpio_pin_toggle(LED_4);
#endif	
	
	if(g_mode == MODE_NORMAL){
	
		//send beacon
		esb_init(true);
		send_beacon();
	}		
	else if(g_mode == MODE_PAIRING){
		
		nrf_esb_start_rx();
	}
}

static void host_chip_id_read(uint8_t *dst)
{
	uint8_t i;
	
	uint32_t dev_id = NRF_FICR->DEVICEID[1];

	for(i = 0; i < 4; i++){
		dst[i] = (dev_id >> (i * 8)) & 0xff;
	}
}
  
 

#define MAXIMUM_RSSI_SAMPLES		256

void do_channel_list_generation(){
	
	uint32_t packet;
	uint16_t samples, highest_rssi, rssi[MAXIMUM_CHANNELS_PER_REGION];
	uint8_t i, k, selected_ch;
	
	NRF_RADIO->PACKETPTR = (uint32_t)&packet;
	
	for(i = 0; i < MAXIMUM_CHANNEL_LIST_SIZE; i++){
		
		for(k = 0; k < MAXIMUM_CHANNELS_PER_REGION; k++){
			rssi[k] = 0;
		}
		
		highest_rssi = 0;
		
		for(samples = 0; samples < MAXIMUM_RSSI_SAMPLES; samples++){
		
			for(k = 0; k < MAXIMUM_CHANNELS_PER_REGION; k++){
				
				NRF_RADIO->FREQUENCY = gca_available_chlist[i][k];
				NRF_RADIO->EVENTS_READY = 0U;
				NRF_RADIO->TASKS_RXEN = 1U;
				while(NRF_RADIO->EVENTS_READY == 0U);
				NRF_RADIO->EVENTS_END = 0U;

				NRF_RADIO->TASKS_START = 1U;

				NRF_RADIO->EVENTS_RSSIEND = 0U;
				NRF_RADIO->TASKS_RSSISTART = 1U;
				while(NRF_RADIO->EVENTS_RSSIEND == 0U);

				rssi[k] += NRF_RADIO->RSSISAMPLE;

				NRF_RADIO->EVENTS_DISABLED = 0U;    
				NRF_RADIO->TASKS_DISABLE   = 1U;  // Disable the radio.

				while(NRF_RADIO->EVENTS_DISABLED == 0U)
				{
						// Do nothing.
				}
				
				if(highest_rssi < rssi[k]){
					highest_rssi = rssi[k];
					selected_ch = gca_available_chlist[i][k];
				}
			}
		}

		g_ds.chlist[i] = selected_ch;
	}
}

void enter_normal_mode(){
	
	//enter normal mode
	g_pairing_timeout = 0;

	interval_timer_stop();
	
	//change to system channel list.
	memcpy(ga_chlist, g_ds.chlist, MAXIMUM_CHANNEL_LIST_SIZE);
	g_mode = MODE_NORMAL;
	g_cur_ch_idx = MAXIMUM_CHANNEL_LIST_SIZE;
	
	interval_timer_start();
	
	nrf_gpio_pin_set(LED_1);
	
}
void enter_setup_mode(){

	uint32_t err_code;
	
	//enter setup mode.
	//1) channel picking
	//2) pairing.
	
	g_mode = MODE_CHANNEL_PICKING;

	do_channel_list_generation();
	
    err_code = esb_init(false);
    APP_ERROR_CHECK(err_code);
	
	memcpy(ga_chlist, gca_pairing_chlist, MAXIMUM_CHANNEL_LIST_SIZE);
	g_cur_ch_idx = 0;
	
	APP_ERROR_CHECK(nrf_esb_set_rf_channel(ga_chlist[g_cur_ch_idx]));
	
#ifdef USE_SCHEME_2	
	g_devs_paired_mask = 0;
#endif	
	g_pairing_timeout = MAXIMUM_PAIRING_TIMEOUT_MS;
	g_mode = MODE_PAIRING;

	g_ds.controller_slot_idx = 0;
	g_ds.display_slot_idx = 0;

    err_code = nrf_esb_start_rx();
    APP_ERROR_CHECK(err_code);

	interval_timer_start();

	nrf_gpio_pin_clear(LED_1);
	
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
	
	//LED to indicate beacon transmission. 'L' => start Tx, 'H' => end Tx.
	nrf_gpio_cfg_output(LED_2);
	nrf_gpio_pin_set(LED_2);

	//LED to indicate loss packet after 1 frame cycle. Toggle if not receive all packets after 1 frame cycle.
	nrf_gpio_cfg_output(LED_3);
	nrf_gpio_pin_set(LED_3);

	//LED to indicate 1 frame cycle. Toggle every beginning of new frame.
	nrf_gpio_cfg_output(LED_4);
	nrf_gpio_pin_set(LED_4);
	
	//Press and hold BUTTON 1 to activate pairing.
	nrf_gpio_cfg_input(BUTTON_1, NRF_GPIO_PIN_PULLUP);
}


uint32_t esb_init( bool is_ptx )
{
    uint32_t err_code;
	
    nrf_esb_config_t nrf_esb_config         = NRF_ESB_DEFAULT_CONFIG;
	nrf_esb_config.retransmit_count			= 0;
    nrf_esb_config.protocol                 = NRF_ESB_PROTOCOL_ESB_DPL;
    nrf_esb_config.bitrate                  = NRF_ESB_BITRATE_2MBPS;
    nrf_esb_config.mode                     = (is_ptx == true ? NRF_ESB_MODE_PTX : NRF_ESB_MODE_PRX);
    nrf_esb_config.event_handler            = nrf_esb_event_handler;
    nrf_esb_config.selective_auto_ack       = true;//is_ptx;

	nrf_esb_disable();
	
    err_code = nrf_esb_init(&nrf_esb_config);
    VERIFY_SUCCESS(err_code);

    return err_code;
}

void interval_timer_init(){

	//configure to interval 
	NRF_TIMER0->TASKS_CLEAR = 1;
	
    NRF_TIMER0->MODE        = TIMER_MODE_MODE_Timer;       // Set the timer in Timer Mode.
    NRF_TIMER0->PRESCALER   = 4;                           // Prescaler 4 produces 1000000 Hz timer frequency.
    NRF_TIMER0->BITMODE     = TIMER_BITMODE_BITMODE_24Bit; // 16 bit mode.
    NRF_TIMER0->CC[0]       = INTERVAL_TIMER_INTERVAL_10MS * 100UL;
	
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
    uint32_t err_code;
	bool force_setup = false;
    uint8_t base_addr_0[4] = DEFAULT_PAIRING_ADDRESS_32;
    uint8_t addr_prefix[7] = {PIPE_0_PREFIX, 1, 2, 3, 4, 5, 6};

    gpio_init();

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    clocks_start();
	interval_timer_init();

	host_chip_id_read(g_base_addr_1);
	ds_get((uint32_t *)&g_ds, sizeof(ds_data_t));
	
	if(g_ds.signature != DS_SIGNATURE){
		
		g_ds.signature = DS_SIGNATURE;
		force_setup = true;
	}
	else{
		
		if(g_ds.controller_slot_idx == 0 && g_ds.display_slot_idx == 0){
			force_setup = true;
		}
#ifdef USE_SCHEME_2		
		else
		{
			uint8_t i;
			g_devs_paired_mask = 0;
			
			for(i=0; i<g_ds.display_slot_idx; i++){
				g_devs_paired_mask |= (uint8_t)(0x01 << (5 - i));
			}
			for(i=0; i<g_ds.controller_slot_idx; i++){
				g_devs_paired_mask |= (uint8_t)(0x01 << (1 - i));
			}
		}
#endif		
	}
	
    err_code = nrf_esb_set_base_address_0(base_addr_0);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_1(g_base_addr_1);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_prefixes(addr_prefix, 7);
    VERIFY_SUCCESS(err_code);
	
	if(force_setup || nrf_gpio_pin_read(BUTTON_1) == 0){
		enter_setup_mode();
	}
	else{
		enter_normal_mode();
	}		
	
    __enable_irq();
	
    while (true)
    {
    }
}

/*lint -restore */

