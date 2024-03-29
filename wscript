# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('bsdvr', ['internet', 'wifi'])
    module.source = [
        'model/bsdvr.cc',
        'model/bsdvr-rtable.cc',
        'model/bsdvr-rqueue.cc',
        'model/bsdvr-packet.cc',
        'model/bsdvr-neighbor.cc',
        'helper/bsdvr-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('bsdvr')
    module_test.source = [
        'test/bsdvr-test-suite.cc',
        ]
    # Tests encapsulating example programs should be listed here
    if (bld.env['ENABLE_EXAMPLES']):
        module_test.source.extend([
        #    'test/bsdvr-examples-test-suite.cc',
             ])

    headers = bld(features='ns3header')
    headers.module = 'bsdvr'
    headers.source = [
        'model/bsdvr.h',
        'model/bsdvr-rtable.h',
        'model/bsdvr-rqueue.h',
        'model/bsdvr-packet.h',
        'model/bsdvr-neighbor.h',
        'model/bsdvr-constants.h',
        'helper/bsdvr-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

