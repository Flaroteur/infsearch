#!/usr/bin/env python3

import argparse
import requests
import time
import os
import sqlite3
import hashlib
import sys
import logging
import threading
import json
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Optional, Dict, Any

API_URL = "https://ru.wikipedia.org/w/api.php"
HEADERS = {"User-Agent": "VDA educational"}

DEFAULT_MIN_WORDS = 200
DEFAULT_REQUEST_DELAY = 0.5  
DEFAULT_MAX_RETRIES = 3
DEFAULT_WORKERS = 4
DEFAULT_DB_PATH = "crawler_state.db"
DEFAULT_LOG_FILE = "crawler.log"
DEFAULT_CORPUS_DIR = "corpus"
DEFAULT_METADATA_CSV = "metadata.csv"

RETRY_STATUS_CODES = {429, 503, 504}  
BACKOFF_STATUS_CODES = {429, 503}     


def setup_logging(log_file: str, verbose: bool = False):
    log_level = logging.DEBUG if verbose else logging.INFO
    
    log_format = '%(asctime)s [%(levelname)s] %(message)s'
    date_format = '%Y-%m-%d %H:%M:%S'
    
    logging.basicConfig(
        level=log_level,
        format=log_format,
        datefmt=date_format,
        handlers=[
            logging.FileHandler(log_file, encoding='utf-8'),
            logging.StreamHandler(sys.stdout)
        ]
    )
    
    return logging.getLogger(__name__)


