// Copyright 2025 @ GosuDRM
// SPDX-License-Identifier: GPL-2.0-or-later
#include "ansi.h"
#include "uart.h"  // qmk uart.h
#include "rf_driver.h"

USART_MGR_STRUCT Usart_Mgr;
#define RX_SBYTE    Usart_Mgr.RXDBuf[0]
#define RX_CMD      Usart_Mgr.RXDBuf[1]
#define RX_ACK      Usart_Mgr.RXDBuf[2]
#define RX_LEN      Usart_Mgr.RXDBuf[3]
#define RX_DAT      Usart_Mgr.RXDBuf[4]

extern volatile bool f_uart_ack;
extern volatile bool f_rf_read_data_ok;
extern volatile bool f_rf_sts_sysc_ok;
extern volatile bool f_rf_new_adv_ok;
extern volatile bool f_rf_reset;
extern volatile bool f_rf_hand_ok;
extern volatile bool f_goto_sleep;
extern volatile bool f_wakeup_prepare;

uint8_t  uart_bit_report_buf[NKRO_REPORT_BITS + 1] = {0};
uint8_t  func_tab[32]                              = {0};
uint8_t  bitkb_report_buf[NKRO_REPORT_BITS + 1]    = {0};
uint8_t  bytekb_report_buf[8]    = {0};
uint16_t conkb_report            = 0;
uint16_t syskb_report            = 0;
uint8_t  sync_lost               = 0;
uint8_t  disconnect_delay        = 0;

extern DEV_INFO_STRUCT dev_info;
extern host_driver_t  *m_host_driver;
extern uint8_t         host_mode;
extern uint8_t         rf_blink_cnt;
extern uint16_t        rf_link_show_time;
extern uint16_t        rf_linking_time;
extern uint16_t        no_act_time;
extern bool            f_send_channel;
extern bool            f_dial_sw_init_ok;

report_mouse_t mousekey_get_report(void);
void           uart_init(uint32_t baud); // qmk uart.c
void           uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size);
void           UART_Send_Bytes(uint8_t *Buffer, uint32_t Length);
uint8_t        get_checksum(uint8_t *buf, uint8_t len);
void           uart_receive_pro(void);
void           m_break_all_key(void);
uint16_t       host_last_consumer_usage(void);

/**
 * @brief Uart auto nkey send
 */
bool f_bit_kb_act = 0;
static void uart_auto_nkey_send(uint8_t *pre_bit_report, uint8_t *now_bit_report, uint8_t size)
{
    uint8_t i, j, byte_index;
    uint8_t change_mask, offset_mask;
    uint8_t key_code = 0;
    bool f_byte_send = 0, f_bit_send = 0;

    uart_bit_report_buf[0] = now_bit_report[0];

    if (pre_bit_report[0] ^ now_bit_report[0]) {
        bytekb_report_buf[0] = now_bit_report[0];
        f_byte_send          = 1;
        if (f_bit_kb_act) {
            f_bit_send = 1;
        }
    }

    for (i = 1; i < size; i++) {
        change_mask = pre_bit_report[i] ^ now_bit_report[i];
        offset_mask = 1;
        for (j = 0; j < 8; j++) {
            if (change_mask & offset_mask) {
                if (now_bit_report[i] & offset_mask) {
                    for (byte_index = 2; byte_index < 8; byte_index++) {
                        if (bytekb_report_buf[byte_index] == 0) {
                            bytekb_report_buf[byte_index] = key_code;
                            f_byte_send                   = 1;
                            break;
                        }
                    }
                    if (byte_index >= 8) {
                        uart_bit_report_buf[i] |= offset_mask;
                        f_bit_send = 1;
                    }
                } else {
                    for (byte_index = 2; byte_index < 8; byte_index++) {
                        if (bytekb_report_buf[byte_index] == key_code) {
                            bytekb_report_buf[byte_index] = 0;
                            f_byte_send                   = 1;
                            break;
                        }
                    }
                    if (byte_index >= 8) {
                        uart_bit_report_buf[i] &= ~offset_mask;
                        f_bit_send = 1;
                    }
                }
            }
            key_code++;
            offset_mask <<= 1;
        }
    }

    if (f_bit_send) {
        f_bit_kb_act = 1;
        uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
    }

    if (f_byte_send) {
        uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);
    }
}


