# The real build is GNUmakefile (GNU make syntax). GNU make reads that file
# first and never sees this one; BSD make (FreeBSD's make) reads this and
# hands every target to gmake: pkg install gmake
GMAKE = gmake

help:
	@$(GMAKE) help

.DEFAULT:
	@$(GMAKE) $@