class CrawlerState:
    
    def __init__(self, db_path: str):
        self.db_path = db_path
        self.conn = self._open_db()
        self.lock = threading.RLock()
        self._init_schema()
        
        
    def _open_db(self) -> sqlite3.Connection:
        conn = sqlite3.connect(
            self.db_path,
            check_same_thread=False,
            isolation_level=None  
        )
        
        conn.execute("PRAGMA journal_mode=WAL;")
        conn.execute("PRAGMA synchronous=NORMAL;")
        conn.execute("PRAGMA temp_store=MEMORY;")
        return conn
    
    def _init_schema(self):
        self.conn.executescript("""
        -- Таблица обнаруженных страниц
        CREATE TABLE IF NOT EXISTS pages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT UNIQUE NOT NULL,
            status INTEGER DEFAULT 0,  -- 0=new, 1=done, 2=in_progress, 3=failed
            sha1 TEXT,
            word_count INTEGER,
            source_url TEXT,
            doc_id INTEGER,
            attempts INTEGER DEFAULT 0,
            last_error TEXT,
            discovered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            processed_at TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_pages_status ON pages(status);
        CREATE INDEX IF NOT EXISTS idx_pages_sha1 ON pages(sha1);
        
        -- Очередь категорий для обхода (BFS)
        CREATE TABLE IF NOT EXISTS categories_queue (
            cat TEXT PRIMARY KEY,
            status INTEGER DEFAULT 0,  -- 0=pending, 1=processed
            discovered_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        -- Метаданные и счётчики
        CREATE TABLE IF NOT EXISTS metadata (
            key TEXT PRIMARY KEY,
            value TEXT,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        
        -- Лог событий (для отладки и анализа)
        CREATE TABLE IF NOT EXISTS event_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            event_type TEXT,
            message TEXT,
            details TEXT
        );
        """)
        
        
        with self.lock:
            cur = self.conn.cursor()
            cur.execute("INSERT OR IGNORE INTO metadata(key, value) VALUES('next_doc_id', '1')")
            cur.execute("INSERT OR IGNORE INTO metadata(key, value) VALUES('start_time', ?)", 
                       (datetime.now().isoformat(),))
            self.conn.commit()
    
    def log_event(self, event_type: str, message: str, details: Optional[Dict] = None):
        details_json = json.dumps(details, ensure_ascii=False) if details else None
        with self.lock:
            cur = self.conn.cursor()
            cur.execute(
                "INSERT INTO event_log(event_type, message, details) VALUES(?, ?, ?)",
                (event_type, message, details_json)
            )
            self.conn.commit()
    
    def add_category(self, category: str):
        try:
            with self.lock:
                self.conn.execute(
                    "INSERT OR IGNORE INTO categories_queue(cat, status) VALUES(?, 0)",
                    (category,)
                )
                self.conn.commit()
        except Exception as e:
            logging.error(f"Ошибка добавления категории {category}: {e}")
    
    def add_page(self, title: str):
        try:
            with self.lock:
                self.conn.execute(
                    "INSERT OR IGNORE INTO pages(title, status) VALUES(?, 0)",
                    (title,)
                )
                self.conn.commit()
        except Exception as e:
            logging.debug(f"Ошибка добавления страницы {title}: {e}")
    
    def get_next_category(self) -> Optional[str]:
        with self.lock:
            cur = self.conn.cursor()
            cur.execute("SELECT cat FROM categories_queue WHERE status=0 LIMIT 1")
            row = cur.fetchone()
            if row:
                cat = row[0]
                cur.execute("UPDATE categories_queue SET status=1 WHERE cat=?", (cat,))
                self.conn.commit()
                return cat
            return None
    
    def get_next_page(self) -> Optional[tuple]:
        with self.lock:
            cur = self.conn.cursor()
            
            cur.execute("""
                SELECT id, title FROM pages 
                WHERE status IN (0, 3) AND attempts < ?
                ORDER BY attempts ASC, id ASC
                LIMIT 1
            """, (DEFAULT_MAX_RETRIES,))
            row = cur.fetchone()
            if not row:
                return None
            
            page_id, title = row
            
            cur.execute(
                "UPDATE pages SET status=2, attempts=attempts+1 WHERE id=? AND status IN (0, 3)",
                (page_id,)
            )
            if cur.rowcount != 1:
                return None  
            self.conn.commit()
            return page_id, title
    
    def mark_page_done(self, page_id: int, sha1: str, word_count: int, 
                       source_url: str, doc_id: int):
        with self.lock:
            cur = self.conn.cursor()
            cur.execute("""
                UPDATE pages 
                SET status=1, sha1=?, word_count=?, source_url=?, doc_id=?, 
                    processed_at=CURRENT_TIMESTAMP
                WHERE id=?
            """, (sha1, word_count, source_url, doc_id, page_id))
            self.conn.commit()
    
    def mark_page_failed(self, page_id: int, error: str):
        with self.lock:
            cur = self.conn.cursor()
            cur.execute(
                "UPDATE pages SET status=3, last_error=? WHERE id=?",
                (error, page_id)
            )
            self.conn.commit()
    
    def mark_page_skipped(self, page_id: int, reason: str):
        with self.lock:
            cur = self.conn.cursor()
            cur.execute(
                "UPDATE pages SET status=1, last_error=? WHERE id=?",
                (f"skipped: {reason}", page_id)
            )
            self.conn.commit()
    
    def is_duplicate(self, sha1: str) -> bool:
        with self.lock:
            cur = self.conn.cursor()
            cur.execute("SELECT 1 FROM pages WHERE sha1=? AND doc_id IS NOT NULL LIMIT 1", (sha1,))
            return cur.fetchone() is not None
    
    def get_next_doc_id(self) -> int:
        with self.lock:
            cur = self.conn.cursor()
            cur.execute("SELECT value FROM metadata WHERE key='next_doc_id'")
            doc_id = int(cur.fetchone()[0])
            cur.execute("UPDATE metadata SET value=?, updated_at=CURRENT_TIMESTAMP WHERE key='next_doc_id'",
                       (str(doc_id + 1),))
            self.conn.commit()
            return doc_id
    
    def get_statistics(self) -> Dict[str, Any]:
        with self.lock:
            cur = self.conn.cursor()
            
            
            cur.execute("SELECT status, COUNT(*) FROM pages GROUP BY status")
            status_counts = dict(cur.fetchall())
            
            
            cur.execute("SELECT COUNT(*) FROM pages")
            total_pages = cur.fetchone()[0]
            
            
            cur.execute("SELECT COUNT(*) FROM categories_queue")
            total_cats = cur.fetchone()[0]
            
            
            cur.execute("SELECT COUNT(*) FROM pages WHERE doc_id IS NOT NULL")
            saved_docs = cur.fetchone()[0]
            
            return {
                'total_pages': total_pages,
                'total_categories': total_cats,
                'saved_documents': saved_docs,
                'status_new': status_counts.get(0, 0),
                'status_done': status_counts.get(1, 0),
                'status_in_progress': status_counts.get(2, 0),
                'status_failed': status_counts.get(3, 0)
            }
    
    def close(self):
        self.conn.close()




