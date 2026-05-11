# Simulation Experiments

Both experiments replayed the first 200,000 rows of `lob.csv` with 5,000 units per quote and a quote refresh every 25 snapshots. The AS formula is evaluated in log-return (relative) space and converted to absolute price units; `kappa=50000` targets a ~2-tick half-spread for this asset. Fills use trade-driven queue depletion (`trades.csv`): at order placement the cumulative displayed quantity at all LOB levels at-or-better than the order price is recorded as `queueAhead`; each public trade decrements it and the order fills in full at its posted limit price when `queueAhead` reaches zero.

| Experiment | PnL | Inventory | Turnover | Fills | Half-spread (ticks) | Max Abs Inventory |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline AS 2008 | -104.3712500000 | 5000.0000000000 | 484741.6759999991 | 9285 | ~2.06 | 20000.0000000000 |
| Microprice extension | -104.9177500000 | 5000.0000000000 | 490113.7764999984 | 9387 | ~2.06 | 20000.0000000000 |

## Interpretation

Both runs are net negative over this 200,000-snapshot window. This is expected: the asset declined approximately 6.9% over the period (mid moved from ~0.01104 to ~0.01028). A symmetric market maker in a trending market accumulates directional inventory losses that outweigh spread income — this is adverse selection, not a model defect.

The queue-depletion model produces fewer fills (9285/9387 vs 11106/11144 in the crossing-based model) because each order must wait for the displayed queue ahead of it to be exhausted by real aggressive trades before filling. This is more realistic: on a busy top-of-book level a resting order is not filled by the first crossing trade but only after the volume that was there before it is consumed.

**Key observations:**

- Final inventory is 5000 (one `order_quantity` long) — a small residual from the last open order before the replay ended, not a failure of inventory management.
- Max absolute inventory reached 20000, staying well within the `inventory_limit=50000` guard.
- Turnover is lower (~485k vs ~581k) because fewer fills occur — queue position filtering is working.
- Runtime is ~0.68 s for 200k LOB snapshots + the corresponding trades, well within interactive speed.

## Execution model comparison

| Model | Fills | Turnover | PnL |
| --- | ---: | ---: | ---: |
| Crossing-based (no trades.csv) | 11106 | 581016 | -93.06 |
| Queue depletion (with trades.csv) | 9285 | 484741 | -104.37 |

The crossing model overstates fill rate (~20% more fills) because it ignores queue priority. The queue model is more conservative and more realistic.

## Next experiments

- Run on mean-reverting or range-bound slices of `lob.csv` to show positive PnL regime.
- Parameter sweep over `gamma`, `kappa`, and `quote_interval_events`.
- Walk-forward train/test split for out-of-sample validation.
- Enable `fee_bps` to model realistic maker rebate vs taker fee structure.
- Add partial fills: when `queueAhead` reaches zero but the crossed trade size is smaller than the order, fill only the available amount.
