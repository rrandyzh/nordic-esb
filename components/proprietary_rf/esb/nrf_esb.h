/* Copyright (c) 2016 Nordic Semiconductor. All Rights Reserved.
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

#ifndef __NRF_ESB_H
#define __NRF_ESB_H

#include <stdbool.h>
#include <stdint.h>
#include "nrf.h"
#include "app_util.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup nrf_esb Enhanced ShockBurst
 * @{
 * @ingroup proprietary_api
 *
 * @brief Enhanced ShockBurst (ESB) is a basic protocol that supports two-way data
 *        packet communication including packet buffering, packet acknowledgment,
 *        and automatic retransmission of lost packets.
 */

#define DEBUGPIN1   12
#define DEBUGPIN2   13
#define DEBUGPIN3   14
#define DEBUGPIN4   15


#ifdef  NRF_ESB_DEBUG
#define DEBUG_PIN_SET(a)    (NRF_GPIO->OUTSET = (1 << (a)))
#define DEBUG_PIN_CLR(a)    (NRF_GPIO->OUTCLR = (1 << (a)))
#else
#define DEBUG_PIN_SET(a)
#define DEBUG_PIN_CLR(a)
#endif


// Hardcoded parameters - change if necessary
#ifndef NRF_ESB_MAX_PAYLOAD_LENGTH
#define     NRF_ESB_MAX_PAYLOAD_LENGTH          32                  /**< The maximum size of the payload. Valid values are 1 to 252. */
#endif

#define     NRF_ESB_TX_FIFO_SIZE                8                   /**< The size of the transmission first-in, first-out buffer. */
#define     NRF_ESB_RX_FIFO_SIZE                8                   /**< The size of the reception first-in, first-out buffer. */

// 252 is the largest possible payload size according to the nRF5 architecture.
STATIC_ASSERT(NRF_ESB_MAX_PAYLOAD_LENGTH <= 252);

#define     NRF_ESB_SYS_TIMER                   NRF_TIMER2          /**< The timer that is used by the module. */
#define     NRF_ESB_SYS_TIMER_IRQ_Handler       TIMER2_IRQHandler   /**< The handler that is used by @ref NRF_ESB_SYS_TIMER. */

#define     NRF_ESB_PPI_TIMER_START             10                  /**< The PPI channel used for timer start. */
#define     NRF_ESB_PPI_TIMER_STOP              11                  /**< The PPI channel used for timer stop. */
#define     NRF_ESB_PPI_RX_TIMEOUT              12                  /**< The PPI channel used for RX time-out. */
#define     NRF_ESB_PPI_TX_START                13                  /**< The PPI channel used for starting TX. */

// Interrupt flags
#define     NRF_ESB_INT_TX_SUCCESS_MSK          0x01                /**< The flag used to indicate a success since the last event. */
#define     NRF_ESB_INT_TX_FAILED_MSK           0x02                /**< The flag used to indicate a failure since the last event. */
#define     NRF_ESB_INT_RX_DR_MSK               0x04                /**< The flag used to indicate that a packet was received since the last event. */

#define     NRF_ESB_PID_RESET_VALUE             0xFF                /**< Invalid PID value that is guaranteed to not collide with any valid PID value. */
#define     NRF_ESB_PID_MAX                     3                   /**< The maximum value for PID. */
#define     NRF_ESB_CRC_RESET_VALUE             0xFFFF              /**< The CRC reset value. */

#define ESB_EVT_IRQ        SWI0_IRQn                                /**< The ESB event IRQ number when running on an nRF5x device. */
#define ESB_EVT_IRQHandler SWI0_IRQHandler                          /**< The handler for @ref ESB_EVT_IRQ when running on an nRF5x device. */

/** Default address configuration for ESB. Roughly equal to the nRF24Lxx default (except for the number of pipes, because more pipes are supported). */
#define NRF_ESB_ADDR_DEFAULT                                                    \
{                                                                               \
    .base_addr_p0       = { 0xE7, 0xE7, 0xE7, 0xE7 },                           \
    .base_addr_p1       = { 0xC2, 0xC2, 0xC2, 0xC2 },                           \
    .pipe_prefixes      = { 0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 },   \
    .addr_length        = 5,                                                    \
    .num_pipes          = 8,                                                    \
    .rf_channel         = 2,                                                    \
    .rx_pipes_enabled   = 0xFF                                                  \
}


