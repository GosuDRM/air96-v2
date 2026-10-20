"""Compile the production RF functions against a deterministic host/clock stub."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "keyboards/air96_v2/ansi/rf.c").read_text()
reports = source[source.index("bool f_bit_kb_act"):source.index("/**\n * @brief  Uart send consumer")]
reset = source[source.index("void dev_sts_sync(void)"):source.index("/**\n * @brief Uart send bytes.")]
stubs = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define LINK_USB 0
#define LINK_RF_24 2
#define HOST_USB_TYPE 0
#define HOST_RF_TYPE 1
#define RF_DISCONNECT 0
#define RF_CONNECT 1
#define NRF_RESET_PIN 0
#define CMD_RPT_BYTE_KB 1
#define CMD_RPT_BIT_KB 2
#define CMD_SET_LINK 3
#define CMD_SET_24G_NAME 4
#define CMD_RF_STS_SYSC 5
uint8_t uart_bit_report_buf[31], bytekb_report_buf[8];
unsigned no_act_time, keyboard_protocol, now, byte_sends, bit_sends, commands;
unsigned pin_low, pin_high;
uint8_t sync_lost, host_mode=HOST_RF_TYPE, rf_blink_cnt, disconnect_delay;
uint16_t rf_link_show_time, rf_linking_time;
bool f_rf_reset, f_send_channel;
int rf_host_driver, *m_host_driver;
struct {unsigned link_mode, rf_state;} dev_info={1, RF_CONNECT};
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
void writePinLow(unsigned p){(void)p;pin_low++;}
void writePinHigh(unsigned p){(void)p;pin_high++;}
void host_set_driver(int *p){(void)p;}
void m_break_all_key(void){}
void uart_send_cmd(unsigned c,unsigned a,unsigned d){(void)c;(void)a;(void)d;commands++;}
void uart_send_report(uint8_t t,const uint8_t *b,uint8_t n){
    (void)b;
    if(t==CMD_RPT_BYTE_KB){assert(n==8);byte_sends++;}
    else {assert(t==CMD_RPT_BIT_KB && n==16);bit_sends++;}
}
"""
checks = r"""
static uint8_t held[31], previous[31];
static void key(unsigned k,bool down){
    if(down)held[1+k/8]|=1U<<(k%8);
    else held[1+k/8]&=~(1U<<(k%8));
    uart_auto_nkey_send(previous,held,31);
    memcpy(previous,held,31);
}
static bool represented(unsigned k){
    for(unsigned i=2;i<8;i++)if(bytekb_report_buf[i]==k)return true;
    return k<120 && (uart_bit_report_buf[1+k/8]&(1U<<(k%8)));
}
int main(void){
    for(unsigned k=4;k<=9;k++)key(k,true);
    assert(bit_sends==0); // ordinary six-key typing keeps the byte format
    key(135,true); // high usage must survive six occupied byte slots
    for(unsigned k=4;k<=9;k++)assert(represented(k));
    assert(represented(135));
    no_act_time=201;now=101;uart_send_report_func();
    unsigned before=bit_sends;
    held[0]=2;uart_auto_nkey_send(previous,held,31);
    assert(bit_sends==before+1 && uart_bit_report_buf[0]==2);
    memcpy(previous,held,31);
    uint8_t held_bitmap[15];memcpy(held_bitmap,uart_bit_report_buf+1,15);
    key(135,false);assert(!represented(135));
    assert(memcmp(held_bitmap,uart_bit_report_buf+1,15)==0); // no false release during migration
    for(unsigned k=4;k<=9;k++)assert(represented(k));
    for(unsigned k=4;k<=9;k++)key(k,false);
    for(unsigned i=1;i<16;i++)assert(uart_bit_report_buf[i]==0);
    // A seventh high usage exceeds the protocol, but becomes reportable
    // immediately when a slot frees, without requiring a new key press.
    for(unsigned k=130;k<=136;k++)key(k,true);
    assert(!represented(136));key(130,false);assert(represented(136));
    // Reset progression must return at every timestamp; wait_ms is unstubbed
    // so reintroducing a blocking wait also fails this compilation.
    now=1000;f_rf_reset=true;dev_sts_sync();assert(!pin_low);
    for(now=1001;now<1100;now++)dev_sts_sync();
    assert(!pin_low);dev_sts_sync();assert(pin_low==1 && !pin_high);
    now=1149;dev_sts_sync();assert(!pin_high);
    now=1150;dev_sts_sync();assert(pin_high==1);
    now=1199;dev_sts_sync();assert(!commands);
    now=1200;dev_sts_sync();assert(!commands && !f_rf_reset);
    now=1400;dev_sts_sync();assert(commands && sync_lost==0);
    puts("RF regressions passed: rollover, idle modifiers, releases, overflow recovery, nonblocking reset");
}
"""
with tempfile.TemporaryDirectory(prefix="air96-rf-test-") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test"
    c.write_text(stubs + reports + reset + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
