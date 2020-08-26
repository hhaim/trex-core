#!/usr/bin/python
import sys, os
from multiprocessing import Process
import tempfile
from functools import wraps


def check_offsets(build, scapy_str):
    import sys

    # clean this env
    for key in sys.modules.copy().keys():
        if key.startswith('scapy.'):
            del sys.modules[key]
    globals().clear()

    import outer_packages
    from scapy.all import Ether, IP, UDP

    pkt = eval(scapy_str)

    if build:
        pkt.build()

    assert pkt
    assert pkt.payload

    lay = pkt
    while lay:
        print('  ### %s (offset %s)' % (lay.name, lay._offset))
        lay.dump_fields_offsets()
        if lay == pkt:
            assert lay._offset == 0, 'Offset of first layer should be zero.'
        else:
            if build:
                assert lay._offset != 0, 'Offset of second and further layers should not be zero if packets is built.'
            else:
                assert lay._offset == 0, 'Offset of second and further layers should be zero if packets is not built.'
        for index, field in enumerate(lay.fields_desc):
            if index == 0:
                assert field._offset == 0, 'Offset of first field should be zero.'
            else:
                if build:
                    if field.get_size_bytes() == 0:
                        continue
                    assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
                else:
                    assert field._offset == 0, 'Offset of second and further fields should be zero if packets is not built.'

        lay = lay.payload


def check_offsets_pcap(pcap):
    import sys

    # clean this env
    for key in sys.modules.copy().keys():
        if key.startswith('scapy.'):
            del sys.modules[key]
    globals().clear()

    import outer_packages
    from scapy.all import Ether, IP, UDP
    from scapy.layers.dns import DNS
    from scapy.utils import rdpcap

    pkt = rdpcap(pcap)[0]
    assert pkt
    assert pkt.payload

    not_built_offsets = {}
    cond_var_length = False
    lay = pkt
    while lay:
        print('  ### %s (offset %s)' % (lay.name, lay._offset))
        not_built_offsets[lay.name] = {}
        not_built_offsets[lay.name]['_offset'] = lay._offset
        lay.dump_fields_offsets()
        if lay == pkt:
            assert lay._offset == 0, 'Offset of first layer should be zero.'
        else:
            assert lay._offset != 0, 'Offset of second and further layers should not be zero.'
        for index, field in enumerate(lay.fields_desc):
            if index == 0:
                assert field._offset == 0, 'Offset of first field should be zero.'
                if field.name == "length":
                    cond_var_length = True
            else:
                if field.get_size_bytes() == 0:
                    continue
                if cond_var_length:
                    cond_var_length = False
                    continue
                assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
            not_built_offsets[lay.name][field.name] = field._offset

        lay = lay.payload

    print('')
    pkt.build()
    cond_var_length = False
    lay = pkt
    while lay:
        print('  ### %s (offset %s)' % (lay.name, lay._offset))
        assert not_built_offsets[lay.name]['_offset'] == lay._offset, 'built and not built pcap offsets differ'
        lay.dump_fields_offsets()
        if lay == pkt:
            assert lay._offset == 0, 'Offset of first layer should be zero.'
        else:
            assert lay._offset != 0, 'Offset of second and further layers should not be zero.'
        for index, field in enumerate(lay.fields_desc):
            if index == 0:
                assert field._offset == 0, 'Offset of first field should be zero.'
                if field.name == "length":
                    cond_var_length = True
            else:
                if field.get_size_bytes() == 0:
                    continue
                if cond_var_length:
                    cond_var_length = False
                    continue
                assert field._offset != 0, 'Offset of second and further fields should not be zero if packets is built.'
            assert not_built_offsets[lay.name][field.name] == field._offset, 'built and not built pcap offsets differ'

        lay = lay.payload


def isolate_env(f):
    @wraps(f)
    def wrapped(*a):
        print('')
        p = Process(target = f, args = a)
        p.start()
        p.join()
        if p.exitcode:
            raise Exception('Return status not zero, check the output')
    return wrapped


class CScapy_Test():
    def setUp(self):
        self.dir = os.path.abspath(os.path.dirname(__file__)) + '/'

    # verify that built packet gives non-zero offsets
    @isolate_env
    def test_scapy_offsets_udp_build(self):
        check_offsets(scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = True)

    # verify that non-built packet gives zero offsets
    @isolate_env
    def test_scapy_offsets_udp_nobuild(self):
        check_offsets(scapy_str = "Ether()/IP()/UDP()/('x'*9)", build = False)

    # verify that pcap either built or not gives same non-zero offsets
    @isolate_env
    def test_scapy_offsets_pcap(self):
        check_offsets_pcap(pcap = self.dir + 'golden/bp_sim_dns_vlans.pcap')

    @isolate_env
    def test_scapy_ipfix(self):
        from scapy.contrib.ipfix import IPFIX
        p = IPFIX()
        p.show2()

    @isolate_env
    def test_scapy_icmp_bes(self):
        from scapy.all import Ether, IP, ICMP

        p = Ether()/IP(src='1.1.1.1', dst='2.2.2.2')/ICMP()/('x'*1)
        p1 = Ether()/IP(src='1.1.1.1', dst='2.2.2.2')/ICMP()
        #p.show()
        bytesP = bytes(p)
        bytesP1 = bytes(p1)

        #print(bytesP)
        #print("*"*50)
        recPkt = Ether(bytesP)
        comd = recPkt.command()
        print(comd)

        recPkt1 = Ether(bytesP1)
        comd1 = recPkt1.command()
        print(comd1)

        #print('+'*100)
        binaryPkt = b'$\x8a\x07\x14\xfcY$\x8a\x07\x14\xfcX\x08\x00E\x00\x05\xce\x00\x01\x00\x00@\x01o)\x01\x01\x01\x01\x02\x02\x02\x02\x08\x00\x1c\xd8\xcc\x18\x00\x00xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
        #binaryPkt = b'$\x8a\x07\x14\xfcY$\x8a\x07\x14\xfcX\x08\x00E\x00\x05\xce\x00\x01\x00\x00@\x01o)\x01\x01\x01\x01\x02\x02\x02\x02\x08\x00\x1c\xd8\xcc\x18\x00\x00aaaaa'
        print(Ether(binaryPkt).command())
        #print(Ether(bytesP).command())
        #print(Ether(binaryPkt).command())

 