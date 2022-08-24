/**
 * Copyright (c) 2021 Bjarne Dasenbrook
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef PICOSTEPPER_H
#define PICOSTEPPER_H

#define CLKDIV 125 // Clock divider of 125 gives a maximum steprate of 100,000steps/sec
#define MAXSTEPRATE 12500000/CLKDIV // Convert the clockrate into a maximum steps/second value
#define MINDELAY 0
#define NUMSTEPS 100 // The number of steps taken between accelerations
#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })
#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "four_wire.pio.h"
#include "two_wire.pio.h"
#include "stack.h"

// The index of a device withing the PicoStepperContainer
typedef int PicoStepper;

// A function to call after a async movement has finished
typedef void (*PicoStepperCallback)(PicoStepper);

// State and executing hardware of a StepperDevice
struct picostepper_raw_device_def {
  bool is_configured;
  bool is_running;
  bool enabled;
  bool direction;
  uint steps;
  int position;
  int acceleration_direction;
  uint acceleration;
  uint moving_acceleration;
  struct node *stack;
  uint delay;
	PIO pio;
  int pio_id;
	uint statemachine;
  int dma_channel;
  dma_channel_config dma_config;
  uint32_t command;
  PicoStepperCallback callback;
};
typedef struct picostepper_raw_device_def PicoStepperRawDevice;

// Object containing all devices managed by PicoStepper
struct PicoStepperContainer
{
  int max_device_count;
  PicoStepperRawDevice devices[8];
  bool device_with_index_is_in_use[8];
  int map_dma_ch_to_device_index[32];
};

// The different types of steppers used to select the correct PIO-program
enum PicoStepperMotorType_def {
  FourWireDriver, 
  FourWireDirect, 
  TwoWireDriver
};
typedef enum PicoStepperMotorType_def PicoStepperMotorType;


extern struct PicoStepperContainer psc;
extern bool psc_is_initialised;

static void picostepper_async_handler();
static PicoStepperRawDevice picostepper_create_raw_device();
static void picostepper_psc_init();
static PicoStepper picostepper_init_unclaimed_device();

PicoStepper picostepper_init(uint base_pin, PicoStepperMotorType driver);
PicoStepper picostepper_pindef_init(uint dir_pin, uint step_pin, PicoStepperMotorType driver);
bool picostepper_move_blocking(PicoStepper device, uint steps, bool direction, uint delay, int delay_change);
void picostepper_set_async_direction(PicoStepper device, bool direction);
void picostepper_set_async_enabled(PicoStepper device, bool enabled);
void picostepper_set_async_delay(PicoStepper device, uint delay);
int picostepper_convert_speed_to_delay(float steps_per_second);
int picostepper_convert_delay_to_speed(int delay);
bool picostepper_move_async(PicoStepper device, int steps, PicoStepperCallback func);
bool picostepper_move_to_position(PicoStepper device, int position);
bool picostepper_move_to_positions(volatile PicoStepper devices[], int positions[], uint num_steppers);
void picostepper_accelerate(PicoStepper device);
void picostepper_set_acceleration(PicoStepper device, uint acceleration);
void picostepper_set_position(PicoStepper device, uint position);

#endif