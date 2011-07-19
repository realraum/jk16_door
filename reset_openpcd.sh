#!/bin/zsh
local devPath
for idVendor in /sys/bus/usb/devices/*/idVendor; do
  devPath=${idVendor:h} 
  if [[ "$(< $idVendor)" == "16c0" && "$(< ${devPath}/idProduct )" == "076b" ]]; then
    echo -n suspend >! ${devPath}/power/level
    sleep 1
    echo -n on >! ${devPath}/power/level
  fi
done
