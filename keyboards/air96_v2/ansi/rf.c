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
extern volatile bool f_send_channel;
extern volatile bool f_dial_sw_init_ok;

/* Recovery reports belong to the channel on which they were captured. */
static uint8_t reset_phase;
static bool rf_recovering;
static uint8_t pending_cmd;
static bool waiting_ack;
/* 32 complete taps fit during a short radio reset (press + release). */
#define RF_QUEUE_SIZE 64
#define RF_REPLAY_MAX_AGE_MS 1000
static struct { uint8_t type, size, data[16]; uint32_t captured_at; } rf_queue[RF_QUEUE_SIZE];
static uint8_t queue_head, queue_count;
static uint8_t latest_reports[5][16];
static const uint8_t report_sizes[5] = {5, 8, 16, 2, 2};
static uint32_t replay_timer;
static void rf_replay_task(void);

/* Old taps expire; current states still release any keys held by the host. */
static void rf_queue_resync(void) {
    queue_head = 0;
    queue_count = 5;
    for (uint8_t i = 0; i < 5; i++) {
        rf_queue[i].type = CMD_RPT_MS + i;
        rf_queue[i].size = report_sizes[i];
        rf_queue[i].captured_at = timer_read32();
        memcpy(rf_queue[i].data, latest_reports[i], report_sizes[i]);
    }
}

void rf_reset_sync_lost(void) {
    sync_lost = 0;
    f_rf_reset = false;
    reset_phase = 0;
    rf_recovering = false;
    queue_head = queue_count = 0;
    memset(latest_reports, 0, sizeof(latest_reports));
    writePinHigh(NRF_RESET_PIN);
    Usart_Mgr.RXDLen = 0;
    Usart_Mgr.RXDState = RX_Idle;
}

report_mouse_t mousekey_get_report(void);
void           uart_init(uint32_t baud); // qmk uart.c
void    uart_send_report(uint8_t report_type, const uint8_t *report_buf, uint8_t report_size);
static void    UART_Send_Bytes(const uint8_t *Buffer, uint32_t Length);
static uint8_t get_checksum(const uint8_t *buf, uint8_t len);
void           uart_receive_pro(void);
void           m_break_all_key(void);
uint16_t       host_last_consumer_usage(void);

/**
 * @brief Uart auto nkey send
 */
bool f_bit_kb_act = false;
static void uart_auto_nkey_send(const uint8_t *pre_bit_report, const uint8_t *now_bit_report, uint8_t size)
{
    /* The dongle HID descriptor exposes usages 0..111. The UART frame
       remains 16 bytes; reserve its unused last bitmap byte as zero.
       Keep higher usages in the six byte slots. */
    uint8_t next_byte[8] = {now_bit_report[0], 0};
    uint8_t next_bit[16] = {now_bit_report[0]};
    uint8_t slot = 2;
    (void)pre_bit_report;

    for (uint16_t key = 112; key < (size - 1) * 8; key++) {
        if ((now_bit_report[1 + key / 8] & (1U << (key % 8))) && slot < 8) {
            next_byte[slot++] = key;
        }
    }
    for (uint8_t key = 1; key < 112 && 1 + key / 8 < size; key++) {
        if (now_bit_report[1 + key / 8] & (1U << (key % 8))) {
            /* Keep held bitmap keys on that endpoint. Moving them back
               would release them before the following byte report arrives. */
            if (slot < 8 && !(uart_bit_report_buf[1 + key / 8] & (1U << (key % 8)))) {
                next_byte[slot++] = key;
            } else {
                next_bit[1 + key / 8] |= 1U << (key % 8);
            }
        }
    }

    bool f_byte_send = memcmp(bytekb_report_buf, next_byte, sizeof(next_byte)) != 0;
    bool f_bit_send = memcmp(uart_bit_report_buf + 1, next_bit + 1, sizeof(next_bit) - 1) != 0 ||
                      (f_bit_kb_act && uart_bit_report_buf[0] != next_bit[0]);
    memcpy(bytekb_report_buf, next_byte, sizeof(next_byte));
    memcpy(uart_bit_report_buf, next_bit, sizeof(next_bit));

    if (f_bit_send) {
        /* Retain through idle so modifier changes and release retries reach
           the bitmap endpoint until m_break_all_key() clears both reports. */
        f_bit_kb_act = true;
        uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, sizeof(next_bit));
    }
    if (f_byte_send) {
        uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, sizeof(next_byte));
    }
}


