import sys
p=sys.argv[1]; s=open(p).read()
T='\t'
old=(T+"if (dir) {\n"+T*2+"switch (devstatus[devname]?.devtype) {\n"+T*2+"case 'vlan':\n"+T*2+"case 'bridge':\n")
new=(T+"if (dir) {\n"+T*2+"/* netifd reports a macvlan as an ethernet device. */\n"
     +T*2+"let devtype = (devstatus[devname]?.type == 'macvlan')\n"
     +T*3+"? 'macvlan' : devstatus[devname]?.devtype;\n\n"
     +T*2+"switch (devtype) {\n"+T*2+"case 'vlan':\n"+T*2+"case 'bridge':\n"+T*2+"case 'macvlan':\n")
assert old in s; s=s.replace(old,new,1); open(p,'w').write(s)
