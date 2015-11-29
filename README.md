# ds18b20
Small program that locates all DS18B20 probes in the system
and opens a logfile for each probe and writes the temperature
from each probe to it's logfile.

Arguments:
* -v = Verbose.  Print additional info on what the program is doing.
* -i = Interval. The interval the probes are checked in seconds.
* -l = Log to text files.
* -r = Maintain RRD files.
* -o = Run once. Print temperatures to screen and exit.
* -f = Foreground.  Do not daemonize and run in the foreground.
* -p = Probename. Only report for the named probe or alias. Implies -o and -f

Additionally these arguments can also be set in a config file (/etc/ds18b20.cfg).

RRD support is still pending.

The OS on the Raspberry needs to be properly configured.    In many cases the
following is needed in /boot/config.txt:

dtoverlay=w1-gpio

Also, the drivers/modules "w1-gpio" and "w1-therm" need to be loaded before the
program is run in order for the probes to show up.

It is not recommended to keep the log files for the probes on the SD disk in the PI.
This is because there are values written to these files every INTERVAL (default 5)
minutes.   Such frequent writes will kill your SD card in a short time.
Personally I use the RAM disk for the logs:

ra@raspberrypi ~ $ ls -l /var/probes
lrwxrwxrwx 1 root root 11 Nov 17 04:06 /var/probes -> /run/probes

Then I have a cron job that transfers the logs from the RAM disk to a safe
place every night as the RAM disk is volatile and all data there will be
lost on reboot or powerfailure.

Written by Richard Allen <ra@ra.is> and released under the GPLv2 license.

TODO:
* Finish RRD support.
* Consider mysql/mariadb support.
* Add a Nagios plugin mode.
* Set up min/max threshholds for temperature notifications.

