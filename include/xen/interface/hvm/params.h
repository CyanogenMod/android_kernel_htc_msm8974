/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __XEN_PUBLIC_HVM_PARAMS_H__
#define __XEN_PUBLIC_HVM_PARAMS_H__

#include "hvm_op.h"


#define HVM_PARAM_CALLBACK_IRQ 0

#define HVM_PARAM_STORE_PFN    1
#define HVM_PARAM_STORE_EVTCHN 2

#define HVM_PARAM_PAE_ENABLED  4

#define HVM_PARAM_IOREQ_PFN    5

#define HVM_PARAM_BUFIOREQ_PFN 6

#define HVM_PARAM_TIMER_MODE   10
#define HVMPTM_delay_for_missed_ticks    0
#define HVMPTM_no_delay_for_missed_ticks 1
#define HVMPTM_no_missed_ticks_pending   2
#define HVMPTM_one_missed_tick_pending   3

#define HVM_PARAM_HPET_ENABLED 11

#define HVM_PARAM_IDENT_PT     12

#define HVM_PARAM_DM_DOMAIN    13

#define HVM_PARAM_ACPI_S_STATE 14

#define HVM_PARAM_VM86_TSS     15

#define HVM_PARAM_VPT_ALIGN    16

#define HVM_PARAM_CONSOLE_PFN    17
#define HVM_PARAM_CONSOLE_EVTCHN 18

#define HVM_NR_PARAMS          19

#endif 
