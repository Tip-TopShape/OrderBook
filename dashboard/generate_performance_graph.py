#!/usr/bin/env python3
"""
OrderBook Comprehensive Performance Analysis Generator

Generates a professional 4x3 grid (12 panels) of performance analysis plots

Usage:
    python3 generate_performance_graph.py --file metrics.xlsx
    python3 generate_performance_graph.py --file metrics.csv
"""

import sys
import re
import argparse
from dataclasses import dataclass
from typing import List

import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import numpy as np
import pandas as pd
from scipy import stats


@dataclass
class MetricsSnapshot:
    """Single snapshot of metrics at a point in time."""
    records: int = 0
    latency_avg_ns: int = 0
    latency_min_ns: int = 0
    latency_max_ns: int = 0
    memory_mb: float = 0.0
    cpu_percent: float = 0.0
    fill_rate: float = 0.0
    total_orders: int = 0
    buy_levels: int = 0
    sell_levels: int = 0
    bid_orders: int = 0
    ask_orders: int = 0
    ctx_switches: int = 0


def parse_metrics_file(filepath: str) -> List[MetricsSnapshot]:
    """Parse metrics from Excel (.xlsx) or CSV (.csv) file exported by dashboard."""
    # Read file based on extension
    if filepath.endswith('.xlsx'):
        df = pd.read_excel(filepath)
    elif filepath.endswith('.csv'):
        df = pd.read_csv(filepath)
    else:
        raise ValueError(f"Unsupported file format. Expected .xlsx or .csv, got: {filepath}")

    # Normalize column names: "Avg Match Ns" -> "avg_match_ns"
    df.columns = [col.lower().replace(' ', '_') for col in df.columns]

    # Column mapping from dashboard export to MetricsSnapshot fields
    column_map = {
        'records': 'records',
        'avg_match_ns': 'latency_avg_ns',
        'min_ns': 'latency_min_ns',
        'max_ns': 'latency_max_ns',
        'memory_bytes': 'memory_bytes',
        'cpu_pct': 'cpu_percent',
        'full_fills_pct': 'fill_rate',
        'total_orders': 'total_orders',
        'buy_levels': 'buy_levels',
        'sell_levels': 'sell_levels',
        'bid_orders': 'bid_orders',
        'ask_orders': 'ask_orders',
        'ctx_switches': 'ctx_switches',
    }

    snapshots = []
    for _, row in df.iterrows():
        snapshot = MetricsSnapshot()

        # Map each column
        for csv_col, attr in column_map.items():
            if csv_col in df.columns:
                val = row[csv_col]
                if pd.notna(val):
                    setattr(snapshot, attr, val)

        # Convert memory_bytes to MB if present
        if 'memory_bytes' in df.columns and pd.notna(row.get('memory_bytes')):
            snapshot.memory_mb = row['memory_bytes'] / (1024 * 1024)
        elif 'memory_str' in df.columns and pd.notna(row.get('memory_str')):
            snapshot.memory_mb = _parse_memory_to_mb(str(row['memory_str']))

        # Only add if we have records data
        if snapshot.records > 0:
            snapshots.append(snapshot)

    return snapshots


def _parse_memory_to_mb(s: str) -> float:
    """Convert memory string (e.g., '150.5 MB') to MB."""
    match = re.match(r'([\d.]+)\s*(B|KB|MB|GB)', s)
    if not match:
        return 0.0
    val, unit = float(match.group(1)), match.group(2)
    multipliers = {'B': 1/(1024**2), 'KB': 1/1024, 'MB': 1, 'GB': 1024}
    return val * multipliers.get(unit, 1)


