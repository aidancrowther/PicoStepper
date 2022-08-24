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
  uint base_pin = 20;
  PicoStepper device = picostepper_pindef_init(21, 20, TwoWireDriver);
  PicoStepper device_two = picostepper_pindef_init(18, 19, TwoWireDriver);

  uint delay = 5;
  bool direction = true;
  bool enabled = true;
  uint steps = 10;

  PicoStepper devices[2];
  int positions[2];

  devices[0] = device;
  devices[1] = device_two;

  picostepper_set_async_enabled(device, enabled);
  picostepper_set_async_enabled(device_two, enabled);

  //picostepper_move_async(device, steps, &movement_finished);
  //picostepper_move_async(device_two, steps, &movement_finished);

  picostepper_set_async_delay(device, 100);
  picostepper_set_async_delay(device_two, 100);

  while(true){
    picostepper_set_acceleration(device, 0);
    picostepper_set_acceleration(device_two, 0);
    positions[0] = 4000;
    positions[1] = 0;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    positions[0] = 2050;
    positions[1] = 6000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    positions[0] = 6000;
    positions[1] = 20500;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    positions[0] = 0;
    positions[1] = 4000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    picostepper_set_acceleration(device, 400000);
    picostepper_set_acceleration(device_two, 200000);
    positions[0] = 4000;
    positions[1] = 0;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    positions[0] = 2050;
    positions[1] = 6000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    positions[0] = 6000;
    positions[1] = 2050;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
    positions[0] = 0;
    positions[1] = 4000;
    picostepper_move_to_positions(devices, positions, 2);
    sleep_ms(1000);
  }

/*
  while(true){
    picostepper_set_async_direction(device_two, true);
    picostepper_set_async_direction(device, false);
    sleep_ms(5000);
    picostepper_set_async_direction(device_two, false);
    picostepper_set_async_direction(device, true);
    sleep_ms(5000);
  }
  */

  while (true) {
    picostepper_set_async_direction(device, true);
    // Accelerate
    bool accelerating = true;
    uint speed = 10000;
    while(accelerating)
    {
      if(speed >= 80000) break;
      delay = picostepper_convert_speed_to_delay(speed);
      picostepper_set_async_delay(device, delay);
      sleep_ms(100);
      speed += 200;
    }

    sleep_ms(5000);

    //Deaccelerate
    for (size_t i = 200; i > 1; i--)
    {
      delay = picostepper_convert_speed_to_delay(i*250);
      picostepper_set_async_delay(device, delay);
      sleep_ms(5);
    }
    sleep_ms(5000); //reverse
    /*picostepper_set_async_direction(device, false);
    sleep_ms(5000);
    // Accelerate
    for (size_t i = 1000; i > 5; i--)
    {
      picostepper_set_async_delay(device, i);
      sleep_ms(5);
    }

    sleep_ms(5000);

    //Deaccelerate
    for (size_t i = 5; i < 1000; i++)
    {
      picostepper_set_async_delay(device, i);
      sleep_ms(5);
    }*/
  }
}