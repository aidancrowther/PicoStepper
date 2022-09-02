/**
 * Copyright (c) 2021 Bjarne Dasenbrook
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "picostepper.h"

void movement_finished(PicoStepper device) {
  picostepper_move_async(device, 100, &movement_finished);
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  
  uint x_dir = 21;
  uint x_step = 20;

  uint y_dir = 18;
  uint y_step = 19;

  uint red = 13;
  uint green = 12;
  uint blue = 11;

  PicoStepper device = picostepper_pindef_init(x_dir, x_step, TwoWireDriver);
  PicoStepper device_two = picostepper_pindef_init(y_dir, y_step, TwoWireDriver);

  PicoStepper devices[2];
  devices[0] = device;
  devices[1] = device_two;

  int positions[2];
  positions[0] = 0;
  positions[1] = 0;

  bool enabled = true;

  picostepper_set_async_enabled(device, enabled);
  picostepper_set_max_speed(device, 60000);
  picostepper_set_min_speed(device, 10000);

  picostepper_set_async_enabled(device_two, enabled);
  picostepper_set_max_speed(device_two, 60000);
  picostepper_set_min_speed(device_two, 10000);

  while(true){
    
    picostepper_set_acceleration(device, 0);
    picostepper_set_acceleration(device_two, 0);

    positions[0] = 4000;
    positions[1] = 2000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);

    positions[0] = 2000;
    positions[1] = 4000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);

    positions[0] = 0;
    positions[1] = 0;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    
    picostepper_set_acceleration(device, 200000);
    picostepper_set_acceleration(device_two, 200000);

    positions[0] = 4000;
    positions[1] = 2000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);

    positions[0] = 2000;
    positions[1] = 4000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);

    positions[0] = 0;
    positions[1] = 0;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
  }
}