## This is the 'start run' script for use on the test stand (VME99, no detector,
## hosted by machine 'slopebox')

#!/bin/bash -l
   export TERM=vt100
   echo " terminals" 

echo "EPICS ports:"   
echo EPICS_CA_SERVER_PORT=$EPICS_CA_SERVER_PORT
echo EPICS_CA_REPEATER_PORT=$EPICS_CA_REPEATER_PORT

caput Online_CS_StartStop Stop
caput Online_CS_SaveData Save
caput GLBL:DIG:GeC_disc_width 15  # this is to just for safety
caput GLBL:DIG:BGOs_disc_width 15  # this is to just for safety
#sleep 2

# Now spawn receiver window for IOC99

caput Online_CS_StartStop Start


xterm -T vme99  -geometry 157x10+0+0     -sb -sl 1000 -e "./dgsReceiver" "vme99"  "TestStand_run$1" "gtd01" "2000000000" "14" &
sleep 3
caput Online_CS_StartStop Stop
#sleep 10

##/global/ioc/gui/scripts/run_save.sh dgs_run$1/run$1.save
##
#wait 10
##echo All Done - Terminal Window is Free to use.