/**
 * @brief  Uart send keys report.
 */
void uart_send_report_func(void)
{
    static uint32_t interval_timer = 0;

    if (dev_info.link_mode == LINK_USB) return;
    keyboard_protocol          = 1;

    if (timer_elapsed32(interval_timer) > 100) {
        interval_timer = timer_read32();
        if (no_act_time <= 200) {
            uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);
            wait_us(200);

            if(f_bit_kb_act)
            uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
        }
        else {
            f_bit_kb_act = 0;
        }
    }
}

/**
 * @brief  Uart send consumer keys report.
 * @note Call in rf_driver.c
 */
void uart_send_consumer_report(report_extra_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_CONSUME, (uint8_t *)(&report->usage), 2);
}

/**
 * @brief  Uart send mouse keys report.
 * @note Call in rf_driver.c
 */
void uart_send_mouse_report(report_mouse_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_MS, &report->buttons, 5);
}

/**
 * @brief  Uart send system keys report.
 * @note Call in rf_driver.c
 */
void uart_send_system_report(report_extra_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_SYS, (uint8_t *)(&report->usage), 2);
}

/**
 * @brief  Uart send byte keys report.
 * @note Call in rf_driver.c
 */
void uart_send_report_keyboard(report_keyboard_t *report) {
    no_act_time      = 0;
    report->reserved = 0;
    uart_send_report(CMD_RPT_BYTE_KB, &report->mods, 8);
    memcpy(bytekb_report_buf, &report->mods, 8);
}

/**
 * @brief  Uart send bit keys report.
 * @note Call in rf_driver.c
 */
void uart_send_report_nkro(report_nkro_t *report) {
    no_act_time = 0;
    uart_auto_nkey_send(bitkb_report_buf, &report->mods, NKRO_REPORT_BITS + 1);
    memcpy(&bitkb_report_buf[0], &report->mods, NKRO_REPORT_BITS + 1);
}

/**
 * @brief  Parsing the data received from the RF module.
 */