/** Default radio parameters. Roughly equal to the nRF24Lxx default parameters (except for CRC, which is set to 16 bit, and protocol, which is set to DPL). */
#define NRF_ESB_DEFAULT_CONFIG {.protocol               = NRF_ESB_PROTOCOL_ESB_DPL,         \
                                .mode                   = NRF_ESB_MODE_PTX,                 \
                                .event_handler          = 0,                                \
                                .bitrate                = NRF_ESB_BITRATE_2MBPS,            \
                                .crc                    = NRF_ESB_CRC_16BIT,                \
                                .tx_output_power        = NRF_ESB_TX_POWER_0DBM,            \
                                .retransmit_delay       = 250,                              \
                                .retransmit_count       = 0,                                \
                                .tx_mode                = NRF_ESB_TXMODE_AUTO,              \
                                .radio_irq_priority     = 1,                                \
                                .event_irq_priority     = 2,                                \
                                .payload_length         = 32,                               \
                                .selective_auto_ack     = false                             \
}


/** Default legacy radio parameters, identical to the nRF24Lxx defaults. */
#define NRF_ESB_LEGACY_CONFIG  {.protocol               = NRF_ESB_PROTOCOL_ESB,             \
                                .mode                   = NRF_ESB_MODE_PTX,                 \
                                .event_handler          = 0,                                \
                                .bitrate                = NRF_ESB_BITRATE_2MBPS,            \
                                .crc                    = NRF_ESB_CRC_8BIT,                 \
                                .tx_output_power        = NRF_ESB_TX_POWER_0DBM,            \
                                .retransmit_delay       = 600,                              \
                                .retransmit_count       = 3,                                \
                                .tx_mode                = NRF_ESB_TXMODE_AUTO,              \
                                .radio_irq_priority     = 1,                                \
                                .event_irq_priority     = 2,                                \
                                .payload_length         = 32,                               \
                                .selective_auto_ack     = false                             \
}


/**Macro to create an initializer for a TX data packet.
 *
 * @details This macro generates an initializer. Using the initializer is more efficient
 *          than setting the individual parameters dynamically.
 *
 * @param[in]   _pipe   The pipe to use for the data packet.
 * @param[in]   ...     Comma separated list of character data to put in the TX buffer.
 *                      Supported values consist of 1 to 63 characters.
 *
 * @return  Initializer that sets up the pipe, length, and byte array for content of the TX data.
 */
#define NRF_ESB_CREATE_PAYLOAD(_pipe, ...)                                                  \
        {.pipe = _pipe, .length = NUM_VA_ARGS(__VA_ARGS__), .data = {__VA_ARGS__}};         \
        STATIC_ASSERT(NUM_VA_ARGS(__VA_ARGS__) > 0 && NUM_VA_ARGS(__VA_ARGS__) <= 63)


/**@brief Enhanced ShockBurst protocols. */
typedef enum {
    NRF_ESB_PROTOCOL_ESB,      /*< Enhanced ShockBurst with fixed payload length.                                            */
    NRF_ESB_PROTOCOL_ESB_DPL   /*< Enhanced ShockBurst with dynamic payload length.                                          */
} nrf_esb_protocol_t;


/**@brief Enhanced ShockBurst modes. */
typedef enum {
    NRF_ESB_MODE_PTX,          /*< Primary transmitter mode. */
    NRF_ESB_MODE_PRX           /*< Primary receiver mode.    */
} nrf_esb_mode_t;


/**@brief Enhanced ShockBurst bitrate modes. */
typedef enum {
    NRF_ESB_BITRATE_2MBPS     = RADIO_MODE_MODE_Nrf_2Mbit,      /**< 2 Mb radio mode.                                                */
    NRF_ESB_BITRATE_1MBPS     = RADIO_MODE_MODE_Nrf_1Mbit,      /**< 1 Mb radio mode.                                                */
    NRF_ESB_BITRATE_250KBPS   = RADIO_MODE_MODE_Nrf_250Kbit,    /**< 250 Kb radio mode.                                              */
    NRF_ESB_BITRATE_1MBPS_BLE = RADIO_MODE_MODE_Ble_1Mbit       /**< 1 Mb radio mode using @e Bluetooth low energy radio parameters. */
} nrf_esb_bitrate_t;


/**@brief Enhanced ShockBurst CRC modes. */
typedef enum {
    NRF_ESB_CRC_16BIT = RADIO_CRCCNF_LEN_Two,                   /**< Use two-byte CRC. */
    NRF_ESB_CRC_8BIT  = RADIO_CRCCNF_LEN_One,                   /**< Use one-byte CRC. */
    NRF_ESB_CRC_OFF   = RADIO_CRCCNF_LEN_Disabled               /**< Disable CRC.      */
} nrf_esb_crc_t;


