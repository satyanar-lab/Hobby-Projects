# DC Motor Speed Control using PWM

## Overview
This project implements DC motor speed control using PWM on an STM32 microcontroller. By varying the PWM duty cycle, the average voltage applied to the motor is changed, which controls the motor speed.

## Objectives
- Generate PWM using STM32 timer peripherals
- Control DC motor speed through duty cycle variation
- Practice embedded firmware development using STM32CubeIDE and HAL drivers

## Tools and Technologies
- STM32 Microcontroller
- Embedded C
- STM32CubeIDE
- STM32 HAL Drivers
- PWM / Timer peripherals

## Project Structure
- `Core/` - application source and header files
- `Drivers/` - HAL and CMSIS driver files
- `.ioc` - STM32CubeMX configuration file
- `.project`, `.cproject`, `.mxproject` - IDE project configuration files

## How to Run
1. Open the project in STM32CubeIDE.
2. Build the project.
3. Flash the code to the STM32 target board.
4. Observe DC motor speed control through PWM output.

## Skills Demonstrated
- Embedded systems programming
- PWM signal generation
- STM32 peripheral configuration
- HAL-based firmware development
- Project organization for version control

## Future Improvements
- Add motor direction control
- Add speed feedback using sensors
- Add closed-loop control using PID