void RF_Protocol_Receive(void) {
    uint8_t i, check_sum = 0;

    if (Usart_Mgr.RXDState == RX_Done) {
        f_uart_ack = 1;
        sync_lost = 0;

        if (Usart_Mgr.RXDLen > 4) {
            if((Usart_Mgr.RXDLen - 5) != RX_LEN) 
                return;

            /* Bounds check: RX_LEN comes from the RF module; reject if it
               would cause us to read past the end of RXDBuf. */
            if (RX_LEN > UART_MAX_LEN - 5) {
                Usart_Mgr.RXDState = RX_DATA_OV;
                return;
            }

            for (i = 0; i < RX_LEN; i++)
                check_sum += Usart_Mgr.RXDBuf[4 + i];

            if (check_sum != Usart_Mgr.RXDBuf[4 + i]) {
                Usart_Mgr.RXDState = RX_SUM_ERR;
                return;
            }
        } else if (Usart_Mgr.RXDLen == 3) {
            if (Usart_Mgr.RXDBuf[2] == 0xA0) {
                f_uart_ack = 1;
            }
            else {
                return;
            }
        } else {
            return;
        }

        switch (RX_CMD) {
            case CMD_HAND: {
                f_rf_hand_ok = 1;
                break;
            }

            case CMD_24G_SUSPEND: {
                f_goto_sleep = 1;
                break;
            }

            case CMD_NEW_ADV: {
                f_rf_new_adv_ok = 1;
                break;
            }

            case CMD_RF_STS_SYSC: {
                static uint8_t error_cnt = 0;

                if (dev_info.link_mode == Usart_Mgr.RXDBuf[4]) {
                    error_cnt = 0;

                    dev_info.rf_state = Usart_Mgr.RXDBuf[5];

                    if ((dev_info.rf_state == RF_CONNECT) && ((Usart_Mgr.RXDBuf[6] & 0xf8) == 0)) {
                        dev_info.rf_led = Usart_Mgr.RXDBuf[6];
                    }

                    dev_info.rf_charge = Usart_Mgr.RXDBuf[7];

                    if (Usart_Mgr.RXDBuf[8] <= 100) dev_info.rf_baterry = Usart_Mgr.RXDBuf[8];
                }
                else {
                    if (dev_info.rf_state != RF_INVALID) {
                        if (error_cnt >= 5) {
                            error_cnt      = 0;
                            f_send_channel = 1;
                        } else {
                            error_cnt++;
                        }
                    }
                }

                f_rf_sts_sysc_ok = 1;
                break;
            }

            case CMD_READ_DATA: {
                memcpy(func_tab, &Usart_Mgr.RXDBuf[4], 32);

                if (func_tab[4] <= LINK_USB) {
                    dev_info.link_mode = func_tab[4];
                }

                if (func_tab[5] < LINK_USB) {
                    dev_info.rf_channel = func_tab[5];
                }

                if ((func_tab[6] <= LINK_BT_3) && (func_tab[6] >= LINK_BT_1)) {
                    dev_info.ble_channel = func_tab[6];
                }

                f_rf_read_data_ok = 1;
                break;
            }
            default:
                break;
        }

        Usart_Mgr.RXDLen      = 0;
        Usart_Mgr.RXDState    = RX_Idle;
        Usart_Mgr.RXDOverTime = 0;
    }
}

/**
 * @brief  Uart send cmd.
 * @param  cmd: cmd.
 * @param  wait_ack: wait time for ack after sending.
 * @param  delayms: delay before sending.
 */