def get_sample_data():
    """Generate sample data for testing/demonstration."""
    np.random.seed(42)

    # Simulated data from 100k to 25M records
    n_points = 50
    records = np.linspace(100, 25000, n_points).astype(int)  # in thousands

    # Stable latency around 2 microseconds with occasional spikes
    latency_avg_ns = np.array([2000 + np.random.normal(0, 100) for _ in records])
    latency_avg_ns[::7] += np.random.uniform(200, 500, len(latency_avg_ns[::7]))  # Add spikes

    latency_min_ns = latency_avg_ns * 0.6
    latency_max_ns = latency_avg_ns * 2.5

    # Linear memory growth
    memory_mb = np.array([r * 0.012 + 5 + np.random.normal(0, 2) for r in records])

    # CPU utilization - high initially, drops near end
    cpu_percent = np.array([97 + np.random.uniform(0, 2) for _ in records])
    cpu_percent[-8:] = np.linspace(95, 60, 8) + np.random.uniform(-5, 5, 8)

    # Fill rate with variation
    fill_rate = np.array([45 + np.random.uniform(-5, 10) for _ in records])

    # Order book depth (grows with records)
    total_orders = records * 0.3 + np.random.normal(0, 50, n_points)
    buy_levels = records * 0.02 + np.random.normal(0, 10, n_points)
    sell_levels = records * 0.02 + np.random.normal(0, 10, n_points)

    # Bid/Ask orders
    bid_orders = 20000 + records * 0.5 + np.random.normal(0, 500, n_points)
    ask_orders = 20000 + records * 0.5 + np.random.normal(0, 500, n_points)

    # Context switches
    ctx_switches = records * 0.002 + np.random.normal(0, 5, n_points)

    return (records, latency_avg_ns, latency_min_ns, latency_max_ns,
            memory_mb, cpu_percent, fill_rate, total_orders,
            buy_levels, sell_levels, bid_orders, ask_orders, ctx_switches)


