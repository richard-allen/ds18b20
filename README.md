# ds18b20
 Small program that locates all DS18B20 probes in the system
 and opens a logfile for each probe and writes the temperature
 from each probe to it's logfile.

 Arguments:
 * -v = Verbose.  Print additional info on what the program is doing.
 * -i = Interval. The interval the probes are checked in seconds.
 * -l = Log to text files
 * -r = Maintain RRD files
 * -o = Run once. Print temperatures to screen and exit
 * -f = Foreground.  Do not daemonize and run in the foreground.

  Additionally these arguments can also be set in the config file.

  RRD support is still pending.

 Written by Richard Allen <ra@ra.is> and released under the GPLv2 license.


TODO:
* Finish RRD support.
* Consider mysql/mariadb support.
* Add a Nagios plugin mode.
* Set up min/max threshholds for temperature notifications.