class RobustHTTPClient:
    
    def __init__(self, delay: float = DEFAULT_REQUEST_DELAY, 
                 max_retries: int = DEFAULT_MAX_RETRIES):
        self.session = requests.Session()
        self.delay = delay
        self.max_retries = max_retries
        self.last_request_time = 0
        self.lock = threading.Lock()
    
    def _wait_politeness(self):
        with self.lock:
            elapsed = time.time() - self.last_request_time
            if elapsed < self.delay:
                time.sleep(self.delay - elapsed)
            self.last_request_time = time.time()
    
    def get(self, url: str, params: Dict, timeout: int = 30) -> Optional[Dict]:
        params = dict(params)
        params["format"] = "json"
        
        for attempt in range(self.max_retries):
            try:
                self._wait_politeness()
                
                response = self.session.get(
                    url, 
                    params=params, 
                    headers=HEADERS, 
                    timeout=timeout
                )
                
                
                if response.status_code in BACKOFF_STATUS_CODES:
                    wait_time = self._calculate_backoff(attempt, response)
                    logging.warning(
                        f"HTTP {response.status_code}, ожидание {wait_time:.1f}s "
                        f"(попытка {attempt+1}/{self.max_retries})"
                    )
                    time.sleep(wait_time)
                    continue
                
                response.raise_for_status()
                return response.json()
                
            except requests.exceptions.Timeout as e:
                logging.warning(f"Timeout (попытка {attempt+1}/{self.max_retries}): {e}")
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)
                    
            except requests.exceptions.RequestException as e:
                logging.warning(f"Request error (попытка {attempt+1}/{self.max_retries}): {e}")
                if attempt < self.max_retries - 1:
                    time.sleep(2 ** attempt)
        
        logging.error(f"Все попытки исчерпаны для URL: {url}")
        return None
    
    def _calculate_backoff(self, attempt: int, response: requests.Response) -> float:
        
        retry_after = response.headers.get('Retry-After')
        if retry_after and retry_after.isdigit():
            return float(retry_after)
        
        
        return min(2 ** attempt, 60)  




class CategoryDiscoverer:
    
    def __init__(self, state: CrawlerState, client: RobustHTTPClient):
        self.state = state
        self.client = client
    
    def discover_from_category(self, root_category: str, max_depth: int = 3):

        logging.info(f"Начало обхода категории: {root_category}")
        self.state.add_category(root_category)
        self.state.log_event("discovery_start", f"Root category: {root_category}")
        
        processed = 0
        while True:
            cat = self.state.get_next_category()
            if not cat:
                break
            
            processed += 1
            logging.info(f"Обработка категории [{processed}]: {cat}")
            self._process_category(cat)
        
        stats = self.state.get_statistics()
        logging.info(f"Обход завершён. Обнаружено: {stats['total_pages']} страниц, "
                    f"{stats['total_categories']} категорий")
        self.state.log_event("discovery_complete", "Category discovery finished", stats)
    
    def _process_category(self, category: str):
        continuation = {}
        pages_found = 0
        subcats_found = 0
        
        while True:
            params = {
                "action": "query",
                "list": "categorymembers",
                "cmtitle": f"Category:{category}",
                "cmlimit": "500"
            }
            params.update(continuation)
            
            data = self.client.get(API_URL, params)
            if not data:
                logging.error(f"Не удалось получить данные для категории: {category}")
                break
            
            members = data.get("query", {}).get("categorymembers", [])
            for item in members:
                namespace = item.get("ns", 0)
                title = item.get("title")
                
                if namespace == 0:  
                    self.state.add_page(title)
                    pages_found += 1
                elif namespace == 14:  
                    subcat = title.replace("Category:", "").replace("Категория:", "")
                    self.state.add_category(subcat)
                    subcats_found += 1
            
            if "continue" not in data:
                break
            continuation = data["continue"]
        
        logging.info(f"  └─ Найдено: {pages_found} страниц, {subcats_found} подкатегорий")




