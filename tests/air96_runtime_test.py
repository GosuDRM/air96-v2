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
    numlock = ansi[ansi.index('    if (dev_info.link_mode != LINK_USB || host_mode != HOST_USB_TYPE'):]
    numlock = numlock[:numlock.index('\n}')]
    compile_run(tmp, 'input', [r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#define LINK_USB 4
#define HOST_USB_TYPE 0
#define USB_DEVICE_STATE_CONFIGURED 1
#define KC_NUM_LOCK 83
uint8_t weak,real,keys[256],numlock_phase,host_mode;
unsigned reports,now,numlock_settle_start,numlock_hold_start,no_act_time=100,usb_device_state=1;
struct {unsigned link_mode;} dev_info;
struct leds {bool num_lock;};
struct leds host_keyboard_led_state(void){return (struct leds){false};}
uint8_t get_weak_mods(void){return weak;}
bool is_key_pressed(uint8_t k){return keys[k];}
void add_weak_mods(uint8_t m){weak|=m;}
void set_weak_mods(uint8_t m){weak=m;}
void send_keyboard_report(void){reports++;}
void register_code(uint8_t k){keys[k]=1;}
void unregister_code(uint8_t k){keys[k]=0;}
void wait_ms(unsigned n){now+=n;}
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
''' + helper + '\nvoid numlock_task(void){\n' + numlock + r'''
}
int main(void){
 for(unsigned held=0;held<2;held++){
  real=0x0a;weak=0x40;keys[4]=held;
  tap_shortcut(0x0a,4);
  assert(real==0x0a && weak==0x40 && keys[4]==held && reports);
 }
 for(unsigned mode=0;mode<4;mode++){
  dev_info.link_mode=mode;host_mode=2;numlock_phase=1;numlock_settle_start=1;now=3000;
  numlock_task();assert(!keys[KC_NUM_LOCK] && numlock_phase==1 && !numlock_settle_start);
 }
 dev_info.link_mode=LINK_USB;host_mode=HOST_USB_TYPE;
 numlock_task();now+=2000;numlock_task();assert(keys[KC_NUM_LOCK] && numlock_phase==2);
 now+=50;numlock_task();assert(!keys[KC_NUM_LOCK] && !numlock_phase);
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
print('Runtime regressions passed: buffer boundaries (ASan), held shortcut state, USB NumLock ownership, mid-flush RGB changes')
