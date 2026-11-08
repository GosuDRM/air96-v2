"""Regressions for cross-file buffer sizes, shortcuts, NumLock and RGB flushing."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kb = root / 'keyboards/air96_v2/ansi'
ansi = (kb / 'ansi.c').read_text()
rf = (kb / 'rf.c').read_text()
header = (kb / 'rf_driver.h').read_text()
led = (root / 'drivers/led/issi/is31fl3733.c').read_text()

def compile_run(tmp, name, parts):
    files = []
    for i, part in enumerate(parts):
        p = Path(tmp) / f'{name}{i}.c'
        p.write_text(part)
        files.append(str(p))
    exe = str(Path(tmp) / name)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', *files, '-o', exe], check=True)
    subprocess.run([exe], check=True)

with tempfile.TemporaryDirectory(prefix='air96-runtime-') as tmp:
    # Separate translation units catch ABI mismatches hidden from per-file lint.
    pre = '#include <stdint.h>\n#include <string.h>\n#define NKRO_REPORT_BITS 30\n'
    declarations = '\n'.join(line for line in header.splitlines() if line.startswith('extern uint8_t'))
    definitions = '\n'.join(line for line in rf.splitlines() if re.match(r'uint8_t\s+(uart_bit_report_buf|bitkb_report_buf|bytekb_report_buf)\[', line))
    clears = '\n'.join(line for line in ansi.splitlines() if re.search(r'memset\((uart_bit_report_buf|bitkb_report_buf|bytekb_report_buf),', line))
    compile_run(tmp, 'buffers', [pre + declarations + '\nint main(void){\n' + clears + '\n}', pre + definitions])

    # No wait stub: clearing both endpoints must not stall scanning.
    release = ansi[ansi.index('void m_break_all_key(void)\n'):ansi.index('/**\n * @brief  switch device link mode.')]
    compile_run(tmp, 'release', [pre + definitions + r'''
#include <stdbool.h>
#include <assert.h>
#define LINK_USB 4
#define CMD_RPT_BIT_KB 2
#define CMD_RPT_BYTE_KB 1
typedef struct {uint8_t data[31];} report_nkro_t;
typedef struct {uint8_t data[8];} report_keyboard_t;
report_nkro_t nr,*nkro_report=&nr;
report_keyboard_t kr,*keyboard_report=&kr;
struct {bool nkro;} keymap_config;
struct {unsigned link_mode;} dev_info;
bool f_bit_kb_act;
unsigned nkro_sends,byte_sends,rf_sends;
void clear_weak_mods(void){}
void clear_mods(void){}
void clear_keyboard(void){}
void host_nkro_send(report_nkro_t *r){for(unsigned i=0;i<sizeof *r;i++)assert(!r->data[i]);nkro_sends++;}
void host_keyboard_send(report_keyboard_t *r){for(unsigned i=0;i<sizeof *r;i++)assert(!r->data[i]);byte_sends++;}
void uart_send_report(unsigned t,const uint8_t *r,unsigned n){(void)t;for(unsigned i=0;i<n;i++)assert(!r[i]);rf_sends++;}
''' + release + r'''
int main(void){
 for(unsigned mode=0;mode<=4;mode++){
  dev_info.link_mode=mode;keymap_config.nkro=true;
  memset(&nr,255,sizeof nr);memset(&kr,255,sizeof kr);
  unsigned before=rf_sends;m_break_all_key();
  assert(keymap_config.nkro && !f_bit_kb_act);
  assert(rf_sends-before==(mode==LINK_USB?0:2));
 }
 assert(nkro_sends==5 && byte_sends==5);
}
'''])

    helper = ansi[ansi.index('static void tap_shortcut('):ansi.index('/**\n * @brief  qmk process record')]
    def function(signature):
        body = ansi[ansi.index(signature):]
        return body[:body.index('\n}\n') + 3]
    numlock = function('void notify_usb_device_state_change_kb(') + function('static void numlock_task(void)')
    compile_run(tmp, 'input', [r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#define LINK_USB 4
#define LINK_BT_1 1
#define HOST_USB_TYPE 0
#define HOST_RF_TYPE 2
#define RF_CONNECT 3
#define RF_DISCONNECT 4
#define SYS_SW_WIN 0xa1
#define SYS_SW_MAC 0xa2
#define KC_NUM_LOCK 83
enum usb_device_state {USB_DEVICE_STATE_INIT=1,USB_DEVICE_STATE_CONFIGURED=2,USB_DEVICE_STATE_SUSPEND=3};
uint8_t weak,real,keys[256],numlock_phase,host_mode;
unsigned reports,now=1,presses,numlock_settle_start,numlock_hold_start,no_act_time=100,usb_device_state;
bool f_numlock_rearm,f_wakeup_prepare,f_goto_sleep,host_num;
struct {unsigned link_mode,rf_state,sys_sw_state;} dev_info;
struct leds {bool num_lock;};
struct leds host_keyboard_led_state(void){return (struct leds){host_num};}
uint8_t get_weak_mods(void){return weak;}
bool is_key_pressed(uint8_t k){return keys[k];}
void add_weak_mods(uint8_t m){weak|=m;}
void set_weak_mods(uint8_t m){weak=m;}
void send_keyboard_report(void){reports++;}
/* Host model: each NumLock press toggles the host's NumLock state. */
void register_code(uint8_t k){keys[k]=1;if(k==KC_NUM_LOCK){presses++;host_num=!host_num;}}
void unregister_code(uint8_t k){keys[k]=0;}
void wait_ms(unsigned n){now+=n;}
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
void notify_usb_device_state_change_user(enum usb_device_state s){(void)s;}
''' + helper + numlock + r'''
static void run(unsigned ms){while(ms--){numlock_task();now++;}}
/* PC restart, reset and configure events applied back to back: housekeeping
   never observes the bus unconfigured, so only the reset event can re-arm. */
static void usb_reenumerate(void){
 usb_device_state=USB_DEVICE_STATE_INIT;notify_usb_device_state_change_kb(USB_DEVICE_STATE_INIT);
 usb_device_state=USB_DEVICE_STATE_CONFIGURED;notify_usb_device_state_change_kb(USB_DEVICE_STATE_CONFIGURED);
}
int main(void){
 for(unsigned held=0;held<2;held++){
  real=0x0a;weak=0x40;keys[4]=held;
  tap_shortcut(0x0a,4);
  assert(real==0x0a && weak==0x40 && keys[4]==held && reports);
 }
 /* Keyboard power-on, wired: NumLock turned on once the host has settled. */
 dev_info.sys_sw_state=SYS_SW_WIN;dev_info.link_mode=LINK_USB;host_mode=HOST_USB_TYPE;
 usb_device_state=USB_DEVICE_STATE_CONFIGURED;numlock_phase=1;
 run(1999);assert(!presses);
 run(10);assert(presses==1 && keys[KC_NUM_LOCK]);
 run(60);assert(!keys[KC_NUM_LOCK] && host_num && !numlock_phase);
 /* A user who turns NumLock off is not overridden while the link stays up. */
 host_num=false;run(10000);assert(presses==1);
 /* PC restart (bus reset + re-enumeration) comes back with NumLock off: turn it on. */
 usb_reenumerate();run(2200);assert(presses==2 && host_num && !keys[KC_NUM_LOCK]);
 /* Host already restores NumLock on: never toggle it back off. */
 usb_reenumerate();run(3000);assert(presses==2 && host_num);
 /* Wait for an idle window so the press can't interleave with typing. */
 host_num=false;usb_reenumerate();no_act_time=0;run(5000);assert(presses==2);
 no_act_time=100;run(100);assert(presses==3 && host_num);
 run(60);
 /* Wireless: charging cable resets don't target the radio host; connect does. */
 dev_info.link_mode=LINK_BT_1;host_mode=HOST_RF_TYPE;dev_info.rf_state=RF_DISCONNECT;host_num=false;
 usb_reenumerate();assert(!f_numlock_rearm);run(5000);assert(presses==3);
 dev_info.rf_state=RF_CONNECT;run(2200);assert(presses==4 && host_num);
 /* Reconnect (e.g. PC restart over Bluetooth) re-applies NumLock on. */
 host_num=false;dev_info.rf_state=RF_DISCONNECT;run(500);dev_info.rf_state=RF_CONNECT;run(2200);
 assert(presses==5 && host_num);
 /* Nothing is sent while the radio sleeps; the wake reconnect re-applies it. */
 host_num=false;f_goto_sleep=true;run(5000);assert(presses==5);
 f_goto_sleep=false;run(2200);assert(presses==6 && host_num);
 /* Link drop mid-press still releases the key (no stuck NumLock). */
 host_num=false;dev_info.rf_state=RF_DISCONNECT;run(10);dev_info.rf_state=RF_CONNECT;
 while(!keys[KC_NUM_LOCK])run(1);
 dev_info.rf_state=RF_DISCONNECT;run(60);assert(!keys[KC_NUM_LOCK] && !numlock_phase);
 /* Mac mode: macOS has no NumLock (the key acts as Clear), so never press it. */
 unsigned before=presses;host_num=false;dev_info.sys_sw_state=SYS_SW_MAC;
 dev_info.rf_state=RF_CONNECT;run(5000);
 dev_info.link_mode=LINK_USB;host_mode=HOST_USB_TYPE;usb_reenumerate();run(5000);assert(presses==before);
}
'''])
    flush = led[led.index('bool is31fl3733_flush_continue(void) {'):]
    compile_run(tmp, 'led', [r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#define IS31FL3733_DRIVER_COUNT 2
#define IS31FL3733_PWM_REGISTER_COUNT 192
#define IS31FL3733_COMMAND_PWM 1
#define IS31FL3733_REG_COMMAND_WRITE_LOCK 254
#define IS31FL3733_COMMAND_WRITE_LOCK_MAGIC 197
#define IS31FL3733_REG_COMMAND 253
#define I2C_STATUS_SUCCESS 0
#define IS31FL3733_I2C_PERSISTENCE 0
#define IS31FL3733_I2C_TIMEOUT 1
struct {uint8_t pwm_buffer[192];bool pwm_buffer_dirty;} driver_buffers[2];
uint8_t i2c_addresses[2]={0,1},hardware[2][192];
unsigned calls,fail_at;
int i2c_write_register(uint8_t d,uint8_t r,const uint8_t *p,uint8_t n,unsigned t){(void)t;if(++calls==fail_at)return -1;if(n==16)memcpy(hardware[d/2]+r,p,n);return 0;}
''' + flush + r'''
int main(void){
 driver_buffers[0].pwm_buffer[0]=1;driver_buffers[0].pwm_buffer_dirty=true;
 assert(!is31fl3733_flush_continue());assert(hardware[0][0]==1);
 driver_buffers[0].pwm_buffer[0]=2;driver_buffers[0].pwm_buffer_dirty=true;
 while(!is31fl3733_flush_continue()){}
 assert(driver_buffers[0].pwm_buffer_dirty); // an already-sent chunk changed
 while(!is31fl3733_flush_continue()){}
 assert(hardware[0][0]==2 && !driver_buffers[0].pwm_buffer_dirty);
 // Failed page unlock, page selection and PWM writes must all recover.
 for(unsigned failure=1;failure<=14;failure++){
  memset(hardware,0,sizeof hardware);memset(driver_buffers[0].pwm_buffer,7,192);
  driver_buffers[0].pwm_buffer_dirty=true;fail_at=calls+failure;
  unsigned limit=100;while(!is31fl3733_flush_continue())assert(--limit);
  for(unsigned i=0;i<192;i++)assert(hardware[0][i]==7);
 }

}
'''])
print('Runtime regressions passed: buffer boundaries (ASan), held shortcut state, NumLock on at boot, PC restart and wireless reconnect, mid-flush RGB changes')
