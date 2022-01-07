test0 and test1 work fine, and test2 gives some errors after the file is filled up,
but other than that, test2 works fine.
The code was tested on mimi, I didn't test it on local because the there were 
some errors when running Makefile on local.
I didn't test fuse cause it doesn't compile for some reason, but 
sfs_getnextfilename() is definitely working fine since it was tested in
test2.