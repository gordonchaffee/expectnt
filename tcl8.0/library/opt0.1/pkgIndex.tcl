# Tcl package index file, version 1.0
# This file is NOT generated by the "pkg_mkIndex" command
# because if someone just did "package require opt", let's just load
# the package now, so they can readily use it
# and even "namespace import tcl::*" ...
# (tclPkgSetup just makes things slow and do not work so well with namespaces)
package ifneeded opt 0.1 [list source [file join $dir optparse.tcl]]
