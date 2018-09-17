from trex_stl_lib.api import *
from scapy.contrib.igmpv3 import * # import from contrib folder of scapy 


class STLS1(object):

    def __init__ (self):
        pass;

    def create_stream (self):
        # 2 MPLS label the internal with  s=1 (last one)
        pkt =  Ether()/IGMPv3()/IGMPv3mr(records=[IGMPv3gr(maddr="127.0.0.1")])

        # burst of 17 packets
        return STLStream(packet = STLPktBuilder(pkt = pkt ,vm = []),
                         mode = STLTXSingleBurst( pps = 1, total_pkts = 17) )


    def get_streams (self, direction = 0, **kwargs):
        # create 1 stream 
        return [ self.create_stream() ]

def register():
    return STLS1()



