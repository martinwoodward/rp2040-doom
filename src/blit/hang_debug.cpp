// Debug-only hang/fault catcher for bringing up new boards.
// Records the faulting/stuck PC in watchdog scratch registers, reboots, then
// shows the result on screen and USB serial on the next boot.
#ifdef DOOM_HANG_DEBUG
#include <cstdio>
#include "32blit.hpp"

#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/powman.h"

static constexpr uint32_t magic = 0xD0DE0000;
static constexpr uint32_t hang_timeout_us = 5000000;

static volatile uint32_t heartbeat_time = 0;
static volatile uint32_t phase = 0;
static volatile bool armed = false;
static int alarm_num = -1;

static volatile bool have_report = false;
static uint32_t report[8], report2[4];

static void __attribute__((noreturn)) save_and_reboot(uint32_t cause, uint32_t *frame, uint32_t extra) {
  if(!have_report) {
    powman_hw->scratch[0] = magic | cause;
    powman_hw->scratch[1] = frame[6]; // pc
    powman_hw->scratch[2] = frame[5]; // lr
    powman_hw->scratch[3] = extra;
    powman_hw->scratch[4] = frame[7]; // xpsr
    powman_hw->scratch[5] = scb_hw->bfar;
    powman_hw->scratch[6] = uint32_t(frame);
    powman_hw->scratch[7] = phase;
  } else {
    // second fault while showing the report
    watchdog_hw->scratch[0] = frame[6];
    watchdog_hw->scratch[1] = frame[5];
    watchdog_hw->scratch[2] = scb_hw->bfar;
    watchdog_hw->scratch[3] = (extra & 0xFFFF) | (frame[7] & 0x1FF) << 16;
  }
  watchdog_reboot(0, 0, 0);
  while(true) {}
}

extern "C" void hang_debug_fault_c(uint32_t *frame) {
  save_and_reboot(1, frame, scb_hw->cfsr);
}

extern "C" void hang_debug_alarm_c(uint32_t *frame) {
  timer_hw->intr = 1u << alarm_num;

  if(armed && time_us_32() - heartbeat_time > hang_timeout_us)
    save_and_reboot(2, frame, phase);

  timer_hw->alarm[alarm_num] = timer_hw->timerawl + 200000;
}

extern "C" void __attribute__((naked)) isr_hardfault() {
  asm volatile(
    "movs r0, #4\n"
    "mov r1, lr\n"
    "tst r0, r1\n"
    "beq 1f\n"
    "mrs r0, psp\n"
    "b hang_debug_fault_c\n"
    "1:\n"
    "mrs r0, msp\n"
    "b hang_debug_fault_c\n"
  );
}

static void __attribute__((naked)) alarm_isr() {
  asm volatile(
    "movs r0, #4\n"
    "mov r1, lr\n"
    "tst r0, r1\n"
    "beq 1f\n"
    "mrs r0, psp\n"
    "b hang_debug_alarm_c\n"
    "1:\n"
    "mrs r0, msp\n"
    "b hang_debug_alarm_c\n"
  );
}

void hang_debug_init() {
  if((powman_hw->scratch[0] & 0xFFFF0000) == magic) {
    for(int i = 0; i < 8; i++)
      report[i] = powman_hw->scratch[i];
    for(int i = 0; i < 4; i++)
      report2[i] = watchdog_hw->scratch[i];
    powman_hw->scratch[0] = 0;
    have_report = true;
    return;
  }

  alarm_num = hardware_alarm_claim_unused(true);
  auto irq = hardware_alarm_get_irq_num(alarm_num);
  irq_set_exclusive_handler(irq, alarm_isr);
  irq_set_priority(irq, 0);
  hw_set_bits(&timer_hw->inte, 1u << alarm_num);
  irq_set_enabled(irq, true);
  timer_hw->alarm[alarm_num] = timer_hw->timerawl + 200000;
}

bool hang_debug_has_report() {
  return have_report;
}

void hang_debug_beat(uint32_t new_phase) {
  phase = new_phase;
  heartbeat_time = time_us_32();
  armed = true;
}

void hang_debug_render() {
  using namespace blit;

  static const char *names[] = {"PC", "LR", "X", "PSR", "BFAR", "SP", "PH"};
  screen.pen = Pen(0, 0, 0);
  screen.clear();
  screen.pen = Pen(255, 255, 0);
  screen.text((report[0] & 0xFF) == 1 ? "HARDFAULT" : "HANG", minimal_font, Point(2, 1), false);
  for(int i = 0; i < 7; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%-4s %08lX", names[i], (unsigned long)report[i + 1]);
    screen.text(buf, minimal_font, Point(2, 14 + i * 14), false);
  }

  static uint32_t last_print = 0;
  if(now() - last_print > 100) {
    last_print = now();
    printf("DOOMDBG c=%lx pc=%08lx lr=%08lx cfsr=%08lx psr=%08lx bfar=%08lx sp=%08lx ph=%08lx | 2nd pc=%08lx lr=%08lx bfar=%08lx x=%08lx\n",
      (unsigned long)report[0], (unsigned long)report[1], (unsigned long)report[2], (unsigned long)report[3],
      (unsigned long)report[4], (unsigned long)report[5], (unsigned long)report[6], (unsigned long)report[7],
      (unsigned long)report2[0], (unsigned long)report2[1], (unsigned long)report2[2], (unsigned long)report2[3]);
  }

  // scale the top-left quarter up 2x so it's readable from a distance
  auto fb = (uint16_t *)screen.data;
  int w = screen.bounds.w;
  for(int y = screen.bounds.h / 2 - 1; y >= 0; y--) {
    for(int x = w / 2 - 1; x >= 0; x--) {
      auto c = fb[x + y * w];
      fb[x * 2 + y * 2 * w] = c;
      fb[x * 2 + 1 + y * 2 * w] = c;
      fb[x * 2 + (y * 2 + 1) * w] = c;
      fb[x * 2 + 1 + (y * 2 + 1) * w] = c;
    }
  }
}
#endif