uint8_t uart_send_cmd(uint8_t cmd, uint8_t wait_ack, uint8_t delayms) {
    wait_ms(delayms);

    memset(&Usart_Mgr.TXDBuf[0], 0, UART_MAX_LEN);

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = cmd;
    Usart_Mgr.TXDBuf[2] = 0x00;

    switch (cmd) {
        case CMD_SLEEP:
        case CMD_HAND: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_RF_STS_SYSC: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;
            break;
        }

        case CMD_SET_LINK: {
            dev_info.rf_state   = RF_LINKING;
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;

            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            break;
        }

        case CMD_NEW_ADV: {
            dev_info.rf_state   = RF_PAIRING;
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = 1;
            Usart_Mgr.TXDBuf[6] = dev_info.link_mode + 1;

            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            f_rf_new_adv_ok  = 0;
            break;
        }

        case CMD_CLR_DEVICE: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_SET_CONFIG: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = POWER_DOWN_DELAY;
            Usart_Mgr.TXDBuf[5] = POWER_DOWN_DELAY;
            break;
        }

        case CMD_SET_NAME: {
            Usart_Mgr.TXDBuf[3]  = 17;
            Usart_Mgr.TXDBuf[4]  = 1;
            Usart_Mgr.TXDBuf[5]  = 15;
            Usart_Mgr.TXDBuf[6]  = 'N';
            Usart_Mgr.TXDBuf[7]  = 'u';
            Usart_Mgr.TXDBuf[8]  = 'P';
            Usart_Mgr.TXDBuf[9]  = 'h';
            Usart_Mgr.TXDBuf[10] = 'y';
            Usart_Mgr.TXDBuf[11] = ' ';
            Usart_Mgr.TXDBuf[12] = 'A';
            Usart_Mgr.TXDBuf[13] = 'i';
            Usart_Mgr.TXDBuf[14] = 'r';
            Usart_Mgr.TXDBuf[15] = '9';
            Usart_Mgr.TXDBuf[16] = '6';
            Usart_Mgr.TXDBuf[17] = ' ';
            Usart_Mgr.TXDBuf[18] = 'V';
            Usart_Mgr.TXDBuf[19] = '2';
            Usart_Mgr.TXDBuf[20] = '-';
            /* Checksum computed by generic write after the switch */
            break;
        }

        case CMD_SET_24G_NAME: {
            /* UTF-16LE encoded name for 2.4G dongle */
            static const uint8_t name_24g[] = {
                'N', 0, 'u', 0, 'P', 0, 'h', 0, 'y', 0, ' ', 0,
                'A', 0, 'i', 0, 'r', 0, '9', 0, '6', 0, ' ', 0,
                'V', 0, '2', 0, ' ', 0, 'D', 0, 'o', 0, 'n', 0,
                'g', 0, 'l', 0, 'e', 0
            };
            Usart_Mgr.TXDBuf[3] = 44;  // uart data len
            Usart_Mgr.TXDBuf[4] = 44;  // name valid len
            Usart_Mgr.TXDBuf[5] = 3;   // fixed field
            memcpy(&Usart_Mgr.TXDBuf[6], name_24g, sizeof(name_24g));
            /* Checksum computed by generic write after the switch */
            break;
        }

        case CMD_READ_DATA: {
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = 0x00;
            Usart_Mgr.TXDBuf[5] = FUNC_VALID_LEN;
            Usart_Mgr.TXDBuf[6] = FUNC_VALID_LEN;
            break;
        }

        case CMD_RF_DFU: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        default:
            break;
    }

    f_uart_ack = 0;
    Usart_Mgr.TXDBuf[4 + Usart_Mgr.TXDBuf[3]] = get_checksum(&Usart_Mgr.TXDBuf[4], Usart_Mgr.TXDBuf[3]);
    UART_Send_Bytes(Usart_Mgr.TXDBuf, Usart_Mgr.TXDBuf[3] + 5);

    if (wait_ack) {
        while (wait_ack--) {
            wait_ms(1);
            if (f_uart_ack) return TX_OK;
        }
    } else {
        return TX_OK;
    }

    return TX_TIMEOUT;
}

/**
 * @brief RF module state sync.
 */
void dev_sts_sync(void) {
    static uint32_t interval_timer      = 0;
    static uint8_t  link_state_temp     = RF_DISCONNECT;
    static uint8_t  reset_cooldown      = 0;
    static uint8_t  usb_sync_prescaler  = 0;

    if (timer_elapsed32(interval_timer) < 200)
        return;
    else
        interval_timer = timer_read32();

    /* Post-reset cooldown: skip sync_lost counting to give NRF time to boot */
    if (reset_cooldown > 0) {
        reset_cooldown--;
    }

    if (f_rf_reset) {
        f_rf_reset = 0;
        wait_ms(100);
        writePinLow(NRF_RESET_PIN);
        wait_ms(50);
        writePinHigh(NRF_RESET_PIN);
        wait_ms(50);
        reset_cooldown = 3;  /* ~600ms cooldown (3 × 200ms intervals) */
    }
    else if (f_send_channel) {
        f_send_channel = 0;
        uart_send_cmd(CMD_SET_LINK, 10, 5);
    }

    if (dev_info.link_mode == LINK_USB) {
        if (host_mode != HOST_USB_TYPE) {
            host_mode = HOST_USB_TYPE;
            host_set_driver(m_host_driver);
            m_break_all_key();
        }
        rf_blink_cnt = 0;
    }
    else {
        if (host_mode != HOST_RF_TYPE) {
            host_mode = HOST_RF_TYPE;
            m_break_all_key();
            host_set_driver(&rf_host_driver);
        }

        if (dev_info.rf_state != RF_CONNECT) {
            if (disconnect_delay >= 10) {
                if (link_state_temp != dev_info.rf_state) {
                    rf_blink_cnt    = 3;
                    rf_link_show_time = 0;
                    link_state_temp = dev_info.rf_state;
                }
            } else {
                disconnect_delay++;
            }
        }
        else if (dev_info.rf_state == RF_CONNECT) {
            rf_linking_time  = 0;
            disconnect_delay = 0;
            rf_blink_cnt     = 0;

            if (link_state_temp != RF_CONNECT) {
                link_state_temp   = RF_CONNECT;
                rf_link_show_time = 0;
                if (dev_info.link_mode == LINK_RF_24) {
                    uart_send_cmd(CMD_SET_24G_NAME, 10, 5);
                }
            }
        }
    }

    /* In USB mode, only sync every ~2 seconds (10 × 200ms) for battery info.
       In RF mode, sync every 200ms for connection state. */
    if (dev_info.link_mode == LINK_USB) {
        if (++usb_sync_prescaler >= 10) {
            usb_sync_prescaler = 0;
            uart_send_cmd(CMD_RF_STS_SYSC, 1, 1);
        }
    } else {
        usb_sync_prescaler = 0;
        uart_send_cmd(CMD_RF_STS_SYSC, 1, 1);
    }

    if (dev_info.link_mode != LINK_USB && reset_cooldown == 0) {
        if (++sync_lost >= 10) {
            sync_lost  = 0;
            f_rf_reset = 1;
        }
    }
}

