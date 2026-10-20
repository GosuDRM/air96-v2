"""Exercise production wireless parsing, switching and sleep with host stubs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kb = root / "keyboards/air96_v2/ansi"
rf = (kb / "rf.c").read_text()
ansi = (kb / "ansi.c").read_text()
header = (kb / "ansi.h").read_text()
sleep = (kb / "sleep.c").read_text()

def between(text, start, end):
    return text[text.index(start):text.index(end, text.index(start))]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
'''
code += between(header, "typedef enum", "#define SYS_SW_WIN")
code += r'''
#define RX_CMD Usart_Mgr.RXDBuf[1]
#define RX_LEN Usart_Mgr.RXDBuf[3]
#define RF_LONG_PRESS_DELAY 60
#define HOST_USB_TYPE 0
#define HOST_RF_TYPE 2
#define SLEEP_TIME_DELAY 36000
#define LINK_TIMEOUT 12000
#define MATRIX_ROWS 6
#define USB_SUSPENDED 1
#define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED 2
#define DC_BOOST_PIN 0
#define RGB_DRIVER_SDB1 1
#define RGB_DRIVER_SDB2 2
#define LNK_RF 100
#define LNK_BLE1 101
#define LNK_BLE2 102
#define LNK_BLE3 103
USART_MGR_STRUCT Usart_Mgr;
DEV_INFO_STRUCT dev_info;
bool f_uart_ack,f_rf_hand_ok,f_rf_new_adv_ok,f_rf_read_data_ok,f_rf_sts_sysc_ok;
bool f_goto_sleep,f_wakeup_prepare,f_send_channel,f_rf_sw_press,waiting_ack,rf_recovering;
uint8_t pending_cmd,reset_phase;
uint8_t sync_lost,func_tab[32],rf_sw_temp,rf_sw_press_delay,host_mode;
uint16_t no_act_time,rf_linking_time,rf_link_show_time;
uint32_t rf_disconnect_time,now,numlock_settle_start;
uint8_t numlock_phase;
int rf_host_driver,*m_host_driver;
unsigned clears,clear_mode,report_key,command,downs,logical_keys,logical_mods,rows[6];
struct {bool sleep_enable;} user_config={true};
struct {unsigned state,status;} USB_DRIVER;
typedef struct {struct {bool pressed;} event;} keyrecord_t;
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
void m_break_all_key(void){clears++;clear_mode=dev_info.link_mode;report_key=0;}
void rf_reset_sync_lost(void){sync_lost=0;}
void host_set_driver(int *p){(void)p;}
uint8_t uart_send_cmd(uint8_t c,uint8_t a,uint8_t d){(void)a;(void)d;command=c;return 0;}
void uart_receive_pro(void){}
void wait_ms(unsigned n){now+=n;}
void writePinLow(unsigned p){(void)p;downs++;}
void writePinHigh(unsigned p){(void)p;}
unsigned has_anykey(void){return logical_keys;}
unsigned has_anymod(void){return logical_mods;}
unsigned matrix_get_row(unsigned r){return rows[r];}
void usb_lld_wakeup_host(void *p){(void)p;}
unsigned wakes;
void usbWakeupHost(void *p){(void)p;wakes++;}
void restart_usb_driver(void *p){(void)p;}
'''
code += between(rf, "static void RF_Protocol_Receive(void) {", "/**\n * @brief  Uart send cmd.")
code += between(ansi, "static void switch_dev_link(uint8_t mode)\n", "/**\n * @brief  scan dial switch.")
code += between(ansi, "static void long_press_key(void)\n", "    if (f_dev_reset_press)") + "}\n"
code += "bool handle(uint16_t keycode,keyrecord_t *record){switch(keycode){\n"
code += between(ansi, "        case LNK_RF:", "        case MAC_TASK:")
code += "default:return true;}}\n"
code += r"""
#define DEV_MODE_PIN 10
#define SYS_MODE_PIN 11
#define SYS_SW_MAC 0
#define SYS_SW_WIN 1
bool f_dial_sw_init_ok=true,f_sys_show;
struct {bool nkro;} keymap_config;
unsigned pins;
void setPinInputHigh(unsigned p){(void)p;}
unsigned readPin(unsigned p){return pins & (p==10?1:2);}
void default_layer_set(unsigned l){(void)l;}
"""
code += between(ansi, "static void dial_sw_scan(void)\n", "/**\n * @brief  power on scan dial switch.")
code += between(ansi, "static void m_power_on_dial_sw_scan(void)\n", "/* Shortcuts borrow")
code += sleep[sleep.index("void Sleep_Handle(void) {"):]
code += r'''
static void frame(unsigned n,unsigned cmd){
    Usart_Mgr.RXDState=RX_Done;Usart_Mgr.RXDLen=n;Usart_Mgr.RXDBuf[1]=cmd;
    f_uart_ack=false;sync_lost=9;waiting_ack=true;pending_cmd=cmd;
}
int main(void){
    // Contact bounce must not clear held input or suppress report delivery.
    dev_info.link_mode=LINK_BT_1;dev_info.rf_channel=LINK_BT_1;
    unsigned initial_clears=clears;
    for(unsigned i=0;i<10;i++){pins=i%2;now+=20;dial_sw_scan();assert(f_dial_sw_init_ok && clears==initial_clears);}
    pins=0;for(unsigned i=0;i<30;i++){now+=20;dial_sw_scan();assert(f_dial_sw_init_ok);}
    // Both OS layouts retain NKRO after a stable physical switch change.
    for(unsigned mode=0;mode<2;mode++){
        pins=mode?2:0;keymap_config.nkro=false;
        dev_info.sys_sw_state=mode?SYS_SW_WIN:SYS_SW_MAC;
        for(unsigned i=0;i<30;i++){now+=20;dial_sw_scan();}
        assert(keymap_config.nkro);
    }
    for(unsigned mode=0;mode<2;mode++){
        pins=mode?2:0;keymap_config.nkro=false;
        dev_info.sys_sw_state=mode?SYS_SW_WIN:SYS_SW_MAC;
        m_power_on_dial_sw_scan();assert(keymap_config.nkro);
    }
    // Invalid traffic must not acknowledge a command or postpone recovery.
    frame(6,CMD_HAND);RX_LEN=1;Usart_Mgr.RXDBuf[4]=1;Usart_Mgr.RXDBuf[5]=2;
    RF_Protocol_Receive();assert(!f_uart_ack && sync_lost==9 && !f_rf_hand_ok);
    frame(6,CMD_HAND);RX_LEN=2;RF_Protocol_Receive();assert(!f_uart_ack && sync_lost==9);
    frame(3,CMD_HAND);Usart_Mgr.RXDBuf[2]=0;RF_Protocol_Receive();assert(!f_uart_ack);
    // Valid short handshake/pairing ACKs still work, but cannot supply stale data.
    unsigned cmds[]={CMD_HAND,CMD_NEW_ADV,CMD_RF_STS_SYSC,CMD_READ_DATA};
    for(unsigned i=0;i<4;i++){
        memset(Usart_Mgr.RXDBuf,1,sizeof Usart_Mgr.RXDBuf);
        frame(3,cmds[i]);Usart_Mgr.RXDBuf[2]=0xa0;RX_LEN=32;
        dev_info.link_mode=LINK_BT_2;dev_info.rf_state=RF_DISCONNECT;
        RF_Protocol_Receive();assert(f_uart_ack && !sync_lost);
        assert(dev_info.link_mode==LINK_BT_2 && dev_info.rf_state==RF_DISCONNECT);
        assert(!f_rf_read_data_ok && !f_rf_sts_sysc_ok);
    }
    assert(f_rf_hand_ok && f_rf_new_adv_ok);
    // Valid full status must still update connection and battery data.
    frame(10,CMD_RF_STS_SYSC);RX_LEN=5;
    uint8_t data[]={LINK_BT_2,RF_CONNECT,0,1,77};
    memcpy(Usart_Mgr.RXDBuf+4,data,5);Usart_Mgr.RXDBuf[9]=83;
    RF_Protocol_Receive();assert(f_rf_sts_sysc_ok && dev_info.rf_state==RF_CONNECT && dev_info.rf_baterry==77);
    // Both short selection and long pairing release the old host at transition.
    for(unsigned target=LINK_RF_24;target<=LINK_BT_3;target++){
        dev_info.link_mode=LINK_BT_1;keyrecord_t r={{true}};
        rf_sw_press_delay=0;handle(LNK_RF+target,&r);
        unsigned before=clears;report_key=4;sync_lost=9;rf_disconnect_time=99;
        r.event.pressed=false;handle(LNK_RF+target,&r);
        assert(clears==before+1 && clear_mode==LINK_BT_1 && !report_key);
        assert(dev_info.link_mode==target && command==CMD_SET_LINK && !f_send_channel);
        assert(!sync_lost && !rf_disconnect_time);
        dev_info.link_mode=LINK_BT_1;rf_sw_temp=target;f_rf_sw_press=true;
        rf_sw_press_delay=59;report_key=4;before=clears;now+=100;
        long_press_key();assert(clears==before+1 && !report_key && clear_mode==LINK_BT_1);
        assert(command==CMD_NEW_ADV && !f_send_channel);
    }
    // Physical Fn, normal keys and logical modifiers all prevent wireless sleep.
    for(unsigned mode=LINK_RF_24;mode<=LINK_BT_3;mode++){
        dev_info.link_mode=mode;dev_info.rf_state=RF_CONNECT;
        for(unsigned kind=0;kind<3;kind++){
            rows[5]=(kind==0);logical_keys=(kind==1);logical_mods=(kind==2);
            no_act_time=SLEEP_TIME_DELAY;f_goto_sleep=true;unsigned before=downs;
            now+=100;Sleep_Handle();assert(!f_goto_sleep && !no_act_time && downs==before);
        }
    }
    logical_mods=0;no_act_time=SLEEP_TIME_DELAY;now+=100;Sleep_Handle();assert(f_goto_sleep);
    now+=100;Sleep_Handle();assert(f_wakeup_prepare); // genuine idle still sleeps
    f_wakeup_prepare=false;dev_info.link_mode=LINK_USB;USB_DRIVER.state=USB_SUSPENDED;
    rows[0]=1;no_act_time=100;f_goto_sleep=true;unsigned before=downs;
    now+=100;Sleep_Handle();assert(downs>before); // USB suspend is not vetoed
    // Local USB wake returns immediately; host resume never signals wake again.
    USB_DRIVER.status=2;no_act_time=0;f_wakeup_prepare=true;now+=100;
    unsigned start=now;Sleep_Handle();assert(now==start && wakes==1);
    USB_DRIVER.state=0;no_act_time=100;f_wakeup_prepare=true;now+=100;
    start=now;Sleep_Handle();assert(now==start && wakes==1);
    puts("Wireless regressions passed: invalid frames, short ACKs, status, all channels, pairing, held-key sleep, USB suspend");
}
'''
with tempfile.TemporaryDirectory(prefix="air96-wireless-test-") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test"
    c.write_text(code)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
