"""Compile the complete production RF transport with simulated UART and clock."""
from pathlib import Path
import subprocess
import tempfile

kb = Path(__file__).resolve().parents[1] / 'keyboards/air96_v2/ansi'
header = (kb / 'ansi.h').read_text()
source = (kb / 'rf.c').read_text()
source = source[:source.index('void rf_uart_init(void) {')]
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
stubs = r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define NKRO_REPORT_BITS 30
#define NRF_RESET_PIN 1
#define NRF_WAKEUP_PIN 2
#define DC_BOOST_PIN 3
#define RGB_DRIVER_SDB1 4
#define RGB_DRIVER_SDB2 5
#define HOST_USB_TYPE 0
#define HOST_RF_TYPE 2
#define POWER_DOWN_DELAY 24
'''
stubs += header[header.index('typedef enum'):header.index('#define SYS_SW_WIN')]
stubs += r'''
typedef struct {uint8_t mods,reserved,keys[6];} report_keyboard_t;
typedef struct {uint8_t mods,bits[30];} report_nkro_t;
typedef struct {uint16_t usage;} report_extra_t;
typedef struct {uint8_t buttons,x,y,v,h;} report_mouse_t;
typedef int host_driver_t;
volatile bool f_uart_ack,f_rf_read_data_ok,f_rf_sts_sysc_ok,f_rf_new_adv_ok,f_rf_reset,f_rf_hand_ok,f_goto_sleep,f_wakeup_prepare,f_send_channel,f_dial_sw_init_ok=true;
DEV_INFO_STRUCT dev_info={.link_mode=LINK_RF_24,.rf_state=RF_CONNECT};
host_driver_t *m_host_driver,rf_host_driver;
uint8_t host_mode=HOST_RF_TYPE,rf_blink_cnt,keyboard_protocol;
uint16_t rf_link_show_time,rf_linking_time,no_act_time;
unsigned now,low_count,tx_count,delay_count;
uint8_t rx[256],tx[512][64];unsigned rx_n,rx_pos;
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
void wait_ms(unsigned n){now+=n;delay_count+=n;}
void wait_us(unsigned n){(void)n;}
void writePinLow(unsigned p){if(p==NRF_RESET_PIN)low_count++;}
void writePinHigh(unsigned p){(void)p;}
void host_set_driver(host_driver_t *p){(void)p;}
void m_break_all_key(void){}
bool uart_available(void){return rx_pos<rx_n;}
uint8_t uart_read(void){return rx[rx_pos++];}
unsigned reply_mode;
void uart_transmit(const uint8_t *p,uint16_t n){
 assert(tx_count<512);memcpy(tx[tx_count++],p,n);
 if(reply_mode && p[2]==0){rx_pos=0;rx_n=3;rx[0]=UART_HEAD;rx[1]=reply_mode==1?p[1]:CMD_RF_STS_SYSC;rx[2]=0xa0;}
}
'''
checks = r'''
static void feed(const uint8_t *p,unsigned n){memcpy(rx,p,n);rx_pos=0;rx_n=n;uart_receive_pro();}
static void connected(void){
 uint8_t p[]={UART_HEAD,CMD_RF_STS_SYSC,0,5,LINK_RF_24,RF_CONNECT,0,0,70,73};feed(p,sizeof p);
}
int main(void){
 // A queued late ACK must not extend an expired partial packet.
 uint8_t partial[]={UART_HEAD,CMD_RF_STS_SYSC,0,5};
 uint8_t ack[]={UART_HEAD,CMD_HAND,0xa0};
 now=1;feed(partial,4);now=100;feed(ack,3);assert(f_rf_hand_ok);
 f_rf_hand_ok=false;
 uint8_t over[]={UART_HEAD,CMD_RF_STS_SYSC,0,255,UART_HEAD,CMD_HAND,0xa0};
 feed(over,7);assert(f_rf_hand_ok && Usart_Mgr.RXDLen==0);
 // Correct command identity is required, including when status arrives instead.
 reply_mode=2;assert(uart_send_cmd(CMD_SET_LINK,3,0)==TX_TIMEOUT);
 reply_mode=1;assert(uart_send_cmd(CMD_SET_LINK,3,0)==TX_OK);reply_mode=0;
 dev_info.link_mode=LINK_BT_1;f_goto_sleep=false;
 uint8_t suspend[]={UART_HEAD,CMD_24G_SUSPEND,0xa0};feed(suspend,3);assert(!f_goto_sleep);
 dev_info.link_mode=LINK_RF_24;feed(suspend,3);assert(f_goto_sleep);f_goto_sleep=false;
 // No heartbeat or recovery reset while intentionally asleep.
 f_wakeup_prepare=true;unsigned before=tx_count;
 for(now=200;now<=3000;now+=200)dev_sts_sync();
 assert(tx_count==before && !f_rf_reset && !low_count);f_wakeup_prepare=false;
 // Capture a press AND release during recovery, not just the final state.
 f_rf_reset=true;now=4000;dev_sts_sync();
 uint8_t press[8]={0,0,4},release[8]={0};before=tx_count;
 uart_send_report(CMD_RPT_BYTE_KB,press,8);uart_send_report(CMD_RPT_BYTE_KB,release,8);
 assert(queue_count==2 && tx_count==before);
 now=4100;dev_sts_sync();assert(low_count==1);
 now=4150;dev_sts_sync();now=4200;dev_sts_sync();
 rf_replay_task();assert(tx_count==before); // still needs reconnection
 now=4400;dev_sts_sync();assert(f_send_channel==false && rf_recovering);
 connected();assert(!rf_recovering);
 before=tx_count;rf_replay_task();assert(tx_count==before+1 && tx[before][6]==4);
 rf_replay_task();assert(tx_count==before+1);
 now+=8;rf_replay_task();assert(tx_count==before+2 && tx[before+1][6]==0 && !queue_count);
 // Reconnect never renames the dongle or waits for that command's ACK.
 now+=200;delay_count=0;before=tx_count;dev_sts_sync();assert(delay_count==0);
 for(unsigned i=before;i<tx_count;i++)assert(tx[i][1]!=CMD_SET_24G_NAME);
 // Channel change cancels replay and a pending reset.
 f_rf_reset=true;uart_send_report(CMD_RPT_BYTE_KB,press,8);
 rf_reset_sync_lost();assert(!queue_count && !f_rf_reset && !rf_recovering);
 // Overflow resynchronizes all endpoints instead of leaving an old release lost.
 f_rf_reset=true;
 for(unsigned i=0;i<40;i++)uart_send_report(CMD_RPT_BYTE_KB,i%2?release:press,8);
 assert(queue_count<RF_QUEUE_SIZE);
 assert(latest_reports[CMD_RPT_BYTE_KB-CMD_RPT_MS][2]==0);
 f_rf_reset=false;rf_recovering=false;dev_info.rf_state=RF_CONNECT;
 while(queue_count){now+=8;rf_replay_task();}
 assert(tx[tx_count-1][6]==0);
 puts("Transport passed: parser gaps, command ACKs, suspend ownership, sleep, reset press/release replay, pacing, reconnect, channel cancellation, overflow");
}
'''
with tempfile.TemporaryDirectory(prefix='air96-transport-') as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test'
    c.write_text(stubs + source + checks)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
