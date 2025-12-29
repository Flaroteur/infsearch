
import sys
import os

def check_vocab(vocab_path):
    print("=== Проверка vocabulary ===")
    terms = {}
    with open(vocab_path, 'r', encoding='utf-8') as f:
        for i, line in enumerate(f):
            parts = line.strip().split('\t')
            if len(parts) != 4:
                print(f"Строка {i}: неверный формат: {line}")
                continue
            term, term_id, doc_freq, offset = parts
            terms[term] = {
                'id': int(term_id),
                'df': int(doc_freq),
                'offset': int(offset)
            }
    print(f"Загружено {len(terms)} терминов")
    return terms

def check_postings(postings_path, vocab, terms_to_check=None):
    print("\n=== Проверка postings.bin ===")
    if terms_to_check is None:
        terms_to_check = list(vocab.keys())[:10]  
    
    for term in terms_to_check:
        info = vocab.get(term)
        if not info:
            print(f"Термин '{term}' не найден в vocabulary")
            continue
        
        with open(postings_path, 'rb') as f:
            f.seek(info['offset'])
            
            import struct
            num_postings_bytes = f.read(4)
            if len(num_postings_bytes) < 4:
                print(f"Термин '{term}': не удалось прочитать количество постингов")
                continue
            
            num_postings = struct.unpack('i', num_postings_bytes)[0]
            print(f"Термин '{term}': {num_postings} постингов")
            
            
            for i in range(min(3, num_postings)):
                doc_id_bytes = f.read(4)
                term_freq_bytes = f.read(4)
                num_pos_bytes = f.read(4)
                
                if len(doc_id_bytes) < 4 or len(term_freq_bytes) < 4 or len(num_pos_bytes) < 4:
                    print(f"  Ошибка чтения постинга {i}")
                    break
                
                doc_id = struct.unpack('i', doc_id_bytes)[0]
                term_freq = struct.unpack('i', term_freq_bytes)[0]
                num_pos = struct.unpack('i', num_pos_bytes)[0]
                
                print(f"  [{i}] doc_id={doc_id}, term_freq={term_freq}, positions={num_pos}")
                
                
                f.seek(num_pos * 4, 1)  

def check_docids(docids_path):
    print("\n=== Проверка docids.txt ===")
    docids = {}
    with open(docids_path, 'r', encoding='utf-8') as f:
        for line in f:
            parts = line.strip().split('\t')
            if len(parts) != 2:
                print(f"Неверный формат: {line}")
                continue
            doc_id, filename = parts
            docids[int(doc_id)] = filename
    print(f"Загружено {len(docids)} doc_id")
    
    
    ids = sorted(docids.keys())
    if ids:
        max_id = max(ids)
        missing = [i for i in range(max_id) if i not in docids]
        if missing:
            print(f"Пропущенные doc_id: {missing[:10]}... (всего {len(missing)})")
    
    return docids

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} index_dir")
        sys.exit(1)
    
    index_dir = sys.argv[1]
    vocab_path = os.path.join(index_dir, 'vocab.txt')
    postings_path = os.path.join(index_dir, 'postings.bin')
    docids_path = os.path.join(index_dir, 'docids.txt')
    
    if not os.path.exists(vocab_path):
        print(f"Ошибка: {vocab_path} не найден")
        sys.exit(1)
    
    vocab = check_vocab(vocab_path)
    docids = check_docids(docids_path)
    check_postings(postings_path, vocab, ['в', 'абиссинский', 'ворон'])