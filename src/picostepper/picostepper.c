/**
 * Copyright (c) 2021 Bjarne Dasenbrook
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "picostepper.h"

// Object containing all status-data and device-handles
struct PicoStepperContainer psc;
bool psc_is_initialised = false;

static void picostepper_async_handler() {
  // Safe interrupt value and clear the interrupt
  uint32_t interrupt_request = dma_hw->ints0;
  dma_hw->ints0 = interrupt_request;

  // Find the channel that invoked the interrupt
  for (size_t bit_position = 0; bit_position < 15; bit_position++)
  {
    int flag = interrupt_request & (1 << bit_position);
    if(flag == 0) {
      continue;
    }

    int dma_channel = bit_position;
    if (dma_channel == -1)
    {
      continue;
    }
    PicoStepper device = (PicoStepper) psc.map_dma_ch_to_device_index[dma_channel];
    // Invoke callback for device
    psc.devices[device].is_running = false;
    if(psc.devices[device].callback != NULL) {
      (*psc.devices[device].callback)(device);
    }
  }
}

// Create a PicoStepperRawDevice
static PicoStepperRawDevice picostepper_create_raw_device() {
  PicoStepperRawDevice psrq;
  psrq.is_configured = false;
  psrq.is_running = false;
  psrq.direction = true;
  psrq.steps = 0;
  psrq.position = 0;
  psrq.acceleration = 0;
  psrq.enabled = false;
  psrq.command = 0;
  psrq.pio_id = -1;
  psrq.callback = NULL;
  psrq.dma_config = dma_channel_get_default_config(0);
  psrq.delay = 1;
  return psrq;
}

// Create the PicoStepperContainer if yet not created
static void picostepper_psc_init() {
  if(psc_is_initialised) {
    return;
  }
  psc.max_device_count = 8;
  for (size_t i = 0; i < psc.max_device_count; i++)
  {
    psc.device_with_index_is_in_use[i] = false;
    psc.devices[i] = picostepper_create_raw_device();
  }
  for (size_t i = 0; i < 32; i++)
  {
    psc.map_dma_ch_to_device_index[i] = -1;
  }
  psc_is_initialised = true;
  return;
}

// Allocate hardware resources for a stepper
static PicoStepper picostepper_init_unclaimed_device() {
  // Initialize the PicoStepperContainer (if neccessary)
  picostepper_psc_init();
  // Select an PicoStepper device that is not in use (if possible)
  int unclaimed_device_index = -1;
  for (size_t i = 0; i < psc.max_device_count; i++)
  {
    if(!psc.device_with_index_is_in_use[i]) {
      unclaimed_device_index = i;
      break;
    }
  }
  // Error: No free resources to use for the stepper
  if (unclaimed_device_index == -1)
  {
    return (PicoStepper) -1;
  }
  // Select a PIO-Block and statemachine to generate the signal on
  PIO pio_block = pio0;
  int pio_id = 0;
  uint statemachine = pio_claim_unused_sm(pio_block, false);
  // Error: No free resources to use for the stepper
  if(statemachine == -1) {
    pio_block = pio1;
    pio_id = 1;
    statemachine = pio_claim_unused_sm(pio_block, true);
  }
  // Select a DMA-Channel and configure it
  int dma_ch = dma_claim_unused_channel(true);
  dma_channel_config dma_conf = dma_channel_get_default_config(dma_ch);
  channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_conf, false);
  // DREQ_PIO0_TX0 => 0x0, DREQ_PIO0_TX1 => 0x1, DREQ_PIO0_TX2 => 0x2, DREQ_PIO0_TX3 => 0x3
  // DREQ_PIO1_TX0 => 0x8, DREQ_PIO1_TX1 => 0x9, DREQ_PIO1_TX2 => 0xa, DREQ_PIO1_TX3 => 0xb
  uint drq_source = (pio_id * 8) + statemachine;
  channel_config_set_dreq(&dma_conf, drq_source);
  dma_channel_set_irq0_enabled(dma_ch, true);
  irq_set_exclusive_handler(DMA_IRQ_0, picostepper_async_handler);
  irq_set_enabled(DMA_IRQ_0, true);
  // Update the state of the device and mark it as allocated
  psc.map_dma_ch_to_device_index[dma_ch] = unclaimed_device_index;
  psc.device_with_index_is_in_use[unclaimed_device_index] = true;
  psc.devices[unclaimed_device_index].pio = pio_block;
  psc.devices[unclaimed_device_index].pio_id = pio_id;
  psc.devices[unclaimed_device_index].statemachine = statemachine;
  psc.devices[unclaimed_device_index].dma_channel = dma_ch;
  psc.devices[unclaimed_device_index].dma_config = dma_conf;
  psc.devices[unclaimed_device_index].is_running = false;
  psc.devices[unclaimed_device_index].is_configured = true; 
  return (PicoStepper) unclaimed_device_index;
}

PicoStepper picostepper_init(uint base_pin, PicoStepperMotorType driver) {
  // Choose the programm to generate the steps
  const pio_program_t * picostepper_driver_program;
  //picostepper_driver_program = &picostepper_four_wire_program;
  switch(driver){
    case FourWireDriver: 
      picostepper_driver_program = &picostepper_four_wire_program;
      break;

    case TwoWireDriver:
      picostepper_driver_program = &picostepper_two_wire_program;
      break;

    // Other drivers are not yet implemented
    default:  return -1;
  }
  // Create picostepper object and claim pio resources
  PicoStepper device = picostepper_init_unclaimed_device();
  // Load the driver program into the pio instruction memory
  uint picostepper_program_offset = pio_add_program(psc.devices[device].pio, picostepper_driver_program);
  // Init PIO program

  switch(driver){
    case FourWireDriver:   
      picostepper_four_wire_program_init(psc.devices[device].pio, psc.devices[device].statemachine, picostepper_program_offset, base_pin, CLKDIV);
      break;

      case TwoWireDriver:   
      picostepper_two_wire_program_init(psc.devices[device].pio, psc.devices[device].statemachine, picostepper_program_offset, base_pin, CLKDIV);
      break;
      
    // Other drivers are not yet implemented
    default:  return -1;
  }
  
  
  return device;
}

PicoStepper picostepper_pindef_init(uint dir_pin, uint step_pin, PicoStepperMotorType driver) {
  // Choose the programm to generate the steps
  const pio_program_t * picostepper_driver_program;
  // Create picostepper object and claim pio resources
  PicoStepper device = picostepper_init_unclaimed_device();

  if(dir_pin > step_pin){

  picostepper_driver_program = &picostepper_two_wire_program;
  // Load the driver program into the pio instruction memory
  uint picostepper_program_offset = pio_add_program(psc.devices[device].pio, picostepper_driver_program);
  // Init PIO program
  picostepper_two_wire_program_init(psc.devices[device].pio, psc.devices[device].statemachine, picostepper_program_offset, step_pin, CLKDIV);

  } else {

    picostepper_driver_program = &picostepper_two_wire_reversed_program;
    // Load the driver program into the pio instruction memory
    uint picostepper_program_offset = pio_add_program(psc.devices[device].pio, picostepper_driver_program);
    // Init PIO program
    picostepper_two_wire_reversed_program_init(psc.devices[device].pio, psc.devices[device].statemachine, picostepper_program_offset, dir_pin, CLKDIV);

  }
  
  return device;
}

// Move the stepper and wait for it to finish before returning from the function
// Warning: Care must be taken whan configurating a delay_change value to
//          prevent integer under or overflows in the resulting delay!!! 
bool picostepper_move_blocking(PicoStepper device, uint steps, bool direction, uint delay, int delay_change) {
  //Error: picostepper delay to big (greater than (2^30)-1
  if (delay > 1073741823) {
      return false;
  }
  // Error: No valid PicoStepper device
  if(device == -1) {
    return false;
  }
  // Error: Device is already running.
  if(psc.devices[device].is_running) {
    return false;
  }
  // Set device status to running
  psc.devices[device].is_running = true;
  // For each step submit a step command to the pio
  uint32_t command;
  uint calculated_delay = delay;
  for (size_t i = 0; i < steps; i++)
  {
    command = (((calculated_delay << 1) | (direction ^ DRIVER)) << 1 ) | 1;
    pio_sm_put_blocking(psc.devices[device].pio, psc.devices[device].statemachine, command);
    calculated_delay += delay_change;
  }
  // Wait until the statemachine has consumed all commands from the buffer
  while (pio_sm_is_tx_fifo_empty(psc.devices[device].pio, psc.devices[device].statemachine) == false);
  // Set device status to not running
  psc.devices[device].is_running = false;
  return true;
}

// Set the direction used by picostepper_move_async.
// Can be set during a running async movement.
void picostepper_set_async_direction(PicoStepper device, bool direction) {
  psc.devices[device].direction = direction ^ DRIVER;
  psc.devices[device].command = (((psc.devices[device].delay << 1) | psc.devices[device].direction) << 1 )| psc.devices[device].enabled;
}

// Set the enabled state for step-commands send by picostepper_move_async
// Can be set during a running async movement.
void picostepper_set_async_enabled(PicoStepper device, bool enabled) {
  psc.devices[device].enabled = enabled;
  psc.devices[device].command = (((psc.devices[device].delay << 1) | psc.devices[device].direction) << 1 )| psc.devices[device].enabled;
}

bool picostepper_get_async_enabled(PicoStepper device){
  return psc.devices[device].enabled;
}

// Set the delay between steps used by picostepper_move_async.
// Can be set during a running async movement.
void picostepper_set_async_delay(PicoStepper device, uint delay) {
  psc.devices[device].delay = max(delay, MINDELAY);
  psc.devices[device].command = (((psc.devices[device].delay << 1) | psc.devices[device].direction) << 1 )| psc.devices[device].enabled;
}

void picostepper_set_async_speed(PicoStepper device, uint speed){
  picostepper_set_async_delay(device, picostepper_convert_speed_to_delay(speed));
}

// Set the acceleration for the stepper
void picostepper_set_acceleration(PicoStepper device, uint acceleration){
  psc.devices[device].acceleration = acceleration;
}

// Set the steppers internal position value
void picostepper_set_position(PicoStepper device, uint position){
  psc.devices[device].position = position;
}

// Set the steppers internal position value
void picostepper_set_max_speed(PicoStepper device, uint speed){
  psc.devices[device].max_speed = speed;
}

// Set the steppers internal position value
void picostepper_set_min_speed(PicoStepper device, uint speed){
  psc.devices[device].min_speed = speed;
  psc.devices[device].delay = picostepper_convert_speed_to_delay(speed);
}

// Invert the delay to speed math, calculate a delay given a speed and the known maximum steprate
int picostepper_convert_speed_to_delay(float steps_per_second) {
  int delay = max((int) (double) (10*MAXSTEPRATE)/(double) steps_per_second-10, 0);
  return delay;
}

// Thanks to Jersey for helping with this math
int picostepper_convert_delay_to_speed(int delay){
  int step_rate = (int) (double) MAXSTEPRATE/(((double) delay/10)+1);
  return step_rate;
}


// Move the stepper ans imidiatly return from function without waiting for the movement to finish
bool picostepper_move_async(PicoStepper device, int steps, PicoStepperCallback func) {

  if(psc.devices[device].is_running || !psc.devices[device].is_configured ) {
    return false;
  }

  if(psc.devices[device].delay == 0) psc.devices[device].delay = picostepper_convert_speed_to_delay(psc.devices[device].min_speed);

  psc.devices[device].callback = func;
    
  if(psc.devices[device].pio_id == 0) {
    dma_channel_configure(
        psc.devices[device].dma_channel,
        &psc.devices[device].dma_config,
        &pio0_hw->txf[psc.devices[device].statemachine], // Write address (only need to set this once)
        NULL,             // Don't provide a read address yet
        steps,            // Write the same value many times, then halt and interrupt
        false             // Don't start yet
    );
  } else {
    dma_channel_configure(
        psc.devices[device].dma_channel,
        &psc.devices[device].dma_config,
        &pio1_hw->txf[psc.devices[device].statemachine], // Write address (only need to set this once)
        NULL,             // Don't provide a read address yet
        steps,            // Write the same value many times, then halt and interrupt
        false             // Don't start yet
    );
  }
  // Set read address for the dma (switching between two buffers has not been implemented) and start transmission
  dma_channel_set_read_addr(psc.devices[device].dma_channel, &psc.devices[device].command, true);
  psc.devices[device].is_running = true;

  return true;
}

// Handle stepper acceleration as an async callback
void picostepper_accelerate(volatile PicoStepper device){
  // Base case, if direction is 0 we are coasting, do nothing
  if(psc.devices[device].acceleration_direction == 0 || psc.devices[device].moving_acceleration == 0) return;

  //printf("Coasting slices %d\n", psc.devices[device].coasting_slices);

  // If direction is negative, we are decelerating
  if(psc.devices[device].acceleration_direction < 0){

    // Pop the new delay value off the speed stack
    uint delay = psc.devices[device].coasting_slices-- > 0 ? psc.devices[device].delay : pop(&psc.devices[device].stack);

    //printf("Decelerating to %d\n", delay);

    picostepper_set_async_delay(device, delay);
    return;
  }

  // If direction is positive, we are accelerating
  if(psc.devices[device].acceleration_direction > 0){

    // Record the current delay value and push it to the speed stack
    uint delay = psc.devices[device].delay;
    uint init_delay = delay;

    // Calculate the current speed based on the delay
    uint speed = picostepper_convert_delay_to_speed(delay);

    // Determine how many steps per second to increase speed based on acceleration and current speed
    double time_s = (double) NUMSTEPS/ (double) speed;
    speed += (uint) ((double) psc.devices[device].moving_acceleration)*time_s;

    // If the delay is too small to make a change, decrease delay by 1. If we are already at the minimum delay (maximum speed), keep it there
    uint calculated_delay = picostepper_convert_speed_to_delay(speed);
    calculated_delay = calculated_delay == delay ? delay - 1 : calculated_delay;

    uint min_delay = picostepper_convert_speed_to_delay(psc.devices[device].max_speed);
    delay = max(calculated_delay, min_delay);

    if(delay == min_delay){
      psc.devices[device].coasting_slices++;
      //printf("Coasting at %d\n", delay);
    } else {
      push(&psc.devices[device].stack, init_delay);
      //printf("Init at %d Accelerating to %d\n", init_delay, delay);
    }

    picostepper_set_async_delay(device, delay);
    return;
  }
}

// Take a number of steps as a position value and move to it applying acceleration as needed
bool picostepper_move_to_position(volatile PicoStepper device, int position){

  // Determine the number of steps to be taken, and in which direction
  uint current_position = psc.devices[device].position;
  uint steps = abs(position - current_position);
  bool direction = position > current_position;

  // Update the tracked position value
  psc.devices[device].position = position;
  psc.devices[device].delay = picostepper_convert_speed_to_delay(psc.devices[device].min_speed);
  psc.devices[device].coasting_slices = 0;
  psc.devices[device].moving_acceleration = psc.devices[device].acceleration;

  picostepper_set_async_direction(device, direction);

  // Split the movement into even slices for accelerating and decelerating
  uint acceleration_steps = steps/NUMSTEPS;

  // Accelerate to maximum speed taking NUMSTEPS per slice (this can be tuned as needed, but is accounted for later)
  psc.devices[device].acceleration_direction = 1;
  for(uint i=0; i<acceleration_steps/2; i++){
    picostepper_move_async(device, NUMSTEPS, &picostepper_accelerate);
    while(psc.devices[device].is_running) sleep_us(10);
  }

  // Consume any extra steps that prevent an even split, coast at current speed rather than changing it
  psc.devices[device].acceleration_direction = 0;
  if(steps%NUMSTEPS > 0){
    picostepper_move_async(device, steps%NUMSTEPS, NULL);
    while(psc.devices[device].is_running) sleep_us(10);
  }

  // Decelerate in the same fashion as accelerating
  psc.devices[device].acceleration_direction = -1;
  for(uint i=0; i<acceleration_steps/2; i++){
    picostepper_move_async(device, NUMSTEPS, &picostepper_accelerate);
    while(psc.devices[device].is_running) sleep_us(10);
  }

  return true;
}

// Take a number of steps as a position value and move to it applying acceleration as needed for multiple steppers
// All the loops feel a bit redundant, might want to try and look into consolidating them better
bool picostepper_move_to_positions(volatile PicoStepper devices[], int positions[], uint num_steppers, bool sequential){

  // Setup trackers for the various steppers
  bool stepper_directions[num_steppers];
  uint stepper_steps[num_steppers];
  uint step_differences[num_steppers];
  uint steps = 0;
  uint acceleration = psc.devices[devices[0]].acceleration;
  uint most_steps = 0;
  bool no_move_needed = true;

  for(uint stepper; stepper < num_steppers; stepper++) no_move_needed &= positions[stepper] == psc.devices[devices[stepper]].position;
  if(no_move_needed) return true;

  //bool sequential = abs(steppers[0] - steppers[1]) <= NUMSTEPS;

  //printf("Need to move\n");

  // Determine what each stepper should do
  for(uint stepper; stepper < num_steppers; stepper++){

    // Determine the number of steps to be taken, and in which direction
    uint current_position = psc.devices[devices[stepper]].position;
    uint steps_to_take = abs(positions[stepper] - current_position);
    uint direction = positions[stepper] > current_position;
    //printf("Stepper %d: current: %d, target: %d, steps: %d\n", stepper, current_position, positions[stepper], steps_to_take);
    stepper_directions[stepper] = direction;
    stepper_steps[stepper] = steps_to_take;
    step_differences[stepper] = steps_to_take;

    // Update the tracked position value and direction
    psc.devices[devices[stepper]].position = positions[stepper];
    picostepper_set_async_direction(devices[stepper], direction);

    // Determine which motor has the least distance to travel to set the number of slices
    steps = steps == 0 ? steps_to_take : min(steps, steps_to_take);
    acceleration = min(acceleration, psc.devices[devices[stepper]].acceleration);
    most_steps = max(most_steps, steps_to_take);

  }

  //printf("Stepper movemenets determiend\n");

  // Split the movement into even slices for accelerating and decelerating
  //uint acceleration_steps = steps/NUMSTEPS;
  uint acceleration_steps = most_steps/NUMSTEPS;
  if (psc.devices[devices[0]].acceleration == 0 || psc.devices[devices[1]].acceleration == 0){
    acceleration_steps = 0;
  }

  //printf("Acceleration steps: %d\n", acceleration_steps);

  // Set each steppers acceleration direction and acceleration rate
  // By using the slowest of the two accelerations, we ensure that neither stepper has to wait on the other per each slice
  for(int stepper = 0; stepper < num_steppers; stepper++){
    psc.devices[devices[stepper]].acceleration_direction = 1;
    psc.devices[devices[stepper]].moving_acceleration = (double) (acceleration*stepper_steps[stepper]) / (double) most_steps;
  }

  //printf("Accelerations set\n");

  // Determine the number of steps to take per slice
  // Ex: Stepper_1 moves 1200 steps
  // Stepper_2 moves 600 steps
  // To arrive at the same time, per slice, stepper_1 moves 200 steps while stepper_2 moves 100
  for(int stepper = 0; stepper < num_steppers; stepper++){
    if(acceleration_steps) stepper_steps[stepper] = stepper_steps[stepper]/acceleration_steps;
  }

  //printf("Steps per slice calculated\n");

  // Accelerate for the first half of the slices
  for(int i=0; i<acceleration_steps/2; i++){
    for(int stepper = 0; stepper < num_steppers; stepper++){
      //printf("Stepper %d taking %d steps with acceleration %d\n", stepper, stepper_steps[stepper], psc.devices[devices[stepper]].moving_acceleration);
      if(stepper_steps[stepper] > MINSTEPS){
        picostepper_move_async(devices[stepper], stepper_steps[stepper], &picostepper_accelerate);
        step_differences[stepper] -= stepper_steps[stepper]*2;
      } else step_differences[stepper] += stepper_steps[stepper]*2;

      if(sequential) while(psc.devices[devices[stepper]].is_running);
    }
    if(!sequential){
      // Wait for all motors to finish moving before proceeding
      for(int stepper = 0; stepper < num_steppers; stepper++){
        while(psc.devices[devices[stepper]].is_running);
      }
    }
  }

  //printf("Done accelerating\n");

  sleep_us(5);

  // We consume any remaining steps here 
  // This may cause motors to arrive NUMSTEPS out of sync, with a small enough value this isn't a problem yet
  for(int stepper = 0; stepper < num_steppers; stepper++){
    //printf("Stepper %d coasting %d steps\n", stepper, step_differences[stepper]);
    psc.devices[devices[stepper]].acceleration_direction = 0;
    if(step_differences[stepper] > 0) picostepper_move_async(devices[stepper], step_differences[stepper], NULL);
    if(sequential) while(psc.devices[devices[stepper]].is_running);
  }

  if(!sequential){
      // Wait for all motors to finish moving before proceeding
      for(int stepper = 0; stepper < num_steppers; stepper++){
        while(psc.devices[devices[stepper]].is_running);
      }
    }

  sleep_us(5);

  //printf("Done coasting\n");

  // Set acceleration direction to decelerate
  for(int stepper = 0; stepper < num_steppers; stepper++){
    psc.devices[devices[stepper]].acceleration_direction = -1;
  }

  // Begin decelerating
  for(int i=0; i<acceleration_steps/2; i++){
    for(int stepper = 0; stepper < num_steppers; stepper++){
      if(stepper_steps[stepper] > MINSTEPS) picostepper_move_async(devices[stepper], stepper_steps[stepper], &picostepper_accelerate);
      if(sequential) while(psc.devices[devices[stepper]].is_running);
    }
    if(!sequential){
      // Wait for all motors to finish moving before proceeding
      for(int stepper = 0; stepper < num_steppers; stepper++){
        while(psc.devices[devices[stepper]].is_running);
      }
    }
  }

  sleep_us(5);

  //printf("Done decelerating\n");

  return true;
}