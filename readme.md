# ESP32 Servo Control with ADC and FreeRTOS Interrupts

This project uses an ESP32 with ESP-IDF to control a servo motor using potentiometer input. The potentiometer is read through the ADC, mapped to a servo pulse width, and used to update the servo position. The project also uses FreeRTOS synchronization primitives such as semaphores and mutexes to handle button interrupts and shared servo control safely.

## Features

- Reads potentiometer input using ADC1
- Maps ADC values to servo pulse widths
- Controls a servo motor using PWM through the LEDC driver
- Uses a GPIO interrupt on the BOOT button
- Uses a FreeRTOS semaphore for interrupt signaling
- Uses a FreeRTOS mutex for safe shared servo access
- Built with ESP-IDF

## Hardware Used

- ESP32 development board
- Servo motor
- Potentiometer
- Jumper wires
- Breadboard

## Pin Configuration

| Component | ESP32 Pin |
|---|---|
| Servo signal | GPIO18 |
| BOOT button | GPIO0 |
| Potentiometer signal | GPIO34 / ADC1 Channel 6 |

## Project Structure

```text
assignment5_servo_interrupt/
├── main/
│   ├── assignment5.c
│   └── CMakeLists.txt
├── CMakeLists.txt
├── sdkconfig
└── README.md

```md
## Demo Video

[Watch the demo video](assets/servo_interrupt_demo.mp4)