

import argparse
import csv
import glob
import logging
import os
from collections import Counter
from pathlib import Path
from typing import List, Tuple, Dict

import matplotlib.pyplot as plt
import numpy as np
from sklearn.linear_model import LinearRegression


def load_token_frequencies(tokens_dir: str) -> Counter:
    freq_counter = Counter()
    tokens_path = Path(tokens_dir)
    
    if not tokens_path.exists():
        raise FileNotFoundError(f"Директория не найдена: {tokens_dir}")
    
    token_files = list(tokens_path.glob("*.tokens"))
    
    if not token_files:
        raise ValueError(f"В директории {tokens_dir} не найдены .tokens файлы")
    
    logging.info(f"Найдено {len(token_files)} файлов для обработки...")
    
    for file_path in token_files:
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if not line:
                        continue
                    
                    parts = line.split()
                    if len(parts) < 2:
                        logging.warning(f"{file_path}:{line_num} - пропущена пустая строка: {line}")
                        continue
                    
                    token = parts[0]
                    
                    positions_count = len(parts) - 1
                    if positions_count > 0:
                        freq_counter[token] += positions_count
                    
        except Exception as e:
            logging.error(f"Ошибка при чтении {file_path}: {e}")
            continue
    
    logging.info(f"Всего уникальных токенов: {len(freq_counter)}")
    return freq_counter


def rank_and_sort_frequencies(freq_counter: Counter) -> List[Tuple[int, str, int]]:

    
    sorted_tokens = sorted(
        freq_counter.items(),
        key=lambda x: (-x[1], x[0])
    )
    
    ranked_data = []
    for rank, (token, freq) in enumerate(sorted_tokens, start=1):
        ranked_data.append((rank, token, freq))
    
    return ranked_data


def save_to_csv(data: List[Tuple[int, str, int]], csv_path: str):
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['rank', 'token', 'frequency'])
        writer.writerows(data)
    logging.info(f"CSV сохранён: {csv_path}")


def perform_zipf_analysis(data: List[Tuple[int, str, int]]) -> Tuple[np.ndarray, np.ndarray, Dict]:

    ranks = np.array([item[0] for item in data], dtype=float)
    frequencies = np.array([item[2] for item in data], dtype=float)
    
    
    log_ranks = np.log(ranks)
    log_frequencies = np.log(frequencies)
    
    
    
    X = log_ranks.reshape(-1, 1)
    y = log_frequencies
    
    reg = LinearRegression()
    reg.fit(X, y)
    
    slope = reg.coef_[0]
    intercept = reg.intercept_
    r_score = reg.score(X, y)
    
    
    y_pred = reg.predict(X)
    
    results = {
        'slope': slope,
        'intercept': intercept,
        'r_squared': r_score,
        'y_pred': y_pred
    }
    
    logging.info(f"Параметры регрессии:")
    logging.info(f"  Slope (параметр s): {slope:.4f}")
    logging.info(f"  Intercept: {intercept:.4f}")
    logging.info(f"  R²: {r_score:.4f}")
    
    return log_ranks, log_frequencies, results


def plot_zipf_law(
    log_ranks: np.ndarray,
    log_frequencies: np.ndarray,
    regression_results: Dict,
    output_path: str,
    top_n: int = 1000
):

    plt.figure(figsize=(12, 8))
    
    
    n_points = min(top_n, len(log_ranks))
    x_plot = log_ranks[:n_points]
    y_plot = log_frequencies[:n_points]
    y_pred_plot = regression_results['y_pred'][:n_points]
    
    
    plt.scatter(
        x_plot, y_plot,
        alpha=0.6,
        s=20,
        label='Токены',
        color='steelblue'
    )
    
    
    plt.plot(
        x_plot, y_pred_plot,
        color='darkred',
        linewidth=2,
        label=f"Линейная регрессия (s = {regression_results['slope']:.3f})"
    )
    
    plt.xlabel('log(Rank)', fontsize=12)
    plt.ylabel('log(Frequency)', fontsize=12)
    plt.title(f'Закон Ципфа: Log-log зависимость частоты от ранга\n(R² = {regression_results["r_squared"]:.4f})', 
              fontsize=14)
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    logging.info(f"График сохранён: {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description="Анализ закона Ципфа на корпусе токенизированных документов",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="Пример использования:\n"
               "  python zipf_analysis.py --tokens-dir tokens --output-csv zipf_data.csv --output-plot zipf_plot.png"
    )
    
    parser.add_argument(
        '--tokens-dir',
        default='tokens',
        help='Директория с .tokens файлами (по умолчанию: tokens)'
    )
    
    parser.add_argument(
        '--output-csv',
        default='zipf_data.csv',
        help='Путь к выходному CSV файлу (по умолчанию: zipf_data.csv)'
    )
    
    parser.add_argument(
        '--output-plot',
        default='zipf_plot.png',
        help='Путь к выходному графику (по умолчанию: zipf_plot.png)'
    )
    
    parser.add_argument(
        '--top-n-plot',
        type=int,
        default=1000,
        help='Количество топ-токенов для визуализации (по умолчанию: 1000)'
    )
    
    parser.add_argument(
        '--verbose',
        '-v',
        action='store_true',
        help='Подробный вывод логов'
    )
    
    args = parser.parse_args()
    
    
    log_level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        format='%(levelname)s: %(message)s',
        level=log_level
    )
    
    logging.info("=" * 60)
    logging.info("Анализ закона Ципфа")
    logging.info("=" * 60)
    
    try:
        
        logging.info("\n[Шаг 1] Загрузка токенов из директории...")
        freq_counter = load_token_frequencies(args.tokens_dir)
        
        
        logging.info("\n[Шаг 2] Ранжирование токенов...")
        ranked_data = rank_and_sort_frequencies(freq_counter)
        
        
        logging.info("\n[Шаг 3] Сохранение в CSV...")
        save_to_csv(ranked_data, args.output_csv)
        
        
        logging.info("\n[Шаг 4] Линейная регрессия (log-log)...")
        log_ranks, log_freqs, reg_results = perform_zipf_analysis(ranked_data)
        
        
        logging.info("\n[Шаг 5] Построение графика...")
        plot_zipf_law(
            log_ranks, 
            log_freqs, 
            reg_results, 
            args.output_plot,
            top_n=args.top_n_plot
        )
        
        
        logging.info("\n" + "=" * 60)
        logging.info("Анализ завершён успешно!")
        logging.info(f"  CSV: {args.output_csv} ({len(ranked_data)} записей)")
        logging.info(f"  График: {args.output_plot}")
        logging.info(f"\nОценка параметра Zipf (slope): {reg_results['slope']:.4f}")
        logging.info(f"Теоретическое значение: ~ -1.0")
        logging.info("=" * 60)
        
    except Exception as e:
        logging.error(f"Ошибка выполнения: {e}", exc_info=args.verbose)
        return 1
    
    return 0


if __name__ == '__main__':
    exit(main())