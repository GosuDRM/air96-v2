"""A bouncing first contact must produce one tap; intentional taps stay distinct."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kb = root / "keyboards/air96_v2/ansi"
algorithm = re.search(r"DEBOUNCE_TYPE\s*=\s*(\w+)", (kb / "rules.mk").read_text())[1]
delay = json.loads((kb / "keyboard.json").read_text())["debounce"]
source = (root / "quantum/debounce" / f"{algorithm}.c").read_text()
source = source.replace('#include "debounce.h"', '').replace('#include "timer.h"', '')
code = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#define MATRIX_COLS 1
typedef uint16_t matrix_row_t;
typedef uint16_t fast_timer_t;
static unsigned now;
fast_timer_t timer_read_fast(void){return now;}
#define TIMER_DIFF_FAST(a,b) ((uint16_t)((a)-(b)))
'''
code += f"\n#define DEBOUNCE {delay}\n" + source
code += r'''
int main(void){
 matrix_row_t raw[1]={0},cooked[1]={0};
 unsigned presses=0,releases=0;
 debounce_init(1);
 // Long idle followed by a bouncing press and release, then two clean taps.
 for(now=0;now<=70200;now++){
  unsigned t=now>=70000?now-70000:1000;
  matrix_row_t next=t<6 || (t>=12 && t<50) || (t>=56 && t<62) ||
                    (t>=100 && t<125) || (t>=150 && t<175);
  bool changed=raw[0]!=next;raw[0]=next;
  if(debounce(raw,cooked,1,changed)){
   if(cooked[0])presses++;else releases++;
  }
  // Presses must reach the host on the first contact scan: debounce
  // may delay releases, never the press itself (input latency).
  if(t==0)assert(presses==1);
  if(t==90)assert(presses==1 && releases==1);
 }
 assert(presses==3 && releases==3 && !cooked[0]);
 debounce_free();
 puts("Debounce passed: zero press latency, idle first-contact chatter, release chatter, distinct taps");
}
'''
with tempfile.TemporaryDirectory(prefix="air96-debounce-") as tmp:
    c = Path(tmp) / "test.c"
    exe = Path(tmp) / "test"
    c.write_text(code)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
