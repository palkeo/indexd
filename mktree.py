#!/usr/bin/python
import os

SYMBOLS = 'abcdefghijklmnopqrstuvwxyz0123456789'

for k in SYMBOLS:
    try:
        os.makedirs('/tmp/mount/%s' % (k,)) # replace /tmp/mount by the directory configured in the indexd daemon
    except:
        print('exists')
