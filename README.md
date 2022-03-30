EIGRP
=========

EIGRP is free software that implements RFC 7868 (https://www.rfc-editor.org/info/rfc7868).
This is a clone of, and rewrite of, the code found int the FRR (https://github.com/FRRouting/frr).
While EIGRP is designed be protocol agnostic, it currently only handles IPv4.


Roadmap
------------------

First and foremost, I agree with evolution (small commits) vs revolution (large commits).  That said, significant changes are needed to refactor the code to address issues, scale, and missing capabilities within the code. This should not be considered as disrespecting the original authors or editors, in fact i have complete respect and admiration for them.

To that point, and due due to the initial effort needed for the wide-metric work, I have cloned EIGRP from FRR and created an independent repo that i can perform small commits to.  Each of the commits i perform have minimal to now testing and a high chance of destabilizing EIGRP code base. My intent is to wrap up the refactor work along with the wide metrics and bases to support IPv6.  Then deliver a finished, commercial ready, version of the EIGRP code. To ease the adoption, I have gone to lengths to ensure what's being done is a drop in overlay to FRR.

I completely understand the repo-owners aversion to large code drops, but I just don't see a clean way to deliver stable code which is not also a large change set.

As for code refactoring.  My goals are as follows.
   1) make it readable!!!! use function scoping, and CRUD model
   2) Add wide metrics in - this involves major edits to metrics, neighbors, and interface code
   3) make it modular (use typedef to hide data, create functions that align with data objects)
   4) Add redistribution
   5) Add Query Filtering (support stub like capability without doing 'STUBS')
   6) Add IPv6 support
   7) Anything else that sounds like fun


Installation & Use
------------------

This is not a standalone repo.  To contribute to the development of this code, setup you development environment per the FRR documentation:

```
	http://docs.frrouting.org/projects/dev-guide/en/latest/building.html
```

Next, download EIGRP source

```
	git clone https://github.com/diivious/eigrpd.git eigrpd
```

Once setup, checkout the FRR source

```
	git clone https://github.com/frrouting/frr.git FRR
```

Delete the existing eigrpd directory

```
	cd frr
	rm -rf eigrpd
```

Copy this version of EIGRP top FRR

```
	cp -R ../eigrpd .
```

Lastly, build FRR as normal

```
	./bootstrap.sh
	./configure \
	    --enable-exampledir=/usr/share/doc/frr/examples/ \
	    --localstatedir=/var/opt/frr \
	    --sbindir=/usr/lib/frr \
	    --sysconfdir=/etc/frr \
	    --enable-multipath=64 \
	    --enable-user=frr \
	    --enable-group=frr \
	    --enable-vty-group=frrvty \
	    --enable-configfile-mask=0640 \
	    --enable-logfile-mask=0640 \
	    --enable-fpm \
	    --with-pkg-git-version \
	    --with-pkg-extra-version=-dVs-EIGRP-v0
	make
```

Debugging
---------
You need 2 shells - one for frr vtysh commands and one user shell to
debug eigrp:

In your root shell:
```
   #See if frr is running
   systemctl status frr

   #Check to see if frr is set to autostart eigrpd in
   #/etc/frr/daemons, if so set it to "no", before starting frr

   # if its running, then stop it
   systemctl stop frr

   # anf lastly, start it
   systemctl start frr

   #if your like me, paranoid, then kill the eigrpd process just in case
   kill -9 `pidof eigrpd`
```

In your user shell - EIGRP:
```
   sudo gdb eigrpd/.libs/eigrpd 
```

In your root shell:
```
  vtysh -c 'configure terminal' -c 'router eigrp 4453' -c 'network 0.0.0.0/0' 
  vtysh -c 'show running-config'
  vtysh -c 'show ip eigrp top'
  vtysh -c 'show ip eigrp nei'
```

Contributing
------------

I welcome and appreciate all contributions, no matter how small!


Security
--------

To report security issues, please email me.

```
diivious [at] hotmail.com
```
