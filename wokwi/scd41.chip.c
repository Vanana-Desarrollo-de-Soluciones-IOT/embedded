#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SCD4X Commands
#define SCD4X_START_PERIODIC_MEASURE    0x21b1
#define SCD4X_READ_MEASUREMENT          0xec05
#define SCD4X_STOP_PERIODIC_MEASURE     0x3f86
#define SCD4X_GET_SERIAL_NUMBER         0x3682
#define SCD4X_GET_DATA_READY_STATUS     0xe4b8
#define SCD4X_SET_TEMPERATURE_OFFSET    0x241d
#define SCD4X_GET_TEMPERATURE_OFFSET    0x2318
#define SCD4X_SET_SENSOR_ALTITUDE       0x2427
#define SCD4X_GET_SENSOR_ALTITUDE       0x2322
#define SCD4X_SET_AMBIENT_PRESSURE      0xe000
#define SCD4X_PERFORM_FORCED_RECALIB    0x362f
#define SCD4X_SET_AUTOMATIC_CALIB       0x2416
#define SCD4X_GET_AUTOMATIC_CALIB       0x2313
#define SCD4X_PERSIST_SETTINGS          0x3615
#define SCD4X_PERFORM_SELF_TEST         0x3639
#define SCD4X_PERFORM_FACTORY_RESET     0x3632
#define SCD4X_REINIT                    0x3646
#define SCD4X_MEASURE_SINGLE_SHOT       0x219d
#define SCD4X_MEASURE_SINGLE_SHOT_RHT_ONLY 0x2196
#define SCD4X_POWER_DOWN                0x36e0
#define SCD4X_WAKE_UP                   0x36f6
#define SCD4X_START_LOW_POWER_MEASURE   0x21ac

// SCD41 I2C Address
#define SCD41_I2C_ADDR 0x62

// Default Simulated Values
#define DEFAULT_CO2 420.0
#define DEFAULT_TEMP 25.0
#define DEFAULT_HUMIDITY 50.0

// CRC calculation
#define CRC8_POLYNOMIAL 0x31
#define CRC8_INIT 0xFF

typedef struct {
  float co2;
  float temp;
  float humidity;
  uint16_t serial_number[3];
  uint16_t temperature_offset;
  uint16_t sensor_altitude;
  uint32_t ambient_pressure;
  bool auto_calib;
  bool periodic_measurement;
  bool low_power_mode;
  bool sleeping;
  uint16_t last_cmd;
  uint8_t cmd_buffer[16];
  uint8_t cmd_len;
  bool reading_cmd;
  uint32_t cycle_step;
  timer_t measurement_timer;
  pin_t sda_pin;
  pin_t scl_pin;
} chip_state_t;

// CRC8 calculation function
static uint8_t calc_crc(uint16_t data) {
  uint8_t crc = CRC8_INIT;
  uint8_t buf[2] = {(data >> 8) & 0xFF, data & 0xFF};
  
  for (int i = 0; i < 2; i++) {
    crc ^= buf[i];
    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ CRC8_POLYNOMIAL;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}

// Function to pack data with CRC
static void pack_data(uint16_t data, uint8_t *buf) {
  buf[0] = (data >> 8) & 0xFF;
  buf[1] = data & 0xFF;
  buf[2] = calc_crc(data);
}

static float interpolate_float(float start, float end, uint32_t step, uint32_t steps) {
  return start + ((end - start) * step) / steps;
}

// Update simulated measurements with a clean-to-polluted cycle.
static void update_measurements(chip_state_t *chip) {
  uint32_t phase = chip->cycle_step % 12;
  float co2 = 450;
  float temp = 25;
  float humidity = 50;

  if (phase < 3) {
    co2 = 450;
    temp = 25;
    humidity = 50;
  } else if (phase < 6) {
    uint32_t step = phase - 3;
    co2 = interpolate_float(450, 1200, step, 2);
    temp = interpolate_float(25, 28, step, 2);
    humidity = interpolate_float(50, 62, step, 2);
  } else if (phase < 9) {
    co2 = 1200;
    temp = 28;
    humidity = 62;
  } else {
    uint32_t step = phase - 9;
    co2 = interpolate_float(1200, 450, step, 2);
    temp = interpolate_float(28, 25, step, 2);
    humidity = interpolate_float(62, 50, step, 2);
  }

  chip->co2 = co2 + (rand() % 31 - 15);
  chip->temp = temp + (rand() % 11 - 5) / 10.0;
  chip->humidity = humidity + (rand() % 7 - 3);
  chip->cycle_step++;
  
  printf("SCD41: Measurement updated - CO2: %.0f ppm, Temp: %.1f C, Hum: %.1f%%\n", 
         chip->co2, chip->temp, chip->humidity);
}

static void on_measurement_timer(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  if (chip->periodic_measurement) {
    update_measurements(chip);
  }
}

// Callback when the chip is addressed on the I2C bus
static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("SCD41: I2C connect - address: 0x%02X, read: %d\n", address, read);
  
  if (!read) {
    // If writing, start receiving a command
    chip->cmd_len = 0;
    chip->reading_cmd = true;
  }
  return true; // ACK
}