/**
 * @brief Uart send bytes.
 * @param Buffer data buf
 * @param Length data length
 */
void UART_Send_Bytes(uint8_t *Buffer, uint32_t Length) {
    writePinLow(NRF_WAKEUP_PIN);
    wait_us(50);

    uart_transmit(Buffer, Length);

    wait_us(50 + Length * 32);
    writePinHigh(NRF_WAKEUP_PIN);
}

/**
 * @brief get checksum.
 * @param buf data buf
 * @param len data length
 */
uint8_t get_checksum(uint8_t *buf, uint8_t len) {
    uint8_t i;
    uint8_t checksum = 0;

    for (i = 0; i < len; i++)
        checksum += *buf++;

    checksum ^= UART_HEAD;

    return checksum;
}

/**
 * @brief Uart send report.
 * @param report_type  report_type
 * @param report_buf  report_buf
 * @param report_size  report_size
 */
void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size) {
    if (f_dial_sw_init_ok == 0) return;
    if (dev_info.link_mode == LINK_USB) return;
    /* Let RF module handle its own state -- don't drop reports during pairing/linking. */

    /* If waking from sleep, power up RF module before sending the first report.
       Avoids the race where the report hits the UART before Sleep_Handle wakes the NRF.
       Note: we do NOT clear f_wakeup_prepare here — Sleep_Handle() owns the full
       wake sequence including USB wakeup signaling (fix for C3 race condition). */
    if (f_wakeup_prepare) {
        writePinHigh(DC_BOOST_PIN);
        writePinHigh(RGB_DRIVER_SDB1);
        writePinHigh(RGB_DRIVER_SDB2);
        uart_send_cmd(CMD_HAND, 0, 1);
    }

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = report_type;
    Usart_Mgr.TXDBuf[2] = 0x01;
    Usart_Mgr.TXDBuf[3] = report_size;

    memcpy(&Usart_Mgr.TXDBuf[4], report_buf, report_size);
    Usart_Mgr.TXDBuf[4 + report_size] = get_checksum(&Usart_Mgr.TXDBuf[4], report_size);

    UART_Send_Bytes(&Usart_Mgr.TXDBuf[0], report_size + 5);
}

/**
 * @brief Uart receives data and processes it after completion,.
 */
