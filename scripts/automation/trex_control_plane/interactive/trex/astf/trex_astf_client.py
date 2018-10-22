
from ..utils.common import get_current_user
from ..utils import parsing_opts, text_tables

from ..common.trex_logger import Logger
from ..common.trex_exceptions import TRexError
from ..common.trex_client import TRexClient
from ..common.trex_api_annotators import client_api, console_api
from ..common.trex_types import *

from .trex_astf_port import ASTFPort
from .trex_astf_profile import ASTFProfile
from .stats.traffic import CAstfTrafficStats
from .stats.latency import CAstfLatencyStats
from ..utils.common import  is_valid_ipv4, is_valid_ipv6
import hashlib
import sys

class ASTFClient(TRexClient):
    def __init__(self,
                 username = get_current_user(),
                 server = "localhost",
                 sync_port = 4501,
                 async_port = 4500,
                 verbose_level = "error",
                 logger = None):

        """ 
        TRex advance stateful client

        :parameters:
             username : string 
                the user name, for example imarom

              server  : string 
                the server name or ip 

              sync_port : int 
                the RPC port 

              async_port : int 
                the ASYNC port (subscriber port)

              verbose_level: str
                one of "none", "critical", "error", "info", "debug"

              logger: instance of AbstractLogger
                if None, will use ScreenLogger
        """

        api_ver = {'name': 'ASTF', 'major': 1, 'minor': 0}

        TRexClient.__init__(self,
                            api_ver,
                            username,
                            server,
                            sync_port,
                            async_port,
                            verbose_level,
                            logger)
        self.handler = ''
        self.traffic_stats = CAstfTrafficStats(self.conn.rpc)
        self.latency_stats = CAstfLatencyStats(self.conn.rpc)



    def get_mode (self):
        return "ASTF"

############################    called       #############################
############################    by base      #############################
############################    TRex Client  #############################

    def _on_connect_create_ports(self, system_info):
        """
            called when connecting to the server
            triggered by the common client object
        """

        # create ports
        self.ports.clear()
        for port_info in system_info['ports']:
            port_id = port_info['index']
            self.ports[port_id] = ASTFPort(self.ctx, port_id, self.conn.rpc, port_info)
        return RC_OK()

    def _on_connect_clear_stats(self):
        self.traffic_stats.reset()
        self.latency_stats.reset()
        with self.ctx.logger.suppress(verbose = "warning"):
            self.clear_stats(ports = self.get_all_ports(), clear_xstats = False, clear_traffic = False, clear_latency = False)
        return RC_OK()

############################     helper     #############################
############################     funcs      #############################
############################                #############################

    def __get_acquired_ports_param(self):
        port_ids = self.get_acquired_ports()
        if not port_ids:
            raise TRexError('No acquired ports')
        return [{'port_id': id, 'handler': self.ports[id].handler} for id in port_ids]

