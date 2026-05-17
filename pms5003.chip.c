#include "wokwi-api.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PM1 12
#define DEFAULT_PM25 24
#define DEFAULT_PM10 36
#define MAX_PM_VALUE 500
#define MEASUREMENT_INTERVAL_US 1000000  // 1 segundo (realista)
#define RESET_PULSE_MIN_US 10  // Mínimo pulso de reset que detecta (10us)

typedef struct {
  uint16_t pm1;
  uint16_t pm25;
  uint16_t pm10;
  pin_t vcc_pin;
  pin_t gnd_pin;
  pin_t set_pin;
  pin_t reset_pin;
  uart_dev_t uart;
  timer_t measurement_timer;
  timer_t reset_debounce_timer;  // Para anti-rebote del RESET
  bool sleeping;
  uint64_t last_reset_time;
} chip_state_t;

// Declaraciones adelantadas
static void update_measurements(chip_state_t *chip);
static void send_uart_frame(chip_state_t *chip);
static void check_set_pin(chip_state_t *chip);
static void check_reset_pin(chip_state_t *chip);
static void chip_pin_change(void *user_data, pin_t pin, uint32_t value);

static uint16_t clamp_pm_value(int value) {
  if (value < 0) return 0;
  if (value > MAX_PM_VALUE) return MAX_PM_VALUE;
  return (uint16_t)value;
}

static uint16_t add_variation(uint16_t value, uint8_t variation) {
  // Distribución más realista (campana de Gauss aproximada)
  int delta = ((rand() % 3) - 1) * variation / 2;  // -2,0,2
  delta += (rand() % (variation + 1)) - variation/2;  // variación adicional
  return clamp_pm_value((int)value + delta);
}

static void update_measurements(chip_state_t *chip) {
  chip->pm1 = add_variation(DEFAULT_PM1, 4);
  chip->pm25 = add_variation(DEFAULT_PM25, 6);
  chip->pm10 = add_variation(DEFAULT_PM10, 8);

  printf("PM5003: PM1.0=%u ug/m3, PM2.5=%u ug/m3, PM10=%u ug/m3\n",
         chip->pm1, chip->pm25, chip->pm10);
}

static void pack_u16(uint8_t *frame, uint8_t index, uint16_t value) {
  frame[index] = (value >> 8) & 0xff;
  frame[index + 1] = value & 0xff;
}

static void fill_frame(uint8_t *frame, uint16_t pm1, uint16_t pm25, uint16_t pm10) {
  memset(frame, 0, 32);
  frame[0] = 0x42;
  frame[1] = 0x4d;
  frame[2] = 0x00;
  frame[3] = 0x1c;

  pack_u16(frame, 4, pm1);
  pack_u16(frame, 6, pm25);
  pack_u16(frame, 8, pm10);

  uint16_t checksum = 0;
  for (int i = 0; i < 30; i++) {
    checksum += frame[i];
  }

  pack_u16(frame, 30, checksum);
}

static void send_uart_frame(chip_state_t *chip) {
  uint8_t frame[32];
  fill_frame(frame, chip->pm1, chip->pm25, chip->pm10);
  uart_write(chip->uart, frame, sizeof(frame));
}

static void check_set_pin(chip_state_t *chip) {
  bool set_level = pin_read(chip->set_pin);
  if (set_level == LOW && !chip->sleeping) {
    chip->sleeping = true;
    timer_stop(chip->measurement_timer);
    printf("PM5003: Entering SLEEP mode\n");
  } else if (set_level == HIGH && chip->sleeping) {
    chip->sleeping = false;
    timer_start(chip->measurement_timer, MEASUREMENT_INTERVAL_US, true);
    printf("PM5003: Waking up from SLEEP mode\n");
    update_measurements(chip);
    send_uart_frame(chip);
  }
}

static void check_reset_pin(chip_state_t *chip) {
  bool reset_level = pin_read(chip->reset_pin);
  if (reset_level == LOW) {
    printf("PM5003: RESET signal detected\n");
    if (chip->sleeping) {
      chip->sleeping = false;
      timer_start(chip->measurement_timer, MEASUREMENT_INTERVAL_US, true);
    }
    update_measurements(chip);
    send_uart_frame(chip);
  }
}

// Callback para cambios en los pines
static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (pin == chip->set_pin) {
    check_set_pin(chip);
  } else if (pin == chip->reset_pin) {
    check_reset_pin(chip);
  }
}

static void on_measurement_timer(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (!chip->sleeping) {
    update_measurements(chip);
    send_uart_frame(chip);
  }
}

void chip_init(void) {
  chip_state_t *chip = calloc(1, sizeof(chip_state_t));
  if (!chip) {
    return;
  }

  chip->pm1 = DEFAULT_PM1;
  chip->pm25 = DEFAULT_PM25;
  chip->pm10 = DEFAULT_PM10;
  chip->sleeping = false;
  
  // Inicializar pines
  chip->vcc_pin = pin_init("VCC", INPUT);
  chip->gnd_pin = pin_init("GND", INPUT);
  chip->set_pin = pin_init("SET", INPUT_PULLUP);
  chip->reset_pin = pin_init("RESET", INPUT_PULLUP);

  // Configurar UART
  const uart_config_t uart_config = {
    .tx = pin_init("TX", OUTPUT),
    .rx = pin_init("RX", INPUT),
    .baud_rate = 9600,
    .user_data = chip,
  };
  chip->uart = uart_init(&uart_config);

  // Timer para mediciones
  const timer_config_t timer_config = {
    .callback = on_measurement_timer,
    .user_data = chip,
  };
  chip->measurement_timer = timer_init(&timer_config);
  timer_start(chip->measurement_timer, MEASUREMENT_INTERVAL_US, true);

  // Configurar monitoreo de pines con pin_watch (API correcta)
  const pin_watch_config_t set_watch_config = {
    .edge = BOTH,           // Detectar cualquier cambio (HIGH->LOW o LOW->HIGH)
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->set_pin, &set_watch_config);

  const pin_watch_config_t reset_watch_config = {
    .edge = FALLING,        // Solo detectar flanco de bajada (HIGH->LOW)
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->reset_pin, &reset_watch_config);

  printf("PM5003: Chip simulator initialized at 9600 baud\n");
  printf("  - SET pin: HIGH = normal, LOW = sleep mode\n");
  printf("  - RESET pin: LOW pulse = reset module\n");
  //Simular tiempo de arranque
  printf("PM5003: Power-on, warming up...\n");
  timer_start(chip->measurement_timer, 3000000, false);  // Primera medición a los 3s
  timer_start(chip->measurement_timer, MEASUREMENT_INTERVAL_US, true);  // Luego cada 1s
}