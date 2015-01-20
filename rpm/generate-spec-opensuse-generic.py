#!/usr/bin/python

import SpecGenerator

SpecGenerator.process(
    out_file = "statefs-providers-opensuse-generic.spec"
    , filters = ['.*mce'])