// Callback when the microcontroller writes a byte
static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  
  if (chip->cmd_len < sizeof(chip->cmd_buffer)) {
    chip->cmd_buffer[chip->cmd_len++] = data;
  }
  
  // If we have at least 2 bytes, try to process the command
  if (chip->cmd_len >= 2) {
    uint16_t cmd = (chip->cmd_buffer[0] << 8) | chip->cmd_buffer[1];
    chip->last_cmd = cmd;
    
    printf("SCD41: Byte received: 0x%02X, cmd_len: %d, cmd: 0x%04X\n", data, chip->cmd_len, cmd);
    
    // Process immediate commands (no additional data)
    if (chip->cmd_len == 2) {
      switch (cmd) {
        case SCD4X_STOP_PERIODIC_MEASURE:
          chip->periodic_measurement = false;
          chip->low_power_mode = false;
          printf("SCD41: Periodic measurement stopped\n");
          break;
          
        case SCD4X_START_PERIODIC_MEASURE:
          chip->periodic_measurement = true;
          chip->low_power_mode = false;
          update_measurements(chip);
          printf("SCD41: Starting periodic measurement\n");
          break;
          
        case SCD4X_START_LOW_POWER_MEASURE:
          chip->periodic_measurement = true;
          chip->low_power_mode = true;
          update_measurements(chip);
          printf("SCD41: Starting low power measurement\n");
          break;
          
        case SCD4X_POWER_DOWN:
          chip->sleeping = true;
          printf("SCD41: Entering sleep mode\n");
          break;
          
        case SCD4X_WAKE_UP:
          chip->sleeping = false;
          printf("SCD41: Waking up\n");
          break;
          
        case SCD4X_PERFORM_SELF_TEST:
          printf("SCD41: Performing self test\n");
          break;
          
        case SCD4X_REINIT:
          printf("SCD41: Reinitializing\n");
          break;
          
        case SCD4X_PERFORM_FACTORY_RESET:
          chip->temperature_offset = 0;
          chip->sensor_altitude = 0;
          chip->auto_calib = true;
          chip->co2 = DEFAULT_CO2;
          chip->temp = DEFAULT_TEMP;
          chip->humidity = DEFAULT_HUMIDITY;
          printf("SCD41: Factory reset completed\n");
          break;
          
        case SCD4X_PERSIST_SETTINGS:
          printf("SCD41: Settings saved to EEPROM (simulated)\n");
          break;
          
        case SCD4X_MEASURE_SINGLE_SHOT:
          update_measurements(chip);
          printf("SCD41: Single shot measurement completed\n");
          break;
          
        case SCD4X_MEASURE_SINGLE_SHOT_RHT_ONLY:
          update_measurements(chip);
          printf("SCD41: Single shot measurement (RHT only) completed\n");
          break;
          
        default:
          printf("SCD41: Command received: 0x%04X (waiting for more data)\n", cmd);
          break;
      }
    }
    // Process commands with additional data (5 bytes: cmd + data + crc)
    else if (chip->cmd_len >= 5) {
      uint16_t data_val = (chip->cmd_buffer[2] << 8) | chip->cmd_buffer[3];
      uint8_t crc = chip->cmd_buffer[4];
      
      if (crc == calc_crc(data_val)) {
        switch (cmd) {
          case SCD4X_SET_TEMPERATURE_OFFSET:
            chip->temperature_offset = data_val;
            printf("SCD41: Temperature offset set: %u\n", data_val);
            break;
            
          case SCD4X_SET_SENSOR_ALTITUDE:
            chip->sensor_altitude = data_val;
            printf("SCD41: Altitude set: %u m\n", data_val);
            break;
            
          case SCD4X_SET_AMBIENT_PRESSURE:
            chip->ambient_pressure = data_val * 100;
            printf("SCD41: Ambient pressure set: %u Pa\n", chip->ambient_pressure);
            break;
            
          case SCD4X_PERFORM_FORCED_RECALIB:
            printf("SCD41: Forced recalibration with %u ppm\n", data_val);
            break;
            
          case SCD4X_SET_AUTOMATIC_CALIB:
            chip->auto_calib = (data_val == 1);
            printf("SCD41: Auto calibration: %s\n", chip->auto_calib ? "ON" : "OFF");
            break;
            
          default:
            printf("SCD41: Command with data not implemented: 0x%04X\n", cmd);
            break;
        }
      } else {
        printf("SCD41: CRC error in command 0x%04X\n", cmd);
      }
      chip->cmd_len = 0; // Reset after processing
    }
  }
  
  return true; // ACK
}

