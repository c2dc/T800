idf_component_register(SRCS "t800.c" "stateless.c" "model.cc" "main_functions.cc"
                    PRIV_REQUIRES tflite-lib spi_flash
                    INCLUDE_DIRS "include")