/**@brief Enhanced ShockBurst radio transmission power modes. */
typedef enum {
    NRF_ESB_TX_POWER_4DBM     = RADIO_TXPOWER_TXPOWER_Pos4dBm,  /**< 4 dBm radio transmit power.   */
    NRF_ESB_TX_POWER_0DBM     = RADIO_TXPOWER_TXPOWER_0dBm,     /**< 0 dBm radio transmit power.   */
    NRF_ESB_TX_POWER_NEG4DBM  = RADIO_TXPOWER_TXPOWER_Neg4dBm,  /**< -4 dBm radio transmit power.  */
    NRF_ESB_TX_POWER_NEG8DBM  = RADIO_TXPOWER_TXPOWER_Neg8dBm,  /**< -8 dBm radio transmit power.  */
    NRF_ESB_TX_POWER_NEG12DBM = RADIO_TXPOWER_TXPOWER_Neg12dBm, /**< -12 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG16DBM = RADIO_TXPOWER_TXPOWER_Neg16dBm, /**< -16 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG20DBM = RADIO_TXPOWER_TXPOWER_Neg20dBm, /**< -20 dBm radio transmit power. */
    NRF_ESB_TX_POWER_NEG30DBM = RADIO_TXPOWER_TXPOWER_Neg30dBm  /**< -30 dBm radio transmit power. */
} nrf_esb_tx_power_t;


/**@brief Enhanced ShockBurst transmission modes. */
typedef enum {
    NRF_ESB_TXMODE_AUTO,        /*< Automatic TX mode: When the TX FIFO contains packets and the radio is idle, packets are sent automatically. */
    NRF_ESB_TXMODE_MANUAL,      /*< Manual TX mode: Packets are not sent until @ref nrf_esb_start_tx is called. This mode can be used to ensure consistent packet timing. */
    NRF_ESB_TXMODE_MANUAL_START /*< Manual start TX mode: Packets are not sent until @ref nrf_esb_start_tx is called. Then, transmission continues automatically until the TX FIFO is empty. */
} nrf_esb_tx_mode_t;


/**@brief Enhanced ShockBurst event IDs used to indicate the type of the event. */
typedef enum
{
    NRF_ESB_EVENT_TX_SUCCESS,   /**< Event triggered on TX success.     */
    NRF_ESB_EVENT_TX_FAILED,    /**< Event triggered on TX failure.     */
    NRF_ESB_EVENT_RX_RECEIVED   /**< Event triggered on RX received.    */
} nrf_esb_evt_id_t;


/**@brief Enhanced ShockBurst payload.
 *
 * @details The payload is used both for transmissions and for acknowledging a
 *          received packet with a payload.
*/
typedef struct
{
    uint8_t length;                                 /**< Length of the packet (maximum value is NRF_ESB_MAX_PAYLOAD_LENGTH). */
    uint8_t pipe;                                   /**< Pipe used for this payload. */
    int8_t  rssi;                                   /**< RSSI for the received packet. */
    uint8_t noack;                                  /**< Flag indicating that this packet will not be acknowledged. */
    uint8_t pid;                                    /**< PID assigned during communication. */
    uint8_t data[NRF_ESB_MAX_PAYLOAD_LENGTH];       /**< The payload data. */
} nrf_esb_payload_t;


/**@brief Enhanced ShockBurst event. */
typedef struct
{
    nrf_esb_evt_id_t    evt_id;                     /**< Enhanced ShockBurst event ID. */
    uint32_t            tx_attempts;                /**< Number of TX retransmission attempts. */
} nrf_esb_evt_t;


/**@brief Definition of the event handler for the module. */
typedef void (* nrf_esb_event_handler_t)(nrf_esb_evt_t const * p_event);


