#!/bin/bash

# daily  restart service file  daily update  for date
SER_DAILY="$(date +%F).txt"

# folder for service file path daily update date 
FILE_PATH="/home/zedbee/service/$SER_DAILY"

mkdir -p /home/zedbee/service

# this for array in script. any files add for service files. like services=(iaq.service, linux.service)
services=(iaq.service)

# for loop for script
for srv in "${services[@]}"; do

    # this for systemctl restart files give. this daemon process  
    if  sudo systemctl restart "$srv" ; then

        # this for show for output       
        echo "$srv restarted successfully at $(date)" >> "$FILE_PATH"

    else 
        echo "failed to restarted at $(date)" >> "$FILE_PATH"
    fi

done
