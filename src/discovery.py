#!/usr/bin/python

"Runs a command on each host of Mininet."

import sys
import os

from mininet.net import Mininet
from mininet.topolib import TreeTopo
from mininet.node import Host
from mininet.node import OVSController
from mininet.util import ensureRoot
from mininet.log import lg

ensureRoot()

depth=1
fanout=3
if len(sys.argv) == 3:
	depth = int(sys.argv[1])
	fanout = int(sys.argv[2])

lg.setLogLevel('info')
topo = TreeTopo(depth=depth, fanout=fanout)
net = Mininet(topo=topo, controller=OVSController)

switch = net['s1']
root = Host('root', inNamespace=False)
switch.linkTo(root)
root.setIP('10.123.123.123/32')

try:
	net.start()
	start_id = 1000;
	for host in net.hosts:
		host.cmd('START_ID=%i ./test_discovery qq 3 10.0.0.1 >/tmp/%s.log &' % (start_id, host.IP()))
		start_id += 1000;
	for host in net.hosts:
		print host.cmd('wait')
finally:
	os.system("grep -h dscvr /tmp/*.*.log | sort -nk1 -t' '; rm -f /tmp/*.*.log")
	net.stop()
