#include <homekit/homekit.h>
#include <homekit/characteristics.h>

homekit_characteristic_t homekitOnOffCharacteristic = HOMEKIT_CHARACTERISTIC_(ON, true);

homekit_accessory_t * homekitAccessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Equinox Clock"),
      HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Arduino HomeKit"),
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
      HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, NULL),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      &homekitOnOffCharacteristic,
      HOMEKIT_CHARACTERISTIC(NAME, "Lights"),
      NULL
    }),
    NULL
  }),
  NULL
};

homekit_server_config_t homekitConfiguration = {
  .accessories = homekitAccessories,
  .password = "111-11-111"
};