############################       ASTF     #############################
############################       API      #############################
############################                #############################

    @client_api('command', True)
    def reset(self, restart = False):
        """
            Force acquire ports, stop the traffic, remove loaded traffic and clear stats

            :parameters:
                restart: bool
                   Restart the NICs (link down / up)

            :raises:
                + :exc:`TRexError`

        """
        ports = self.get_all_ports()

        if restart:
            self.ctx.logger.pre_cmd("Hard resetting ports {0}:".format(ports))
        else:
            self.ctx.logger.pre_cmd("Resetting ports {0}:".format(ports))

        try:
            with self.ctx.logger.suppress():
            # force take the port and ignore any streams on it
                self.acquire(force = True)
                self.stop()
                self.clear_stats(ports)
                self.set_port_attr(ports,
                                   promiscuous = False,
                                   link_up = True if restart else None)
                self.remove_rx_queue(ports)
                self._for_each_port('stop_capture_port', ports)

            self.ctx.logger.post_cmd(RC_OK())

        except TRexError as e:
            self.ctx.logger.post_cmd(False)
            raise e



    @client_api('command', True)
    def acquire(self, force = False):
        """
            Acquires ports for executing commands

            :parameters:
                force : bool
                    Force acquire the ports.

            :raises:
                + :exc:`TRexError`

        """
        ports = self.get_all_ports()

        if force:
            self.ctx.logger.pre_cmd('Force acquiring ports %s:' % ports)
        else:
            self.ctx.logger.pre_cmd('Acquiring ports %s:' % ports)

        params = {'force': force,
                  'user':  self.ctx.username}
        rc = self._transmit('acquire', params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError('Could not acquire context: %s' % rc.err())

        self.handler = rc.data()['handler']
        for port_id, port_rc in rc.data()['ports'].items():
            self.ports[int(port_id)]._set_handler(port_rc)

        self._post_acquire_common(ports)

    @client_api('command', True)
    def release(self, force = False):
        """
            Release ports

            :parameters:
                none

            :raises:
                + :exc:`TRexError`

        """

        ports = self.get_acquired_ports()
        self.ctx.logger.pre_cmd("Releasing ports {0}:".format(ports))
        params = {'handler': self.handler}
        rc = self._transmit('release', params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError('Could not release context: %s' % rc.err())

        self.handler = ''
        for port_id in ports:
            self.ports[port_id]._clear_handler()


    @client_api('command', True)
    def load_profile(self, filename, tunables = {}):
        """ |  load a profile Supported types are:
            |  .py
            |  .json

            :parameters:
                filename : string
                    filename (with path) of the profile

                tunables : dict
                    forward those key-value pairs to the profile

            :raises:
                + :exc:`TRexError`

        """

        # generate traffic
        profile = ASTFProfile.load(filename, **tunables);
        profile_json = profile.to_json_str(pretty = False, sort_keys = True)

        # send by fragments to server
        self.ctx.logger.pre_cmd('Loading traffic at acquired ports.')
        index_start = 0
        fragment_length = 1000
        while len(profile_json) > index_start:
            index_end = index_start + fragment_length
            params = {
                'handler': self.handler,
                'fragment': profile_json[index_start:index_end],
                }
            if index_start == 0:
                params['frag_first'] = True
            if index_end >= len(profile_json):
                params['frag_last'] = True
            if params.get('frag_first') and not params.get('frag_last'):
                params['total_size'] = len(profile_json)
                params['md5'] = hashlib.md5(profile_json.encode()).hexdigest()

            rc = self._transmit('profile_fragment', params = params)
            if not rc:
                self.ctx.logger.post_cmd(False)
                raise TRexError('Could not load profile, error: %s' % rc.err())
            if params.get('frag_first') and not params.get('frag_last'):
                if rc.data() and rc.data().get('matches_loaded'):
                    break
            index_start = index_end
            fragment_length = 500000

        self.ctx.logger.post_cmd(True)


    @client_api('command', True)
    def start(self, mult = 1, duration = -1,nc=False,latency_pps = 0):

        params = {
            'handler': self.handler,
            'mult': mult,
            'nc':nc,
            'duration': duration,
            'latency_pps' :latency_pps
            }

        self.ctx.logger.pre_cmd('Starting traffic.')
        rc = self._transmit("start", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())

    @client_api('command', True)
    def stop(self):
        params = {
            'handler': self.handler,
            }
        self.ctx.logger.pre_cmd('Stopping traffic.')
        rc = self._transmit("stop", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    # get stats
    @client_api('getter', True)
    def get_stats(self, ports = None, sync_now = True):

        stats = self._get_stats_common(ports, sync_now)
        stats['traffic'] = self.get_traffic_stats()
        stats['latency'] = self.get_latency_stats()

        return stats


    # clear stats
    @client_api('getter', True)
    def clear_stats(self,
                    ports = None,
                    clear_global = True,
                    clear_xstats = True,
                    clear_traffic = True,
                    clear_latency = True):

        if clear_traffic:
            self.clear_traffic_stats()

        if clear_latency:
            self.clear_latency_stats()

        return self._clear_stats_common(ports, clear_global, clear_xstats)


    @client_api('getter', True)
    def get_traffic_stats(self):
        return self.traffic_stats.get_stats()


    @client_api('getter', True)
    def clear_traffic_stats(self):
        return self.traffic_stats.clear_stats()


    @client_api('getter', True)
    def get_latency_stats(self):
        return self.latency_stats.get_stats()


    @client_api('getter', True)
    def clear_latency_stats(self):
        return self.latency_stats.clear_stats()


    @client_api('command', True)
    def start_latency(self, mult = 1, src_ipv4="16.0.0.1", dst_ipv4="48.0.0.1", ports_mask=0xffffffff, dual_ipv4 = "1.0.0.0"):
        """
           Start ICMP latency traffic.

            :parameters:
                mult: float 
                    number of packets per second

                src_ipv4: IP string
                    IPv4 source address for the port

                dst_ipv4: IP string
                    IPv4 destination address

                dual_ipv4: uint32_t
                    bitmask of ports

                dual_ipv4: IP string
                    IPv4 address to be added for each pair of ports (starting from second pair)

            .. note::
                Vlan will be taken from interface configuration

            :raises:
                + :exc:`TRexError`
        """
        if not is_valid_ipv4(src_ipv4):
            raise TRexError("src_ipv4 is not a valid IPv4 address: '{0}'".format(src_ipv4))

        if not is_valid_ipv4(dst_ipv4):
            raise TRexError("dst_ipv4 is not a valid IPv4 address: '{0}'".format(dst_ipv4))

        if not is_valid_ipv4(dual_ipv4):
            raise TRexError("dual_ipv4 is not a valid IPv4 address: '{0}'".format(dual_ipv4))

        params = {
            'handler': self.handler,
            'mult': mult,
            'src_addr': src_ipv4,
            'dst_addr': dst_ipv4,
            'dual_port_addr': dual_ipv4,
            'mask': ports_mask,
            }

        self.ctx.logger.pre_cmd('Starting latency traffic.')
        rc = self._transmit("start_latency", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    @client_api('command', True)
    def stop_latency(self):
        """
           Stop latency traffic.
        """

        params = {
            'handler': self.handler
            }

        self.ctx.logger.pre_cmd('Stopping latency traffic.')
        rc = self._transmit("stop_latency", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())


    @client_api('command', True)
    def update_latency(self, mult = 1):
        """
           Update rate of latency traffic.

            :parameters:
                mult: float 
                    number of packets per second

            :raises:
                + :exc:`TRexError`
        """

        params = {
            'handler': self.handler,
            'mult': mult,
            }

        self.ctx.logger.pre_cmd('Updating latenct rate.')
        rc = self._transmit("update_latency", params = params)
        self.ctx.logger.post_cmd(rc)
        if not rc:
            raise TRexError(rc.err())



############################   console   #############################
############################   commands  #############################
############################             #############################

    @console_api('acquire', 'common', True)
    def acquire_line (self, line):
        '''Acquire ports\n'''

        # define a parser
        parser = parsing_opts.gen_parser(
            self,
            'acquire',
            self.acquire_line.__doc__,
            parsing_opts.FORCE)

        opts = parser.parse_args(line.split())
        self.acquire(force = opts.force)
        return True


    @console_api('reset', 'common', True)
    def reset_line (self, line):
        '''Reset ports'''

        parser = parsing_opts.gen_parser(
            self,
            'reset',
            self.reset_line.__doc__,
            parsing_opts.PORT_RESTART)

        opts = parser.parse_args(line.split())
        self.reset(restart = opts.restart)
        return True


    @console_api('start', 'ASTF', True)
    def start_line(self, line):
        '''start traffic command'''

        parser = parsing_opts.gen_parser(
            self,
            "start",
            self.start_line.__doc__,
            parsing_opts.FILE_PATH,
            parsing_opts.MULTIPLIER_INT,
            parsing_opts.DURATION,
            parsing_opts.TUNABLES,
            parsing_opts.ASTF_NC,
            parsing_opts.ASTF_LATENCY
            )
        opts = parser.parse_args(line.split())
        tunables = opts.tunables or {}
        self.load_profile(opts.file[0], tunables)
        self.start(opts.mult, duration = opts.duration, nc = opts.nc, latency_pps = opts.latency_pps)
        return True

    @console_api('stop', 'ASTF', True)
    def stop_line(self, line):
        '''stop traffic command'''

        parser = parsing_opts.gen_parser(
            self,
            "stop",
            self.stop_line.__doc__,
            )
        self.stop()


    def calc_latency_port_mask(self, ports):
        mask =0
        for p in ports:
            mask += (1<<p)
        return mask


    @console_api('latency', 'ASTF', True)
    def latency_line(self, line):
        '''Latency-related commands'''

        parser = parsing_opts.gen_parser(
            self,
            'latency',
            self.latency_line.__doc__)

        def latency_add_parsers(subparsers, cmd, help = '', **k):
            return subparsers.add_parser(cmd, description = help, help = help, **k)

        subparsers = parser.add_subparsers(title = 'commands', dest = 'command', metavar = '')
        start_parser = latency_add_parsers(subparsers, 'start', help = 'Start latency traffic')
        latency_add_parsers(subparsers, 'stop', help = 'Stop latency traffic')
        update_parser = latency_add_parsers(subparsers, 'update', help = 'Update rate of running latency')
        latency_add_parsers(subparsers, 'show', help = 'alias for stats -l')
        latency_add_parsers(subparsers, 'hist', help = 'alias for stats -h')

        start_parser.add_arg_list(
            parsing_opts.MULTIPLIER_INT,
            parsing_opts.SRC_IPV4,
            parsing_opts.DST_IPV4,
            parsing_opts.PORT_LIST,
            parsing_opts.DUAL_IPV4
            )

        update_parser.add_arg_list(
            parsing_opts.MULTIPLIER_INT,
            )

        opts = parser.parse_args(line.split())

        if opts.command == 'start':
            ports_mask = self.calc_latency_port_mask(opts.ports)
            self.start_latency(mult = opts.mult,
                               src_ipv4 = opts.src_ipv4,
                               dst_ipv4 = opts.dst_ipv4,
                               dual_ipv4 = opts.dual_ip_mask,
                               ports_mask = ports_mask)

        elif opts.command == 'stop':
            self.stop_latency()

        elif opts.command == 'update':
            self.update_latency(mult = opts.mult)

        elif opts.command == 'show':
            self._show_latency_stats()

        elif opts.command == 'hist':
            self._show_latency_histogram()

        else:
            raise TRexError('Unhandled command %s' % opts.command)

        return True


    @console_api('stats', 'common', True)
    def show_stats_line (self, line):
        '''Show various statistics\n'''
        # define a parser
        parser = parsing_opts.gen_parser(self,
                                         "stats",
                                         self.show_stats_line.__doc__,
                                         parsing_opts.PORT_LIST,
                                         parsing_opts.ASTF_STATS_GROUP)

        opts = parser.parse_args(line.split())
        if not opts:
            return

        # without parameters show only global and ports
        if not opts.stats:
            self._show_global_stats()
            self._show_port_stats(opts.ports)
            return

        # decode which stats to show
        if opts.stats == 'global':
            self._show_global_stats()

        elif opts.stats == 'ports':
            self._show_port_stats(opts.ports)

        elif opts.stats == 'xstats':
            self._show_port_xstats(opts.ports, False)

        elif opts.stats == 'xstats_inc_zero':
            self._show_port_xstats(opts.ports, True)

        elif opts.stats == 'status':
            self._show_port_status(opts.ports)

        elif opts.stats == 'cpu':
            self._show_cpu_util()

        elif opts.stats == 'mbuf':
            self._show_mbuf_util()

        elif opts.stats == 'astf':
            self._show_traffic_stats(False)

        elif opts.stats == 'astf_inc_zero':
            self._show_traffic_stats(True)

        elif opts.stats == 'latency':
            self._show_latency_stats()

        elif opts.stats == 'latency_histogram':
            self._show_latency_histogram()

        else:
            raise TRexError('Unhandled stat: %s' % opts.stats)


    def _show_traffic_stats(self, include_zero_lines, buffer = sys.stdout):
        table = self.traffic_stats.to_table(include_zero_lines)
        text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


    def _show_latency_stats(self, buffer = sys.stdout):
        table = self.latency_stats.to_table()
        text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


    def _show_latency_histogram(self, buffer = sys.stdout):
        raise TRexError('Not implemented')
        #table = self.latency_stats.to_table()
        #text_tables.print_table_with_header(table, untouched_header = table.title, buffer = buffer)


