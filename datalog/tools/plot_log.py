#!/usr/bin/env python3.2
#
# add_log.py <temp.rrd> <data.log>
#

import sys
import subprocess
import os

if len(sys.argv) != 3:
  print("Usage: <data.log> <data.png>", file=sys.stderr)
  sys.exit(1)

log_file = sys.argv[1]
png_file = sys.argv[2]
rrd_file = "tmp.rrd"

create = [
  'rrdtool','create',rrd_file,'--step','60','--start',0,
  'DS:temp:GAUGE:120:-20:50',
  'RRA:AVERAGE:0.5:1:1440',
  'RRA:MIN:0.5:1:1440',
  'RRA:MAX:0.5:1:1440'
]

graph = [
  'rrdtool','graph',png_file,
  '-s',None,'-e',None,
  '--height=200','--width=750',
  '--color=BACK#FFFFFF',
  '--vertical-label', '°C', 
  '--title','temperature',
  'DEF:temp='+rrd_file+':temp:AVERAGE',
  'LINE2:temp#3366CC'
]

samples = []
f = open(log_file,"r")
for line in f:
  elem = line.split(' ')
  temp = float(elem[2])
  ts = int(elem[3],16)
  samples.append((ts,temp))
f.close()

# create rrd
start_ts = samples[0][0]
end_ts = samples[-1][0]
create[6] = str(start_ts - 1)
print("creating rrdb:",create)
ret = subprocess.call(create)
if ret != 0:
  print("ERROR calling: {}".format(" ".join(create)))
  sys.exit(1)
  
# update rrd
n = len(samples)
for i in range(n):
  print(i,n,end="\r")
  sys.stdout.flush()
  s = samples[i]
  entry = "{}:{}".format(s[0],s[1])
  cmd = ['rrdupdate',rrd_file,'--',entry]
  ret = subprocess.call(cmd)
  if ret != 0:
    print("ERROR calling: {}".format(" ".join(cmd)))
    sys.exit(1)

# graph
print("plotting rrdb:",graph)
graph[4] = str(start_ts)
graph[6] = str(end_ts)
ret = subprocess.call(graph)
if ret != 0:
  print("ERROR calling: {}".format(" ".join(cmd)))
  sys.exit(1)
  
os.unlink(rrd_file)
print("done")  
