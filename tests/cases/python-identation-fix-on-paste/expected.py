class Test:
    def main(self):
    self.data_source = data_sourceself.in_amount = in_amount    self.trade_dir = trade_dir    self.start_ts_ms = start_ts_msself.analytics_context = analytics_context
if tp_percent == 0 and sl_percent == 0 and \
    tp_price == 0 and sl_price == 0:
    raise ValueError(
        'Round: one of tp_percent,sl_percent,tp_price,sl_price have to be filled')

if tp_percent != 0 and tp_price != 0:
    raise ValueError(
        f'Round: only one of tp_percent={tp_percent} or tp_price={tp_price} have to be filled')

if sl_percent != 0 and sl_price != 0:
raise ValueError(
        f'Round: only one of sl_percent={sl_percent} or sl_price={sl_price} have to be filled')