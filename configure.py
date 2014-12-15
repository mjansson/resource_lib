#!/usr/bin/env python

"""Ninja build configurator for resource library"""

import sys
import os

sys.path.insert( 0, os.path.join( 'build', 'ninja' ) )

import generator

dependlibs = [ 'foundation' ]

generator = generator.Generator( project = 'resource', dependlibs = dependlibs )
target = generator.target
writer = generator.writer
toolchain = generator.toolchain

resource_lib = generator.lib( module = 'resource', sources = [
  'bundle.c', 'event.c', 'local.c', 'remote.c', 'resource.c', 'stream.c' ] )

#if not target.is_ios() and not target.is_android():
#  configs = [ config for config in toolchain.configs if config not in [ 'profile', 'deploy' ] ]
#  if not configs == []:
#    generator.bin( 'blast', [ 'main.c', 'client.c', 'server.c' ], 'blast', basepath = 'tools', implicit_deps = [ resource_lib ], libs = [ 'network' ], configs = configs )

includepaths = generator.test_includepaths()

test_cases = [
#  'stream'
]
if target.is_ios() or target.is_android():
  #Build one fat binary with all test cases
  test_resources = None
  test_cases += [ 'all' ]
  if target.is_ios():
    test_resources = [ 'all/ios/test-all.plist', 'all/ios/Images.xcassets', 'all/ios/test-all.xib' ]
    generator.app( module = '', sources = [ os.path.join( module, 'main.c' ) for module in test_cases ], binname = 'test-all', basepath = 'test', implicit_deps = [ resource_lib ], libs = [ 'test', 'resource', 'foundation' ], resources = test_resources, includepaths = includepaths )
  else:
    generator.bin( module = '', sources = [ os.path.join( module, 'main.c' ) for module in test_cases ], binname = 'test-all', basepath = 'test', implicit_deps = [ resource_lib ], libs = [ 'test', 'resource', 'foundation' ], resources = test_resources, includepaths = includepaths )
else:
  #Build one binary per test case
  generator.bin( module = 'all', sources = [ 'main.c' ], binname = 'test-all', basepath = 'test', implicit_deps = [ resource_lib ], libs = [ 'resource', 'foundation' ], includepaths = includepaths )
  for test in test_cases:
    generator.bin( module = test, sources = [ 'main.c' ], binname = 'test-' + test, basepath = 'test', implicit_deps = [ resource_lib ], libs = [ 'test', 'resource', 'foundation' ], includepaths = includepaths )
