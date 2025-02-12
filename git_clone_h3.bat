set H3_INSTALL=../h3

git clone -b "master"               https://github.com/ARM-software/CMSIS_5.git                     %H3_INSTALL%/CMSIS_5
git clone -b "v3.19.6"              https://github.com/Microchip-MPLAB-Harmony/csp.git              %H3_INSTALL%/csp
git clone -b "master"               https://github.com/Microchip-MPLAB-Harmony/devices.git          %H3_INSTALL%/Devices
git clone -b "v1.5.0"               https://github.com/Microchip-MPLAB-Harmony/harmony-services.git %H3_INSTALL%/harmony-services
git clone -b "master"               https://github.com/Microchip-MPLAB-Harmony/shd.git              %H3_INSTALL%/shd
git clone -b "v1.7.0"               https://github.com/Microchip-MPLAB-Harmony/quick_docs.git       %H3_INSTALL%/quick_docs
git clone -b "v3.18.0"              https://github.com/Microchip-MPLAB-Harmony/dev_packs.git        %H3_INSTALL%/dev_packs
git clone -b "v3.13.5"              https://github.com/Microchip-MPLAB-Harmony/core.git             %H3_INSTALL%/core