/**@brief Main configuration structure for the module. */
typedef struct
{
    nrf_esb_protocol_t      protocol;               /**< Enhanced ShockBurst protocol. */
    nrf_esb_mode_t          mode;                   /**< Enhanced ShockBurst mode. */
    nrf_esb_event_handler_t event_handler;          /**< Enhanced ShockBurst event handler. */

    // General RF parameters
    nrf_esb_bitrate_t       bitrate;                /**< Enhanced ShockBurst bitrate mode. */
    nrf_esb_crc_t           crc;                    /**< Enhanced ShockBurst CRC modes. */

    nrf_esb_tx_power_t      tx_output_power;        /**< Enhanced ShockBurst radio transmission power mode.*/

    uint16_t                retransmit_delay;       /**< The delay between each retransmission of unacknowledged packets. */
    uint16_t                retransmit_count;       /**< The number of retransmissions attempts before transmission fail. */

    // Control settings
    nrf_esb_tx_mode_t       tx_mode;                /**< Enhanced ShockBurst transmission mode. */

    uint8_t                 radio_irq_priority;     /**< nRF radio interrupt priority. */
    uint8_t                 event_irq_priority;     /**< ESB event interrupt priority. */
    uint8_t                 payload_length;         /**< Length of the payload (maximum length depends on the platforms that are used on each side). */

    bool                    selective_auto_ack;     /**< Enable or disable selective auto acknowledgment. */
} nrf_esb_config_t;


/**@brief Function for initializing the Enhanced ShockBurst module.
 *
 * @param  p_config     Parameters for initializing the module.
 *
 * @retval  NRF_SUCCESS             If initialization was successful.
 * @retval  NRF_ERROR_NULL          If the @p p_config argument was NULL.
 * @retval  NRF_ERROR_BUSY          If the function failed because the radio is busy.
 */
uint32_t nrf_esb_init(nrf_esb_config_t const * p_config);


/**@brief Function for suspending the Enhanced ShockBurst module.
 *
 * Calling this function stops ongoing communications without changing the queues.
 *
 * @retval  NRF_SUCCESS             If Enhanced ShockBurst was suspended.
 * @retval  NRF_ERROR_BUSY          If the function failed because the radio is busy.
 */
uint32_t nrf_esb_suspend(void);


/**@brief Function for disabling the Enhanced ShockBurst module.
 *
 * Calling this function disables the Enhanced ShockBurst module immediately.
 * Doing so might stop ongoing communications.
 *
 * @note All queues are flushed by this function.
 *
 * @retval  NRF_SUCCESS             If Enhanced ShockBurst was disabled.
 */
uint32_t nrf_esb_disable(void);


/**@brief Function for checking if the Enhanced ShockBurst module is idle.
 *
 * @retval true                     If the module is idle.
 * @retval false                    If the module is busy.
 */
bool nrf_esb_is_idle(void);


/**@brief Function for writing a payload for transmission or acknowledgment.
 *
 * This function writes a payload that is added to the queue. When the module is in PTX mode, the
 * payload is queued for a regular transmission. When the module is in PRX mode, the payload
 * is queued for when a packet is received that requires an acknowledgment with payload.
 *
 * @param[in]   p_payload     Pointer to the structure that contains information and state of the payload.
 *
 * @retval  NRF_SUCCESS                     If the payload was successfully queued for writing.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 * @retval  NRF_ERROR_NOT_SUPPORTED         If @p p_payload->noack was false, but selective acknowledgment is not enabled.
 * @retval  NRF_ERROR_NO_MEM                If the TX FIFO is full.
 * @retval  NRF_ERROR_INVALID_LENGTH        If the payload length was invalid (zero or larger than the allowed maximum).
 */
uint32_t nrf_esb_write_payload(nrf_esb_payload_t const * p_payload);


/**@brief Function for reading an RX payload.
 *
 * @param[in,out]   p_payload   Pointer to the structure that contains information and state of the payload.
 *
 * @retval  NRF_SUCCESS                     If the data was read successfully.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
uint32_t nrf_esb_read_rx_payload(nrf_esb_payload_t * p_payload);


/**@brief Function for starting transmission.
 *
 * @retval  NRF_SUCCESS                     If the TX started successfully.
 * @retval  NRF_ERROR_BUFFER_EMPTY          If the TX does not start because the FIFO buffer is empty.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 */
uint32_t nrf_esb_start_tx(void);


/**@brief Function for starting to transmit data from the FIFO buffer.
 *
 * @retval  NRF_SUCCESS                     If the transmission was started successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 */
uint32_t nrf_esb_start_rx(void);


/** @brief Function for stopping data reception.
 *
 * @retval  NRF_SUCCESS                     If data reception was stopped successfully.
 * @retval  NRF_ESB_ERROR_NOT_IN_RX_MODE    If the function failed because the module is not in RX mode.
 */
uint32_t nrf_esb_stop_rx(void);


/**@brief Function for removing remaining items from the TX buffer.
 *
 * This function clears the TX FIFO buffer.
 *
 * @retval  NRF_SUCCESS                     If pending items in the TX buffer were successfully cleared.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
uint32_t nrf_esb_flush_tx(void);


/**@brief Function for removing the first item from the TX buffer.
 *
 * @retval  NRF_SUCCESS                     If the operation completed successfully.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 * @retval  NRF_ERROR_BUFFER_EMPTY          If there are no items in the queue to remove.
 */
