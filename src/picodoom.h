#ifndef _PICODOOM_H
#define _PICODOOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "m_fixed.h"

#ifndef PICO_BUILD
// stubs
#define __aligned(x) __attribute__((aligned(x)))
#define __packed __attribute__((packed))
#define __noinline __attribute__((noinline))
#define panic_unsupported() panic("unsupported")
#define panic I_Error

#define CU_REGISTER_DEBUG_PINS(...)
#define DEBUG_PINS_SET(x, y)
#define DEBUG_PINS_CLR(x, y)

typedef int semaphore_t;
#define sem_init(...)
#define sem_acquire_blocking(x)
#define sem_release(x)

typedef int spin_lock_t;
#define spin_lock_instance(x) NULL
#define spin_lock_blocking(x) 0
#define spin_unlock(x, y)

#define get_core_num() 0

static inline uint32_t hw_divider_u32_quotient_inlined(uint32_t a, uint32_t b) {return a / b;}
static inline int32_t hw_divider_s32_quotient_inlined(int32_t a, int32_t b) {return a / b;}
static inline int32_t __mul_instruction (int32_t a, int32_t b) {return a * b;}
#endif

typedef enum {
    PDCOL_NONE = 0,
    PDCOL_TOP,
    PDCOL_MID,
    PDCOL_BOTTOM,
    PDCOL_SKY,
    PDCOL_MASKED,
    PDCOL_FLOOR,
    PDCOL_CEILING,
} pd_column_type;

extern volatile uint8_t interp_in_use;
void pd_init();
void pd_core1_loop();
void pd_begin_frame();
void pd_add_column(pd_column_type type);
void pd_add_masked_columns(uint8_t *ys, int seg_count);
void pd_add_plane_column(int x, int yl, int yh, fixed_t scale, int floor, int fd_num);
void pd_end_frame(int wipe_start);
uint8_t *pd_get_work_area(uint32_t *size);
#if PICO_ON_DEVICE
void pd_start_save_pause(void);
void pd_end_save_pause(void);
const uint8_t *get_end_of_flash(void);
#endif
extern int pd_flag;
extern fixed_t pd_scale;
#ifdef __cplusplus
}
#endif

#endif