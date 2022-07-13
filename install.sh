#!/bin/bash

get_idf
cp -r ./new_components/t800 $IDF_PATH/components/
cp -r ./new_components/tflite-lib $IDF_PATH/components/
cp -r ./new_components/esp-nn $IDF_PATH/components/
cp ./new_components/lwip/CMakeLists.txt $IDF_PATH/components/lwip/CMakeLists.txt
cp ./new_components/lwip/lwip/src/core/ipv4/ip4.c $IDF_PATH/components/lwip/lwip/src/core/ipv4/ip4.c
cp ./new_components/lwip/lwip/src/include/lwip/ip4.h $IDF_PATH/components/lwip/lwip/src/include/lwip/ip4.h
