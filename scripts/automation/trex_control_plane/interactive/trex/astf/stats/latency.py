from trex.common.trex_types import TRexError
from trex.utils.text_opts import format_num, red, green
from trex.utils import text_tables

class CAstfLatencyStats(object):
    def __init__(self, rpc):
        self.rpc = rpc
        self.reset()

    def reset(self):
        self.ref = {}
        self.epoch = None


    def _get_stats_internal(self):
        rc = self.rpc.transmit('get_latency_stats')
        if not rc:
            raise TRexError(rc.err())
        rc_data = rc.data()['data']

        data = {}
        for k, v in rc_data.items():
            if k.startswith('port-'):
                data[int(k[5:])] = v

        return rc_data['epoch'], data


    def get_stats(self):
        epoch, data = self._get_stats_internal()

        if epoch != self.epoch:
            self.reset()
            return data

        # epoch matches, get relative data
        for port_id, port_data in data.items():
            stats = port_data['stats']
            for k in stats.keys():
                stats[k] -= self.ref[port_id]['stats'][k]

        return data


    def clear_stats(self):
        epoch, data = self._get_stats_internal()
        self.epoch = epoch
        self.ref = data


    def to_table(self):
        data = self.get_stats()
        ports = sorted(data.keys())[:4]
        port_count = len(ports)

        longest_key = 0
        rows = {}
        for port_id in ports:
            for k, v in data[port_id]['stats'].items():
                if len(k) > longest_key:
                    longest_key = len(k)
                if k in rows:
                    rows[k].append(v)
                else:
                    rows[k] = [v]

        # init table
        stats_table = text_tables.TRexTextTable('Latency Stats')
        stats_table.set_cols_align(['l'] + ['r'] * port_count)
        stats_table.set_cols_width([longest_key] + [10] * port_count)
        stats_table.set_cols_dtype(['t'] * (1 + port_count))
        header = ['Port:'] + list(data.keys())
        stats_table.header(header)

        for k in sorted(rows.keys()):
            stats_table.add_row([k] + rows[k])

        return stats_table