uint32_t nrf_esb_pop_tx(void);


/**@brief Function for removing remaining items from the RX buffer.
 *
 * @retval  NRF_SUCCESS                     If the pending items in the RX buffer were successfully cleared.
 * @retval  NRF_INVALID_STATE               If the module is not initialized.
 */
uint32_t nrf_esb_flush_rx(void);



/**@brief Function for setting the length of the address.
 *
 * @param[in]       length              Length of the ESB address (in bytes).
 *
 * @retval  NRF_SUCCESS                      If the address length was set successfully.
 * @retval  NRF_ERROR_INVALID_PARAM          If the address length was invalid.
 * @retval  NRF_ERROR_BUSY                   If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_address_length(uint8_t length);


/**@brief Function for setting the base address for pipe 0.
 *
 * @param[in]       p_addr      Pointer to the address data.
 *
 * @retval  NRF_SUCCESS                     If the base address was set successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 */
uint32_t nrf_esb_set_base_address_0(uint8_t const * p_addr);


/**@brief Function for setting the base address for pipe 1 to pipe 7.
 *
 * @param[in]       p_addr      Pointer to the address data.
 *
 * @retval  NRF_SUCCESS                     If the base address was set successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_NULL                  If the required parameter was NULL.
 */
uint32_t nrf_esb_set_base_address_1(uint8_t const * p_addr);


/**@brief Function for setting the number of pipes and the pipe prefix addresses.
 *
 * This function configures the number of available pipes, enables the pipes,
 * and sets their prefix addresses.
 *
 * @param[in]   p_prefixes      Pointer to a char array that contains the prefix for each pipe.
 * @param[in]   num_pipes       Number of pipes.
 *
 * @retval  NRF_SUCCESS                     If the prefix addresses were set successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 * @retval  NRF_ERROR_NULL                  If a required parameter was NULL.
 * @retval  NRF_ERROR_INVALID_PARAM         If an invalid number of pipes was given.
 */
uint32_t nrf_esb_set_prefixes(uint8_t const * p_prefixes, uint8_t num_pipes);


/**@brief Function for enabling pipes.
 *
 * The @p enable_mask parameter must contain the same number of pipes as has been configured
 * with @ref nrf_esb_set_prefixes.
 *
 * @param   enable_mask         Bitfield mask to enable or disable pipes. Setting a bit to
 *                              0 disables the pipe. Setting a bit to 1 enables the pipe.
 *
 * @retval  NRF_SUCCESS                     If the pipes were enabled and disabled successfully.
 * @retval  NRF_ERROR_BUSY                  If the function failed because the radio is busy.
 */
uint32_t nrf_esb_enable_pipes(uint8_t enable_mask);


/**@brief Function for updating the prefix for a pipe.
 *
 * @param   pipe    Pipe for which to set the prefix.
 * @param   prefix  Prefix to set for the pipe.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 * @retval  NRF_ERROR_INVALID_PARAM             If the given pipe number was invalid.
 */
uint32_t nrf_esb_update_prefix(uint8_t pipe, uint8_t prefix);


/** @brief Function for setting the channel to use for the radio.
 *
 * The module must be in an idle state to call this function. As a PTX, the
 * application must wait for an idle state and as a PRX, the application must stop RX
 * before changing the channel. After changing the channel, operation can be resumed.
 *
 * @param[in]   channel                         Channel to use for radio.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_INVALID_STATE                   If the module is not initialized.
 * @retval  NRF_ERROR_BUSY                      If the module was not in idle state.
 * @retval  NRF_ERROR_INVALID_PARAM             If the channel is invalid (larger than 125).
 */
uint32_t nrf_esb_set_rf_channel(uint32_t channel);


/**@brief Function for getting the current radio channel.
 *
 * @param[in, out] p_channel    Pointer to the channel data.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_NULL                      If the required parameter was NULL.
 */
uint32_t nrf_esb_rf_channel_get(uint32_t * p_channel);


/**@brief Function for setting the radio output power.
 *
 * @param[in]   tx_output_power    Output power.
 *
 * @retval  NRF_SUCCESS                         If the operation completed successfully.
 * @retval  NRF_ERROR_BUSY                      If the function failed because the radio is busy.
 */
uint32_t nrf_esb_set_tx_power(nrf_esb_tx_power_t tx_output_power);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // NRF_ESB