void uart_receive_pro(void) {
    static uint32_t rx_timeout_timer = 0;

    /* Timeout recovery: if we started receiving a packet but no new bytes
       arrive within 10ms, discard the partial packet and reset the parser.
       Prevents the parser from hanging on UART noise. */
    if (Usart_Mgr.RXDLen > 0 && !uart_available()) {
        if (timer_elapsed32(rx_timeout_timer) > 10) {
            Usart_Mgr.RXDLen   = 0;
            Usart_Mgr.RXDState = RX_Idle;
        }
        return;
    }

    while (uart_available()) {
        rx_timeout_timer = timer_read32();
        uint8_t b = uart_read();

        if (Usart_Mgr.RXDLen == 0) {
            if (b == UART_HEAD) {
                Usart_Mgr.RXDBuf[0] = b;
                Usart_Mgr.RXDLen = 1;
            }
        }
        else if (Usart_Mgr.RXDLen == 1) {
            Usart_Mgr.RXDBuf[1] = b;
            Usart_Mgr.RXDLen = 2;
        }
        else if (Usart_Mgr.RXDLen == 2) {
            Usart_Mgr.RXDBuf[2] = b;
            Usart_Mgr.RXDLen = 3;
            if (b == 0xA0) {
                Usart_Mgr.RXDState = RX_Done;
                RF_Protocol_Receive();
                Usart_Mgr.RXDLen = 0;
                Usart_Mgr.RXDState = RX_Idle;
            }
        }
        else if (Usart_Mgr.RXDLen == 3) {
            Usart_Mgr.RXDBuf[3] = b;
            Usart_Mgr.RXDLen = 4;
        }
        else {
            if (Usart_Mgr.RXDLen < UART_MAX_LEN) {
                Usart_Mgr.RXDBuf[Usart_Mgr.RXDLen++] = b;
                if (Usart_Mgr.RXDLen == 5 + Usart_Mgr.RXDBuf[3]) {
                    Usart_Mgr.RXDState = RX_Done;
                    RF_Protocol_Receive();
                    Usart_Mgr.RXDLen = 0;
                    Usart_Mgr.RXDState = RX_Idle;
                }
            }
            else {
                Usart_Mgr.RXDLen = 0;
                Usart_Mgr.RXDState = RX_Idle;
            }
        }
    }
}

/**
 * @brief  RF uart initial.
 */
void rf_uart_init(void) {
    /* set uart buad as 460800 */
    uart_init(460800);

    /* Enable parity check */
    USART1->CR1 &= ~((uint32_t)USART_CR1_UE);
    USART1->CR1 |= USART_CR1_M0 | USART_CR1_PCE;
    USART1->CR1 |= USART_CR1_UE;

    /* set Rx and Tx pin pull up */
    /* FIXME: GPIO_OSPEEDER_OSPEEDR6 -- verify this macro exists in the target ChibiOS HAL.
       The correct name may be GPIO_OSPEEDR_OSPEEDR6 (without the extra "ER"). */
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR7_0);
}

/**
 * @brief RF module initial.
 */
/**
 * @brief Send a command and poll for response. Returns true if ok_flag was set.
 */
static bool rf_init_try_cmd(uint8_t cmd, volatile bool *ok_flag) {
    uint8_t timeout = 10;
    *ok_flag = 0;
    while (timeout--) {
        uart_send_cmd(cmd, 0, 5);
        uart_receive_pro();
        uart_receive_pro();
        if (*ok_flag) return true;
    }
    return false;
}

/**
 * @brief RF module initial.
 */
void rf_device_init(void) {
    rf_init_try_cmd(CMD_HAND,        &f_rf_hand_ok);
    rf_init_try_cmd(CMD_READ_DATA,   &f_rf_read_data_ok);
    rf_init_try_cmd(CMD_RF_STS_SYSC, &f_rf_sts_sysc_ok);

    uart_send_cmd(CMD_SET_NAME, 10, 5);

    uart_send_cmd(CMD_SET_24G_NAME, 10, 5);
}