class ArticleProcessor:
    
    def __init__(self, state: CrawlerState, client: RobustHTTPClient,
                 corpus_dir: str, metadata_csv: str, min_words: int):
        self.state = state
        self.client = client
        self.corpus_dir = corpus_dir
        self.metadata_csv = metadata_csv
        self.min_words = min_words
        self.metadata_lock = threading.Lock()
        
        
        os.makedirs(corpus_dir, exist_ok=True)
        
        
        self._init_metadata_csv()
    
    def _init_metadata_csv(self):
        if not os.path.exists(self.metadata_csv):
            with open(self.metadata_csv, 'w', encoding='utf-8') as f:
                f.write("doc_id,title,source_url,word_count\n")
    
    def process_article(self, page_id: int, title: str) -> bool:

        try:
            
            params = {
                "action": "query",
                "prop": "extracts|info",
                "inprop": "url",
                "explaintext": True,
                "titles": title
            }
            
            data = self.client.get(API_URL, params)
            if not data:
                self.state.mark_page_failed(page_id, "API request failed")
                return False
            
            pages = data.get("query", {}).get("pages", {})
            if not pages:
                self.state.mark_page_failed(page_id, "Empty API response")
                return False
            
            page = next(iter(pages.values()))
            extract = page.get("extract")
            fullurl = page.get("fullurl", "")
            
            if not extract:
                self.state.mark_page_skipped(page_id, "no extract")
                return False
            
            
            text = self._clean_text(extract)
            words = text.split()
            word_count = len(words)
            
            
            if word_count < self.min_words:
                self.state.mark_page_skipped(page_id, f"too short ({word_count} words)")
                logging.debug(f"Пропущена (мало слов): {title} ({word_count} слов)")
                return False
            
            
            sha1 = hashlib.sha1(text.encode('utf-8')).hexdigest()
            if self.state.is_duplicate(sha1):
                self.state.mark_page_skipped(page_id, "duplicate content")
                logging.debug(f"Пропущена (дубликат): {title}")
                return False
            
            
            doc_id = self.state.get_next_doc_id()
            self._save_document(doc_id, title, text)
            self._append_metadata(doc_id, title, fullurl, word_count)
            
            
            self.state.mark_page_done(page_id, sha1, word_count, fullurl, doc_id)
            
            logging.info(f"✓ Сохранена: {title} (doc{doc_id:05d}, {word_count} слов)")
            return True
            
        except Exception as e:
            error_msg = f"{type(e).__name__}: {str(e)}"
            self.state.mark_page_failed(page_id, error_msg)
            logging.error(f"Ошибка обработки {title}: {error_msg}")
            return False
    
    def _clean_text(self, extract: str) -> str:
        lines = []
        for line in extract.splitlines():
            s = line.strip()
            if not s:
                continue
            
            if (s.startswith("==") or s.startswith("{{") or 
                s.startswith("[[File:") or s.startswith("[[Категория:")):
                continue
            lines.append(s)
        return "\n".join(lines)
    
    def _save_document(self, doc_id: int, title: str, text: str):
        filename = os.path.join(self.corpus_dir, f"doc{doc_id:05d}.txt")
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(f"{title}\n\n{text}")
    
    def _append_metadata(self, doc_id: int, title: str, url: str, word_count: int):
        with self.metadata_lock:
            with open(self.metadata_csv, 'a', encoding='utf-8', newline='') as f:
                
                safe_title = title.replace('"', '""')
                f.write(f'{doc_id},"{safe_title}",{url},{word_count}\n')




def worker_thread(worker_id: int, state: CrawlerState, processor: ArticleProcessor,
                  client: RobustHTTPClient, max_docs: Optional[int]):
    logging.info(f"Воркер {worker_id} запущен")
    processed = 0
    
    while True:
        
        if max_docs:
            stats = state.get_statistics()
            if stats['saved_documents'] >= max_docs:
                logging.info(f"Воркер {worker_id}: достигнут лимит документов ({max_docs})")
                break
        
        
        page_data = state.get_next_page()
        if not page_data:
            logging.info(f"Воркер {worker_id}: больше нет страниц для обработки")
            break
        
        page_id, title = page_data
        
        
        success = processor.process_article(page_id, title)
        if success:
            processed += 1
    
    logging.info(f"Воркер {worker_id} завершён (обработано: {processed})")