// Callback when the microcontroller wants to read a byte
static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  static uint8_t response[9];
  static int response_index = 0;
  static int response_len = 0;
  static bool first_call = true;
  
  // Initialize the response on the first read call
  if (first_call) {
    response_index = 0;
    first_call = false;
    
    memset(response, 0, sizeof(response));
    
    switch (chip->last_cmd) {
      case SCD4X_READ_MEASUREMENT: {
        uint16_t co2_value = (uint16_t)chip->co2;
        uint16_t temp_value = (uint16_t)((chip->temp + 45.0) * ((uint32_t)1 << 16) / 175.0);
        uint16_t hum_value = (uint16_t)(chip->humidity * ((uint32_t)1 << 16) / 100.0);
        
        response[0] = (co2_value >> 8) & 0xFF;
        response[1] = co2_value & 0xFF;
        response[2] = calc_crc(co2_value);
        
        response[3] = (temp_value >> 8) & 0xFF;
        response[4] = temp_value & 0xFF;
        response[5] = calc_crc(temp_value);
        
        response[6] = (hum_value >> 8) & 0xFF;
        response[7] = hum_value & 0xFF;
        response[8] = calc_crc(hum_value);
        
        response_len = 9;
        printf("SCD41: Preparing measurement - CO2:%.0f Temp:%.1f Hum:%.1f\n", 
               chip->co2, chip->temp, chip->humidity);
        break;
      }
      
      case SCD4X_GET_SERIAL_NUMBER: {
        pack_data(chip->serial_number[0], &response[0]);
        pack_data(chip->serial_number[1], &response[3]);
        pack_data(chip->serial_number[2], &response[6]);
        response_len = 9;
        printf("SCD41: Preparing serial number\n");
        break;
      }
      
      case SCD4X_GET_DATA_READY_STATUS: {
        uint16_t status = chip->periodic_measurement ? 0x0001 : 0x0000;
        pack_data(status, response);
        response_len = 3;
        printf("SCD41: Data ready status: %s\n", status ? "true" : "false");
        break;
      }
      
      case SCD4X_GET_TEMPERATURE_OFFSET: {
        pack_data(chip->temperature_offset, response);
        response_len = 3;
        printf("SCD41: Sending temperature offset: %u\n", chip->temperature_offset);
        break;
      }
      
      case SCD4X_GET_SENSOR_ALTITUDE: {
        pack_data(chip->sensor_altitude, response);
        response_len = 3;
        printf("SCD41: Sending altitude: %u m\n", chip->sensor_altitude);
        break;
      }
      
      case SCD4X_GET_AUTOMATIC_CALIB: {
        pack_data(chip->auto_calib ? 0x0001 : 0x0000, response);
        response_len = 3;
        printf("SCD41: Auto calibration: %s\n", chip->auto_calib ? "ON" : "OFF");
        break;
      }
      
      case SCD4X_PERFORM_SELF_TEST: {
        pack_data(0x0000, response);
        response_len = 3;
        printf("SCD41: Self test result: OK\n");
        break;
      }
      
      default:
        response_len = 0;
        printf("SCD41: Read not supported for command 0x%04X\n", chip->last_cmd);
        break;
    }
  }
  
  uint8_t byte_to_send = 0;
  if (response_index < response_len) {
    byte_to_send = response[response_index++];
    printf("SCD41: Sending byte %d: 0x%02X\n", response_index-1, byte_to_send);
  }
  
  // If finished sending, reset for next read
  if (response_index >= response_len) {
    first_call = true;
    chip->cmd_len = 0;
  }
  
  return byte_to_send;
}

// Callback when disconnected
static void on_i2c_disconnect(void *user_data) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("SCD41: I2C disconnected\n");
  chip->reading_cmd = false;
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  memset(chip, 0, sizeof(chip_state_t));
  
  // Initialize default values
  chip->co2 = DEFAULT_CO2;
  chip->temp = DEFAULT_TEMP;
  chip->humidity = DEFAULT_HUMIDITY;
  chip->serial_number[0] = 0xbe02;
  chip->serial_number[1] = 0x7f07;
  chip->serial_number[2] = 0x3bfb;
  chip->temperature_offset = 0;
  chip->sensor_altitude = 0;
  chip->ambient_pressure = 101325;
  chip->auto_calib = true;
  chip->periodic_measurement = false;
  chip->low_power_mode = false;
  chip->sleeping = false;
  chip->last_cmd = 0;
  chip->cmd_len = 0;
  chip->reading_cmd = false;
  chip->cycle_step = 0;
  
  // Configure I2C pins
  chip->sda_pin = pin_init("SDA", INPUT_PULLUP);
  chip->scl_pin = pin_init("SCL", INPUT_PULLUP);
  
  // Configure I2C with Wokwi API
  const i2c_config_t i2c_config = {
    .address = SCD41_I2C_ADDR,
    .sda = chip->sda_pin,
    .scl = chip->scl_pin,
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect,
    .user_data = chip,
  };
  
  i2c_init((i2c_config_t*)&i2c_config);
  
  // Create timer for periodic measurements (every 5 seconds)
  const timer_config_t timer_config = {
    .callback = on_measurement_timer,
    .user_data = chip
  };
  chip->measurement_timer = timer_init(&timer_config);
  timer_start(chip->measurement_timer, 5000000, true);
  
  printf("SCD41: Chip simulator initialized at I2C address 0x%02X\n", SCD41_I2C_ADDR);
  printf("SCD41: Serial number: %04X %04X %04X\n", 
         chip->serial_number[0], chip->serial_number[1], chip->serial_number[2]);
}
