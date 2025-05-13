## This is the 'stop run' script for use on the test stand (VME99, connected to detector GS000,
## hosted by machine 'slopebox')

#!/bin/bash -l

echo "EPICS ports:"
echo EPICS_CA_SERVER_PORT=$EPICS_CA_SERVER_PORT
echo EPICS_CA_REPEATER_PORT=$EPICS_CA_REPEATER_PORT

caput Online_CS_StartStop Stop

## this appears to be an arbitrary delay time.
## Once the stop happens, the receiver should shut down of its own accord by
## receipt of the "type F" header indicating 'end of run' from each IOC.
##
## if that is truly the case then there is no need at all to go killing 
## processes.

#####		sleep 20

#####		\rm temp
#####		ps aux | grep ioc99 | grep 'xterm' >temp
#####		while read -r var1 var2 var3
#####		do
#####			echo $var2
#####			kill -2 $var2
#####		done <temp

