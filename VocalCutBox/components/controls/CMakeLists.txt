idf_component_register(SRCS "controls.c" "esp32_s31_adc_calibration.c"
  INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "." REQUIRES esp_adc button esp_timer)