def main():
    parser = argparse.ArgumentParser(
        description="Поисковый робот для ЛР2 (устойчивый к сбоям, с логированием)"
    )
    parser.add_argument("--category", required=True, 
                       help="Корневая категория Wikipedia (без 'Category:')")
    parser.add_argument("--max-docs", type=int, default=30000,
                       help="Максимальное количество документов")
    parser.add_argument("--min-words", type=int, default=DEFAULT_MIN_WORDS,
                       help="Минимальное количество слов в документе")
    parser.add_argument("--workers", type=int, default=DEFAULT_WORKERS,
                       help="Количество параллельных воркеров")
    parser.add_argument("--delay", type=float, default=DEFAULT_REQUEST_DELAY,
                       help="Задержка между запросами (politeness)")
    parser.add_argument("--corpus-dir", default=DEFAULT_CORPUS_DIR,
                       help="Директория для сохранения корпуса")
    parser.add_argument("--db", default=DEFAULT_DB_PATH,
                       help="Путь к файлу состояния (SQLite)")
    parser.add_argument("--log", default=DEFAULT_LOG_FILE,
                       help="Путь к файлу логов")
    parser.add_argument("--metadata", default=DEFAULT_METADATA_CSV,
                       help="Путь к файлу метаданных")
    parser.add_argument("--verbose", action="store_true",
                       help="Подробное логирование (DEBUG уровень)")
    parser.add_argument("--resume", action="store_true",
                       help="Продолжить с места остановки (пропустить обнаружение категорий)")
    
    args = parser.parse_args()
    
    
    logger = setup_logging(args.log, args.verbose)
    
    logger.info("=" * 70)
    logger.info("Запуск поискового робота (ЛР2)")
    logger.info("=" * 70)
    logger.info(f"Категория: {args.category}")
    logger.info(f"Макс. документов: {args.max_docs}")
    logger.info(f"Мин. слов: {args.min_words}")
    logger.info(f"Воркеров: {args.workers}")
    logger.info(f"Задержка: {args.delay}s")
    logger.info(f"БД состояния: {args.db}")
    logger.info(f"Режим: {'RESUME' if args.resume else 'NEW'}")
    logger.info("=" * 70)
    
    
    state = CrawlerState(args.db)
    client = RobustHTTPClient(delay=args.delay)
    
    
    if not args.resume:
        logger.info("\n[ФАЗА 1] Обнаружение страниц через категории...")
        discoverer = CategoryDiscoverer(state, client)
        try:
            discoverer.discover_from_category(args.category)
        except KeyboardInterrupt:
            logger.warning("Прервано пользователем во время обнаружения")
            state.close()
            return
    else:
        logger.info("\n[ФАЗА 1] Пропущена (режим --resume)")
        stats = state.get_statistics()
        logger.info(f"В базе уже есть {stats['total_pages']} страниц")
    
    
    logger.info("\n[ФАЗА 2] Обработка страниц...")
    processor = ArticleProcessor(
        state, client, args.corpus_dir, args.metadata, args.min_words
    )
    
    
    logger.info(f"Запуск {args.workers} воркер(ов)...")
    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = []
        for wid in range(args.workers):
            future = executor.submit(
                worker_thread, wid + 1, state, processor, 
                RobustHTTPClient(delay=args.delay),  
                args.max_docs
            )
            futures.append(future)
        
        try:
            
            for future in as_completed(futures):
                try:
                    future.result()
                except Exception as e:
                    logger.error(f"Ошибка в воркере: {e}")
        except KeyboardInterrupt:
            logger.warning("\nПрервано пользователем")
            logger.info("Состояние сохранено, можно продолжить с --resume")
    
    
    logger.info("\n" + "=" * 70)
    logger.info("ЗАВЕРШЕНИЕ РАБОТЫ")
    logger.info("=" * 70)
    
    stats = state.get_statistics()
    logger.info(f"Всего страниц обнаружено: {stats['total_pages']}")
    logger.info(f"Документов сохранено: {stats['saved_documents']}")
    logger.info(f"Успешно обработано: {stats['status_done']}")
    logger.info(f"В процессе: {stats['status_in_progress']}")
    logger.info(f"Неудачных попыток: {stats['status_failed']}")
    logger.info(f"Новых (не обработано): {stats['status_new']}")
    
    state.log_event("crawler_complete", "Crawler finished", stats)
    state.close()
    
    logger.info(f"\nЛоги: {args.log}")
    logger.info(f"Состояние: {args.db}")
    logger.info(f"Корпус: {args.corpus_dir}/")
    logger.info(f"Метаданные: {args.metadata}")
    logger.info("=" * 70)


if __name__ == "__main__":
    main()