def create_performance_graph(records: np.ndarray,
                            latency_avg_ns: np.ndarray,
                            latency_min_ns: np.ndarray,
                            latency_max_ns: np.ndarray,
                            memory_mb: np.ndarray,
                            cpu_percent: np.ndarray,
                            fill_rate: np.ndarray,
                            total_orders: np.ndarray,
                            buy_levels: np.ndarray,
                            sell_levels: np.ndarray,
                            bid_orders: np.ndarray,
                            ask_orders: np.ndarray,
                            ctx_switches: np.ndarray,
                            output_path: str = 'orderbook_analysis.png'):
    """
    Create comprehensive 12-panel performance analysis graph with dark theme.
    """
    if len(records) < 2:
        print("Error: Need at least 2 data points to generate graphs")
        sys.exit(1)

    # Convert to DataFrame
    df = pd.DataFrame({
        'records': records,
        'records_millions': np.array(records) / 1000,
        'latency_ns': latency_avg_ns,
        'latency_us': np.array(latency_avg_ns) / 1000,
        'latency_min_us': np.array(latency_min_ns) / 1000,
        'latency_max_us': np.array(latency_max_ns) / 1000,
        'memory_mb': memory_mb,
        'cpu_pct': cpu_percent,
        'fill_rate': fill_rate,
        'total_orders': total_orders,
        'buy_levels': buy_levels,
        'sell_levels': sell_levels,
        'bid_orders': bid_orders,
        'ask_orders': ask_orders,
        'ctx_switches': ctx_switches,
    })

    # Calculate statistics
    latency_mean = df['latency_us'].mean()
    latency_std = df['latency_us'].std()
    latency_median = df['latency_us'].median()
    fill_mean = df['fill_rate'].mean()

    # Linear regression for memory
    slope, intercept, r_value, p_value, std_err = stats.linregress(
        df['records_millions'], df['memory_mb']
    )

    # Setup dark theme
    plt.style.use('dark_background')

    # Create figure with 4x3 grid
    fig = plt.figure(figsize=(20, 14))
    gs = gridspec.GridSpec(4, 3, figure=fig, hspace=0.35, wspace=0.3)
    fig.patch.set_facecolor('#1e1e1e')

    # Color scheme
    colors = {
        'latency': '#4A90E2',
        'memory': '#50C878',
        'cpu': '#E74C3C',
        'fills': '#9B59B6',
        'warning': '#FFA500',
        'cyan': '#00FFFF',
        'yellow': '#FFFF00',
        'magenta': '#FF00FF',
        'green': '#00FF00',
        'red': '#FF4444',
    }

    # =========================================================================
    # Panel 1 (Row 0, Col 0): Latency Scatter
    # =========================================================================
    ax1 = fig.add_subplot(gs[0, 0])
    ax1.scatter(df['records_millions'], df['latency_us'],
               alpha=0.6, s=20, color=colors['latency'])
    ax1.axhline(latency_mean, color=colors['green'], linestyle='--', linewidth=2,
               label=f'Mean: {latency_mean:.2f}μs')
    ax1.axhline(latency_mean + 3*latency_std, color=colors['red'], linestyle=':',
               linewidth=1, alpha=0.7, label=f'3σ: {latency_mean + 3*latency_std:.2f}μs')
    ax1.set_xlabel('Records (Millions)', fontweight='bold')
    ax1.set_ylabel('Latency (μs)', fontweight='bold')
    ax1.set_title('Match Latency Over Time', fontweight='bold', fontsize=11)
    ax1.legend(loc='upper right', fontsize=8)
    ax1.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 2 (Row 0, Col 1): Memory with Linear Fit
    # =========================================================================
    ax2 = fig.add_subplot(gs[0, 1])
    ax2.plot(df['records_millions'], df['memory_mb'],
            color=colors['memory'], linewidth=2)
    ax2.fill_between(df['records_millions'], 0, df['memory_mb'],
                    alpha=0.3, color=colors['memory'])
    fit_line = slope * df['records_millions'] + intercept
    ax2.plot(df['records_millions'], fit_line, 'r--', linewidth=2,
            label=f'Linear fit (R²={r_value**2:.4f})')
    ax2.set_xlabel('Records (Millions)', fontweight='bold')
    ax2.set_ylabel('Memory (MB)', fontweight='bold')
    ax2.set_title(f'Memory Usage - {slope:.2f} MB/M records',
                 fontweight='bold', fontsize=11)
    ax2.legend(loc='upper left', fontsize=8)
    ax2.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 3 (Row 0, Col 2): CPU Utilization
    # =========================================================================
    ax3 = fig.add_subplot(gs[0, 2])
    ax3.plot(df['records_millions'], df['cpu_pct'],
            color=colors['cpu'], linewidth=2)
    ax3.axhline(95, color=colors['green'], linestyle='--', linewidth=1,
               alpha=0.7, label='Target: 95%')
    ax3.axhline(80, color=colors['warning'], linestyle=':', linewidth=1,
               alpha=0.7, label='Warning: 80%')
    # Mark CPU drop if any
    cpu_drop_idx = df[df['cpu_pct'] < 80].index
    if len(cpu_drop_idx) > 0:
        drop_point = cpu_drop_idx[0]
        ax3.axvline(df.loc[drop_point, 'records_millions'],
                   color=colors['warning'], linestyle='--', linewidth=2,
                   alpha=0.8, label='Processing end')
    ax3.set_xlabel('Records (Millions)', fontweight='bold')
    ax3.set_ylabel('CPU %', fontweight='bold')
    ax3.set_title('CPU Utilization', fontweight='bold', fontsize=11)
    ax3.set_ylim([0, 105])
    ax3.legend(loc='lower left', fontsize=8)
    ax3.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 4 (Row 1, Col 0): Fill Rate Scatter
    # =========================================================================
    ax4 = fig.add_subplot(gs[1, 0])
    ax4.scatter(df['records_millions'], df['fill_rate'],
               alpha=0.6, s=20, color=colors['fills'])
    ax4.axhline(fill_mean, color=colors['red'], linestyle='--', linewidth=2,
               label=f'Mean: {fill_mean:.1f}%')
    ax4.set_xlabel('Records (Millions)', fontweight='bold')
    ax4.set_ylabel('Full Fills %', fontweight='bold')
    ax4.set_title('Fill Rate Distribution', fontweight='bold', fontsize=11)
    ax4.legend(loc='upper right', fontsize=8)
    ax4.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 5 (Row 1, Col 1): Latency Histogram
    # =========================================================================
    ax5 = fig.add_subplot(gs[1, 1])
    ax5.hist(df['latency_us'], bins=30, color=colors['latency'],
            alpha=0.7, edgecolor='white', linewidth=0.5)
    ax5.axvline(latency_mean, color=colors['green'], linestyle='--', linewidth=2,
               label=f'Mean: {latency_mean:.2f}μs')
    ax5.axvline(latency_median, color=colors['yellow'], linestyle='--', linewidth=2,
               label=f'Median: {latency_median:.2f}μs')
    ax5.set_xlabel('Latency (μs)', fontweight='bold')
    ax5.set_ylabel('Frequency', fontweight='bold')
    ax5.set_title('Latency Distribution', fontweight='bold', fontsize=11)
    ax5.legend(loc='upper right', fontsize=8)
    ax5.grid(True, alpha=0.3, axis='y')

    # =========================================================================
    # Panel 6 (Row 1, Col 2): Order Book Depth
    # =========================================================================
    ax6 = fig.add_subplot(gs[1, 2])
    ax6.plot(df['records_millions'], df['total_orders'] / 1000,
            color=colors['cyan'], linewidth=2, label='Total Orders (K)')
    ax6.plot(df['records_millions'], df['buy_levels'] + df['sell_levels'],
            color=colors['magenta'], linewidth=2, label='Price Levels')
    ax6.set_xlabel('Records (Millions)', fontweight='bold')
    ax6.set_ylabel('Count', fontweight='bold')
    ax6.set_title('Order Book Depth Growth', fontweight='bold', fontsize=11)
    ax6.legend(loc='upper left', fontsize=8)
    ax6.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 7 (Row 2, Col 0): Latency Percentiles
    # =========================================================================
    ax7 = fig.add_subplot(gs[2, 0])
    window = max(3, min(15, len(df) // 4))
    df['latency_p50'] = df['latency_us'].rolling(window=window, min_periods=1).median()
    df['latency_p95'] = df['latency_us'].rolling(window=window, min_periods=1).quantile(0.95)
    df['latency_p99'] = df['latency_us'].rolling(window=window, min_periods=1).quantile(0.99)

    ax7.plot(df['records_millions'], df['latency_p50'], color=colors['cyan'],
            linewidth=2, label='p50 (median)')
    ax7.plot(df['records_millions'], df['latency_p95'], color=colors['yellow'],
            linewidth=2, label='p95')
    ax7.plot(df['records_millions'], df['latency_p99'], color=colors['red'],
            linewidth=2, label='p99')
    ax7.set_xlabel('Records (Millions)', fontweight='bold')
    ax7.set_ylabel('Latency (μs)', fontweight='bold')
    ax7.set_title(f'Latency Percentiles (rolling {window})',
                 fontweight='bold', fontsize=11)
    ax7.legend(loc='upper left', fontsize=8)
    ax7.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 8 (Row 2, Col 1): Memory Growth Rate
    # =========================================================================
    ax8 = fig.add_subplot(gs[2, 1])
    df['memory_delta'] = df['memory_mb'].diff()
    df['records_delta'] = df['records_millions'].diff()
    df['memory_growth_rate'] = df['memory_delta'] / df['records_delta']

    # Clean outliers
    growth_rate = df['memory_growth_rate'].replace([np.inf, -np.inf], np.nan)
    valid_mask = growth_rate.notna() & (growth_rate > 0) & (growth_rate < 200)

    if valid_mask.sum() > 0:
        ax8.scatter(df.loc[valid_mask, 'records_millions'],
                   growth_rate[valid_mask], alpha=0.5, s=15, color=colors['memory'])
        ax8.axhline(slope, color=colors['red'], linestyle='--', linewidth=2,
                   label=f'Expected: {slope:.2f} MB/M')
        upper_lim = min(100, growth_rate[valid_mask].quantile(0.98) * 1.2)
        ax8.set_ylim([0, upper_lim])
    ax8.set_xlabel('Records (Millions)', fontweight='bold')
    ax8.set_ylabel('MB per Million Records', fontweight='bold')
    ax8.set_title('Memory Growth Rate', fontweight='bold', fontsize=11)
    ax8.legend(loc='upper right', fontsize=8)
    ax8.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 9 (Row 2, Col 2): Context Switches
    # =========================================================================
    ax9 = fig.add_subplot(gs[2, 2])
    ax9.plot(df['records_millions'], df['ctx_switches'],
            color=colors['warning'], linewidth=2)
    ax9.set_xlabel('Records (Millions)', fontweight='bold')
    ax9.set_ylabel('Context Switches (K)', fontweight='bold')
    ax9.set_title('Voluntary Context Switches', fontweight='bold', fontsize=11)
    ax9.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 10 (Row 3, Col 0): Throughput Estimate
    # =========================================================================
    ax10 = fig.add_subplot(gs[3, 0])
    # Estimate throughput from records delta
    df['throughput'] = df['records_delta'] * 1000  # K records per interval
    valid_idx = df['throughput'].notna()
    if valid_idx.sum() > 0:
        ax10.plot(df.loc[valid_idx, 'records_millions'],
                 df.loc[valid_idx, 'throughput'] / 1000,
                 color=colors['cyan'], linewidth=2)
        avg_tp = df.loc[valid_idx, 'throughput'].mean() / 1000
        ax10.axhline(avg_tp, color=colors['green'], linestyle='--', linewidth=1,
                    label=f'Avg: {avg_tp:.1f}K msg/interval')
    ax10.set_xlabel('Records (Millions)', fontweight='bold')
    ax10.set_ylabel('Throughput (K msgs/interval)', fontweight='bold')
    ax10.set_title('Processing Throughput', fontweight='bold', fontsize=11)
    ax10.legend(loc='upper right', fontsize=8)
    ax10.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 11 (Row 3, Col 1): Bid/Ask Balance
    # =========================================================================
    ax11 = fig.add_subplot(gs[3, 1])
    ax11.plot(df['records_millions'], df['bid_orders'] / 1000,
             color=colors['green'], linewidth=2, label='Bids (K)', alpha=0.8)
    ax11.plot(df['records_millions'], df['ask_orders'] / 1000,
             color=colors['red'], linewidth=2, label='Asks (K)', alpha=0.8)
    ax11.set_xlabel('Records (Millions)', fontweight='bold')
    ax11.set_ylabel('Order Count (K)', fontweight='bold')
    ax11.set_title('Bid/Ask Order Balance', fontweight='bold', fontsize=11)
    ax11.legend(loc='upper left', fontsize=8)
    ax11.grid(True, alpha=0.3)

    # =========================================================================
    # Panel 12 (Row 3, Col 2): Max Latency Spikes
    # =========================================================================
    ax12 = fig.add_subplot(gs[3, 2])
    ax12.scatter(df['records_millions'], df['latency_max_us'],
                alpha=0.6, s=20, color=colors['red'])
    max_mean = df['latency_max_us'].mean()
    ax12.axhline(max_mean, color=colors['yellow'], linestyle='--', linewidth=1,
                alpha=0.7, label=f'Mean: {max_mean:.1f}μs')
    ax12.set_xlabel('Records (Millions)', fontweight='bold')
    ax12.set_ylabel('Max Latency (μs)', fontweight='bold')
    ax12.set_title('Maximum Latency Spikes', fontweight='bold', fontsize=11)
    ax12.legend(loc='upper right', fontsize=8)
    ax12.grid(True, alpha=0.3)

    # =========================================================================
    # Final touches
    # =========================================================================
    max_records = df['records'].max()
    if max_records >= 1000:
        max_str = f"{max_records/1000:.1f}M"
    else:
        max_str = f"{max_records}K"

    plt.suptitle(f'OrderBook_v2 Comprehensive Performance Analysis (100K - {max_str} records)',
                fontsize=14, fontweight='bold', y=0.995, color='white')

    # Save
    plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='#1e1e1e')
    print(f"Comprehensive analysis saved to: {output_path}")

    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Generate OrderBook_v2 comprehensive performance analysis'
    )
    parser.add_argument('--file', '-f', help='Metrics Excel (.xlsx) or CSV (.csv) file exported from dashboard')
    parser.add_argument('--output', '-o', default='orderbook_analysis.png',
                        help='Output image path (default: orderbook_analysis.png)')
    parser.add_argument('--sample', '-s', action='store_true',
                        help='Use sample data for demonstration')
    args = parser.parse_args()

    if args.file:
        print(f"Parsing metrics from: {args.file}")
        snapshots = parse_metrics_file(args.file)

        if not snapshots:
            print("Error: No metrics data found in file")
            sys.exit(1)

        print(f"Found {len(snapshots)} data points")

        # Extract arrays
        records = np.array([s.records // 1000 for s in snapshots])
        latency_avg_ns = np.array([s.latency_avg_ns for s in snapshots])
        latency_min_ns = np.array([s.latency_min_ns if s.latency_min_ns else s.latency_avg_ns * 0.6
                                   for s in snapshots])
        latency_max_ns = np.array([s.latency_max_ns if s.latency_max_ns else s.latency_avg_ns * 2.5
                                   for s in snapshots])
        memory_mb = np.array([s.memory_mb for s in snapshots])
        cpu_percent = np.array([s.cpu_percent for s in snapshots])
        fill_rate = np.array([s.fill_rate for s in snapshots])
        total_orders = np.array([s.total_orders if s.total_orders else r * 300
                                 for s, r in zip(snapshots, records)])
        buy_levels = np.array([s.buy_levels if s.buy_levels else r * 20
                               for s, r in zip(snapshots, records)])
        sell_levels = np.array([s.sell_levels if s.sell_levels else r * 20
                                for s, r in zip(snapshots, records)])
        bid_orders = np.array([s.bid_orders if s.bid_orders else 20000 + r * 500
                               for s, r in zip(snapshots, records)])
        ask_orders = np.array([s.ask_orders if s.ask_orders else 20000 + r * 500
                               for s, r in zip(snapshots, records)])
        ctx_switches = np.array([s.ctx_switches if s.ctx_switches else r * 2
                                 for s, r in zip(snapshots, records)])

    else:
        print("Using sample data (use --file to specify a metrics log)")
        (records, latency_avg_ns, latency_min_ns, latency_max_ns,
         memory_mb, cpu_percent, fill_rate, total_orders,
         buy_levels, sell_levels, bid_orders, ask_orders, ctx_switches) = get_sample_data()

    # Generate the graph
    create_performance_graph(
        records=records,
        latency_avg_ns=latency_avg_ns,
        latency_min_ns=latency_min_ns,
        latency_max_ns=latency_max_ns,
        memory_mb=memory_mb,
        cpu_percent=cpu_percent,
        fill_rate=fill_rate,
        total_orders=total_orders,
        buy_levels=buy_levels,
        sell_levels=sell_levels,
        bid_orders=bid_orders,
        ask_orders=ask_orders,
        ctx_switches=ctx_switches,
        output_path=args.output
    )


if __name__ == '__main__':
    main()