/**
 * @brief  Uart send keys report.
 */
void uart_send_report_func(void)
{
    static uint32_t interval_timer = 0;

    if (dev_info.link_mode == LINK_USB) return;
    if (f_rf_reset || rf_recovering || queue_count) {
        rf_replay_task();
        return;
    }
    keyboard_protocol          = 1;

    if (timer_elapsed32(interval_timer) > 100) {
        interval_timer = timer_read32();
        if (no_act_time <= 200) {
            uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);

            if(f_bit_kb_act)
            uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
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
static void RF_Protocol_Receive(void) {
    if (Usart_Mgr.RXDState == RX_Done) {
        if (Usart_Mgr.RXDLen > 4) {
            if ((Usart_Mgr.RXDLen - 5) != RX_LEN) {
                return;
            }

            /* Bounds check: RX_LEN comes from the RF module; reject if it
               would cause us to read past the end of RXDBuf. */
            if (RX_LEN > UART_MAX_LEN - 5) {
                Usart_Mgr.RXDState = RX_DATA_OV;
                return;
            }

            uint8_t check_sum = 0;
            uint8_t i;
            for (i = 0; i < RX_LEN; i++) {
                check_sum += Usart_Mgr.RXDBuf[4 + i];
            }

            if (check_sum != Usart_Mgr.RXDBuf[4 + i]) {
                Usart_Mgr.RXDState = RX_SUM_ERR;
                return;
            }
        } else if (Usart_Mgr.RXDLen == 3) {
            if (Usart_Mgr.RXDBuf[2] == 0xA0) {
                /* Short ACKs have no payload; do not reuse the last frame. */
                RX_LEN = 0;
            }
            else {
                return;
            }
        } else {
            return;
        }

        /* Only validated frames acknowledge commands or refresh recovery. */
        if (waiting_ack && RX_CMD == pending_cmd) f_uart_ack = true;
        sync_lost = 0;

        switch (RX_CMD) {
            case CMD_HAND: {
                f_rf_hand_ok = true;
                break;
            }

            case CMD_24G_SUSPEND: {
                if (dev_info.link_mode == LINK_RF_24) f_goto_sleep = true;
                break;
            }

            case CMD_NEW_ADV: {
                f_rf_new_adv_ok = true;
                break;
            }

            case CMD_RF_STS_SYSC: {
                static uint8_t error_cnt = 0;

                if (RX_LEN >= 5) {
                    if (dev_info.link_mode == Usart_Mgr.RXDBuf[4]) {
                        error_cnt = 0;

                        dev_info.rf_state = Usart_Mgr.RXDBuf[5];
                        if (reset_phase == 0 && !f_send_channel && dev_info.rf_state == RF_CONNECT) {
                            rf_recovering = false;
                        }

                        if ((dev_info.rf_state == RF_CONNECT) && ((Usart_Mgr.RXDBuf[6] & 0xf8) == 0)) {
                            dev_info.rf_led = Usart_Mgr.RXDBuf[6];
                        }

                        dev_info.rf_charge = Usart_Mgr.RXDBuf[7];

                        if (Usart_Mgr.RXDBuf[8] <= 100) {
                            dev_info.rf_baterry = Usart_Mgr.RXDBuf[8];
                        }
                    }
                    else {
                        if (dev_info.rf_state != RF_INVALID) {
                            if (error_cnt >= 5) {
                                error_cnt      = 0;
                                f_send_channel = true;
                            } else {
                                error_cnt++;
                            }
                        }
                    }

                    f_rf_sts_sysc_ok = true;
                }
                break;
            }

            case CMD_READ_DATA: {
                if (RX_LEN >= FUNC_VALID_LEN) {
                    memcpy(func_tab, &Usart_Mgr.RXDBuf[4], FUNC_VALID_LEN);

                    if (func_tab[4] <= LINK_USB) {
                        dev_info.link_mode = func_tab[4];
                    }

                    if (func_tab[5] < LINK_USB) {
                        dev_info.rf_channel = func_tab[5];
                    }

                    if ((func_tab[6] <= LINK_BT_3) && (func_tab[6] >= LINK_BT_1)) {
                        dev_info.ble_channel = func_tab[6];
                    }

                    f_rf_read_data_ok = true;
                }
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
    if (delayms) wait_ms(delayms);
    /* Drain old replies before changing the selected command's state. */
    uart_receive_pro();

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = cmd;
    Usart_Mgr.TXDBuf[2] = 0x00;
    Usart_Mgr.TXDBuf[3] = 0;

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
            Usart_Mgr.TXDBuf[3]  = 11;
            Usart_Mgr.TXDBuf[4]  = 1;
            Usart_Mgr.TXDBuf[5]  = 9;
            Usart_Mgr.TXDBuf[6]  = 'A';
            Usart_Mgr.TXDBuf[7]  = 'i';
            Usart_Mgr.TXDBuf[8]  = 'r';
            Usart_Mgr.TXDBuf[9]  = '9';
            Usart_Mgr.TXDBuf[10] = '6';
            Usart_Mgr.TXDBuf[11] = ' ';
            Usart_Mgr.TXDBuf[12] = 'V';
            Usart_Mgr.TXDBuf[13] = '2';
            Usart_Mgr.TXDBuf[14] = 0;
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

    if (Usart_Mgr.TXDBuf[3] > UART_MAX_LEN - 5) {
        return TX_DATA_ERR;
    }

    pending_cmd = cmd;
    waiting_ack = wait_ack != 0;
    f_uart_ack = 0;
    Usart_Mgr.TXDBuf[4 + Usart_Mgr.TXDBuf[3]] = get_checksum(&Usart_Mgr.TXDBuf[4], Usart_Mgr.TXDBuf[3]);
    UART_Send_Bytes(Usart_Mgr.TXDBuf, Usart_Mgr.TXDBuf[3] + 5);

    if (wait_ack) {
        while (wait_ack--) {
            wait_ms(1);
            uart_receive_pro();
            if (f_uart_ack) {
                waiting_ack = false;
                return TX_OK;
            }
        }
    } else {
        return TX_OK;
    }

    waiting_ack = false;
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

    static uint32_t reset_timer = 0;

    if (f_wakeup_prepare || f_goto_sleep) {
        if (reset_phase) writePinHigh(NRF_RESET_PIN);
        reset_phase = 0;
        f_rf_reset = false;
        sync_lost = 0;
        interval_timer = timer_read32();
        return;
    }

    /* Service every scan, before the 200ms status throttle. Keep the original
       reset pulse/settling times without blocking keyboard scanning. */
    if (reset_phase == 0 && f_rf_reset) {
        f_rf_reset = false;
        reset_phase = 1;
        rf_recovering = true;
        reset_timer = timer_read32();
    }
    if (reset_phase != 0) {
        if (timer_elapsed32(reset_timer) < (reset_phase == 1 ? 100 : 50)) {
            return;
        }
        reset_timer = timer_read32();
        if (reset_phase == 1) {
            writePinLow(NRF_RESET_PIN);
            reset_phase = 2;
        } else if (reset_phase == 2) {
            writePinHigh(NRF_RESET_PIN);
            reset_phase = 3;
        } else {
            reset_phase = 0;
            reset_cooldown = 3;
            dev_info.rf_state = RF_LINKING;
            f_send_channel = true;
            Usart_Mgr.RXDLen = 0;
            Usart_Mgr.RXDState = RX_Idle;
            sync_lost = 0;
            interval_timer = timer_read32();
        }
        return;
    }

    if (timer_elapsed32(interval_timer) < 200) {
        return;
    }
    interval_timer = timer_read32();

    /* Post-reset cooldown: skip sync_lost counting to give NRF time to boot */
    if (reset_cooldown > 0) {
        reset_cooldown--;
    }

    if (f_send_channel) {
        link_state_temp = RF_DISCONNECT;
        uart_send_cmd(CMD_SET_LINK, 0, 0);
        f_send_channel = false;
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
        else {
            rf_linking_time  = 0;
            disconnect_delay = 0;
            rf_blink_cnt     = 0;

            if (link_state_temp != RF_CONNECT) {
                link_state_temp   = RF_CONNECT;
                rf_link_show_time = 0;
            }
        }
    }

    /* In USB mode, only sync every ~2 seconds (10 × 200ms) for battery info.
       In RF mode, sync every 200ms for connection state. */
    /* Fire-and-forget: no pre-delay, no ACK wait.
       The ACK is picked up by uart_receive_pro() on the next cycle.
       Saves ~2ms of blocking per sync. */
    if (dev_info.link_mode == LINK_USB) {
        if (++usb_sync_prescaler >= 10) {
            usb_sync_prescaler = 0;
            uart_send_cmd(CMD_RF_STS_SYSC, 0, 0);
        }
    } else {
        usb_sync_prescaler = 0;
        uart_send_cmd(CMD_RF_STS_SYSC, 0, 0);
    }

    if (dev_info.link_mode != LINK_USB && reset_cooldown == 0) {
        if (++sync_lost >= 10) {
            sync_lost  = 0;
            f_rf_reset = true;
        }
    }
}

/**
 * @brief Uart send bytes.
 * @param Buffer data buf
 * @param Length data length
 */
static void UART_Send_Bytes(const uint8_t *Buffer, uint32_t Length) {
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
static uint8_t get_checksum(const uint8_t *buf, uint8_t len) {
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
void uart_send_report(uint8_t report_type, const uint8_t *report_buf, uint8_t report_size) {
    if (report_size > UART_MAX_LEN - 5) {
        return;
    }
    if (!f_dial_sw_init_ok) {
        return;
    }
    if (dev_info.link_mode == LINK_USB) {
        return;
    }
    if (report_type < CMD_RPT_MS || report_type > CMD_RPT_SYS || report_size > 16) return;
    uint8_t index = report_type - CMD_RPT_MS;
    memcpy(latest_reports[index], report_buf, report_size);
    /* Relative mouse movement must not be repeated during resynchronization. */
    if (index == 0) memset(latest_reports[0] + 1, 0, 4);
    if (f_rf_reset || rf_recovering || queue_count) {
        if (queue_count == RF_QUEUE_SIZE) {
            /* Bounded outage: sacrifice old transitions, keep current states
               for every endpoint so a lost release cannot leave a key held. */
            rf_queue_resync();
            return;
        }
        uint8_t tail = (queue_head + queue_count) % RF_QUEUE_SIZE;
        rf_queue[tail].captured_at = timer_read32();
        rf_queue[tail].type = report_type;
        rf_queue[tail].size = report_size;
        memcpy(rf_queue[tail].data, report_buf, report_size);
        queue_count++;
        return;
    }

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

/* Drain one report per interval, retaining order across newly arriving input. */
static void rf_replay_task(void) {
    /* Allow faster catch-up on the dongle's 1ms USB polling path while
       retaining Bluetooth pacing. One report per call, never a burst loop. */
    uint8_t interval = dev_info.link_mode == LINK_RF_24 ? 2 : 8;
    if (f_rf_reset || rf_recovering || f_wakeup_prepare || f_goto_sleep || !queue_count || timer_elapsed32(replay_timer) < interval) return;
    if (dev_info.link_mode == LINK_USB || dev_info.rf_state != RF_CONNECT) return;
    if (timer_elapsed32(rf_queue[queue_head].captured_at) >= RF_REPLAY_MAX_AGE_MS) rf_queue_resync();
    replay_timer = timer_read32();
    uint8_t size = rf_queue[queue_head].size;
    uint8_t frame[21] = {UART_HEAD, rf_queue[queue_head].type, 1, size};
    memcpy(frame + 4, rf_queue[queue_head].data, size);
    frame[4 + size] = get_checksum(frame + 4, size);
    UART_Send_Bytes(frame, size + 5);
    queue_head = (queue_head + 1) % RF_QUEUE_SIZE;
    queue_count--;
}

/**
 * @brief Uart receives data and processes it after completion,.
 */
void uart_receive_pro(void) {
    static uint32_t rx_timeout_timer = 0;

    /* Timeout recovery: if we started receiving a packet but no new bytes
       arrive within 10ms, discard the partial packet and reset the parser.
       Prevents the parser from hanging on UART noise. */
    if (Usart_Mgr.RXDLen > 0 && timer_elapsed32(rx_timeout_timer) > 10) {
        Usart_Mgr.RXDLen = 0;
        Usart_Mgr.RXDState = RX_Idle;
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
            if (b > UART_MAX_LEN - 5) {
                Usart_Mgr.RXDLen = 0;
                Usart_Mgr.RXDState = RX_Idle;
            } else {
                Usart_Mgr.RXDBuf[3] = b;
                Usart_Mgr.RXDLen = 4;
            }
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
    /* GPIO_OSPEEDER_OSPEEDR6 is a standard vendor-provided alias for GPIO_OSPEEDR_OSPEEDR6. */
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
    *ok_flag = false;
    while (timeout--) {
        uart_send_cmd(cmd, 0, 5);
        for (uint8_t i = 0; i < 10; i++) {
            wait_ms(1);
            uart_receive_pro();
            if (*ok_flag) {
                return true;
            }
        }
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
