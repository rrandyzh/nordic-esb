#ifndef _APP_CONFIG_H_
#define _APP_CONFIG_H_

#include "nrf_esb.h"

#define USE_SCHEME_2							1

#define MAXIMUM_CHANNELS_PER_REGION				10

#if USE_SCHEME_2
#define DS_SIGNATURE							0x12345678
#define MAXIMUM_CHANNEL_LIST_SIZE				3
#define DEFAULT_PAIRING_CHANNEL_LIST			{2, 48, 76}
#define INTERVAL_TIMER_INTERVAL_10MS			40UL
#define MAXIMUM_RETRY_COUNT						2
#else
#define DS_SIGNATURE							0x87654321
#define MAXIMUM_CHANNEL_LIST_SIZE				5
#define DEFAULT_PAIRING_CHANNEL_LIST			{2, 21, 48, 53, 76}
#define INTERVAL_TIMER_INTERVAL_10MS			40UL
#endif

#define REGION1_CHANNEL_LIST					{1, 3, 4, 5, 6, 7, 8, 9, 10, 12}
#define REGION2_CHANNEL_LIST					{14, 16, 17, 18, 20, 23, 24, 25, 27, 29}
#define REGION3_CHANNEL_LIST					{32, 35, 36, 39, 41, 42, 44, 46, 50, 52}
#define REGION4_CHANNEL_LIST					{51, 52, 54, 55, 57, 59, 61, 62, 63, 65}
#define REGION5_CHANNEL_LIST					{67, 68, 69, 70, 71, 72, 73, 74, 77, 78}

#define DEFAULT_PAIRING_ADDRESS_32				{0x12, 0x34, 0x67, 0x98}

#define PIPE_0_PREFIX							0x88

#define ID_PAIR_REQ								0x1A
#define ID_PAIR_INFO_GET						0x1B

#define DEV_TYPE_DISPLAY						1
#define DEV_TYPE_CONTROLLER						2

#define MAXIMUM_DISPLAY_DEV						4
#define MAXIMUM_CONTROLLER_DEV					2

#define BEACON_BYTE1							0xee
#define BEACON_BYTE2							0xdd

#if USE_SCHEME_2
#define BEACON_BYTE3_NEW_DATA					0x01
#define BEACON_BYTE3_RESEND						0x02
#endif

#define APP_CREATE_PAYLOAD(_pipe, ...)        {.pipe = _pipe, .length = NUM_VA_ARGS(__VA_ARGS__), .data = {__VA_ARGS__}}       


typedef struct {
	
	uint8_t system_address_32[4];
	uint8_t  chlist[MAXIMUM_CHANNEL_LIST_SIZE];
	uint8_t  dev_idx;
	
} pair_info_t;

#endif
