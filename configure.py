#!/usr/bin/env python

"""Ninja build configurator for resource library"""

import sys
import os

sys.path.insert(0, os.path.join('build', 'ninja'))

import generator

dependlibs = ['resource', 'network', 'foundation']

generator = generator.Generator(project = 'resource', dependlibs = dependlibs, variables = [('bundleidentifier', 'com.rampantpixels.resource.$(binname)')])
target = generator.target
writer = generator.writer
toolchain = generator.toolchain

resource_lib = generator.lib(module = 'resource', sources = [
  'bundle.c', 'change.c', 'compile.c', 'compiled.c', 'event.c', 'import.c', 'local.c', 'platform.c', 'remote.c',
  'resource.c', 'source.c', 'sourced.c', 'stream.c', 'version.c'])

network_libs = []
if target.is_windows():
  network_libs += ['ws2_32', 'iphlpapi']

if not target.is_ios() and not target.is_android() and not target.is_tizen():
  configs = [config for config in toolchain.configs if config not in ['profile', 'deploy']]
  if not configs == []:
    generator.bin('resource', ['main.c'], 'resource', basepath = 'tools', implicit_deps = [resource_lib], dependlibs = dependlibs, libs = network_libs, configs = configs)
    generator.bin('sourced', ['main.c', 'server.c'], 'sourced', basepath = 'tools', implicit_deps = [resource_lib], dependlibs = dependlibs, libs = network_libs, configs = configs)
    generator.bin('compiled', ['main.c', 'server.c'], 'compiled', basepath = 'tools', implicit_deps = [resource_lib], dependlibs = dependlibs, libs = network_libs, configs = configs)

#No test cases if we're a submodule
if generator.is_subninja():
  sys.exit()

includepaths = generator.test_includepaths()

test_cases = [
  'source'
]
if toolchain.is_monolithic() or target.is_ios() or target.is_android() or target.is_tizen():
  #Build one fat binary with all test cases
  test_resources = []
  test_extrasources = []
  test_cases += ['all']
  if target.is_ios():
    test_resources = [os.path.join('all', 'ios', item) for item in ['test-all.plist', 'Images.xcassets', 'test-all.xib']]
    test_extrasources = [os.path.join('all', 'ios', 'viewcontroller.m')]
  elif target.is_android():
    test_resources = [os.path.join('all', 'android', item) for item in [
      'AndroidManifest.xml', os.path.join('layout', 'main.xml'), os.path.join('values', 'strings.xml'),
      os.path.join('drawable-ldpi', 'icon.png'), os.path.join('drawable-mdpi', 'icon.png'), os.path.join('drawable-hdpi', 'icon.png'),
      os.path.join('drawable-xhdpi', 'icon.png'), os.path.join('drawable-xxhdpi', 'icon.png'), os.path.join('drawable-xxxhdpi', 'icon.png')
    ]]
    test_extrasources = [os.path.join('all', 'android', 'java', 'com', 'rampantpixels', 'resource', 'test', item) for item in [
      'TestActivity.java'
    ]]
  elif target.is_tizen():
    test_resources = [os.path.join('all', 'tizen', item) for item in [
      'tizen-manifest.xml', os.path.join('res', 'tizenapp.png')
    ]]
  if target.is_macos() or target.is_ios() or target.is_android() or target.is_tizen():
    generator.app(module = '', sources = [os.path.join( module, 'main.c') for module in test_cases] + test_extrasources, binname = 'test-all', basepath = 'test', implicit_deps = [resource_lib], libs = ['test'] + dependlibs + network_libs, resources = test_resources, includepaths = includepaths)
  else:
    generator.bin(module = '', sources = [os.path.join( module, 'main.c') for module in test_cases] + test_extrasources, binname = 'test-all', basepath = 'test', implicit_deps = [resource_lib], libs = ['test'] + dependlibs + network_libs, resources = test_resources, includepaths = includepaths)
else:
  #Build one binary per test case
  generator.bin(module = 'all', sources = ['main.c'], binname = 'test-all', basepath = 'test', implicit_deps = [resource_lib], libs = ['resource', 'network', 'foundation'] + network_libs, includepaths = includepaths)
  for test in test_cases:
    #if target.is_macos():
    #  test_resources = [ os.path.join( 'osx', item ) for item in [ 'test-' + test + '.plist', 'Images.xcassets', 'test-' + test + '.xib' ] ]
    #  generator.app( module = test, sources = [ 'main.c' ], binname = 'test-' + test, basepath = 'test', implicit_deps = [resource_lib], libs = ['test'] + dependlibs + network_libs, resources = test_resources, includepaths = includepaths )
    #else:
    generator.bin(module = test, sources = ['main.c'], binname = 'test-' + test, basepath = 'test', implicit_deps = [resource_lib], libs = ['test'] + dependlibs + network_libs, includepaths = includepaths)
