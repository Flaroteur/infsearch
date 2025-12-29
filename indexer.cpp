#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <dirent.h>
#include "dynamic_array.h"
#include "hash_table.h"

struct FileInfo {
    int doc_id_from_name;
    char name[512];
    
    FileInfo() : doc_id_from_name(0) { name[0] = '\0'; }
    
    bool operator<(const FileInfo& other) const {
        return doc_id_from_name < other.doc_id_from_name;
    }
	
	bool operator>(const FileInfo& other) const {
        return doc_id_from_name > other.doc_id_from_name;
    }
};


struct Posting {
    int doc_id;
    int term_freq;
    DynamicArray<int> positions;
    
    Posting() : doc_id(0), term_freq(0) {}
    Posting(int d) : doc_id(d), term_freq(0) {}
    
    bool operator<(const Posting& other) const {
        return doc_id < other.doc_id;
    }
    
    bool operator>(const Posting& other) const {
        return doc_id > other.doc_id;
    }
};


struct PostingsList {
    DynamicArray<Posting> postings;
    int doc_freq; 
    
    PostingsList() : doc_freq(0) {}
};


struct VocabularyEntry {
    char term[256];  
    int term_id;
    int doc_freq;
    long postings_offset; 
    
    VocabularyEntry() : term_id(0), doc_freq(0), postings_offset(0) {
        term[0] = '\0';
    }
    
    VocabularyEntry(const char* t, int id) : term_id(id), doc_freq(0), postings_offset(0) {
        strncpy(term, t, sizeof(term) - 1);
        term[sizeof(term) - 1] = '\0';
    }
    
    
    VocabularyEntry(const VocabularyEntry& other) 
        : term_id(other.term_id), doc_freq(other.doc_freq), postings_offset(other.postings_offset) {
        strncpy(term, other.term, sizeof(term) - 1);
        term[sizeof(term) - 1] = '\0';
    }
    
    
    VocabularyEntry& operator=(const VocabularyEntry& other) {
        if (this != &other) {
            strncpy(term, other.term, sizeof(term) - 1);
            term[sizeof(term) - 1] = '\0';
            term_id = other.term_id;
            doc_freq = other.doc_freq;
            postings_offset = other.postings_offset;
        }
        return *this;
    }
    
    
    bool operator<(const VocabularyEntry& other) const {
        return strcmp(term, other.term) < 0;
    }
    
    bool operator>(const VocabularyEntry& other) const {
        return strcmp(term, other.term) > 0;
    }
};





class Indexer {
private:
    StringHashTable term_to_id;
    DynamicArray<PostingsList*> index;
    DynamicArray<char*> doc_ids;
    int next_term_id;
    int num_docs;
    
    void ensure_index_size(int term_id) {
        while (static_cast<int>(index.size()) <= term_id) {
            index.push_back(nullptr);
        }
    }
    
public:
    Indexer() : next_term_id(0), num_docs(0) {}
    
    ~Indexer() {
        for (size_t i = 0; i < doc_ids.size(); i++) {
            if (doc_ids[i]) free(doc_ids[i]);
        }
        
        for (size_t i = 0; i < index.size(); i++) {
            if (index[i]) delete index[i];
        }
    }
    
    int add_document(const char* filename) {
        char* copy = static_cast<char*>(malloc(strlen(filename) + 1));
        strcpy(copy, filename);
        doc_ids.push_back(copy);
        return num_docs++;  
    }
    
    void add_term_occurrence(const char* term, int doc_id, int position) {
        
        if (doc_id < 0 || doc_id >= static_cast<int>(doc_ids.size())) {
            std::cerr << "ОШИБКА: Неверный doc_id " << doc_id << std::endl;
            return;
        }
        
        int term_id;
        if (!term_to_id.find(term, term_id)) {
            term_id = next_term_id++;
            term_to_id.insert(term, term_id);
            ensure_index_size(term_id);
            index[term_id] = new PostingsList();
        }
        
        PostingsList* plist = index[term_id];
        if (!plist) {
            std::cerr << "ОШИБКА: Пустой PostingsList" << std::endl;
            return;
        }
        
        
        Posting* posting = nullptr;
        for (size_t i = 0; i < plist->postings.size(); i++) {
            if (plist->postings[i].doc_id == doc_id) {
                posting = &plist->postings[i];
                break;
            }
        }
        
        if (!posting) {
            Posting new_posting(doc_id);
            plist->postings.push_back(new_posting);
            posting = &plist->postings[plist->postings.size() - 1];
            plist->doc_freq++;
        }
        
        posting->term_freq++;
        posting->positions.push_back(position);
    }
    
    void sort_postings() {
        std::cerr << "Сортировка постингов..." << std::endl;
        for (size_t i = 0; i < index.size(); i++) {
            if (index[i]) {
                index[i]->postings.sort_quick();
            }
        }
    }
    
    void save_index(const char* output_dir) {
        std::cerr << "Сохранение индекса в " << output_dir << std::endl;
        
        
        mkdir(output_dir, 0755);
        
        
        DynamicArray<VocabularyEntry> vocab;
        for (auto it = term_to_id.begin(); it != term_to_id.end(); ++it) {
            VocabularyEntry entry(it->key, it->value);
            if (it->value < static_cast<int>(index.size()) && index[it->value]) {
                entry.doc_freq = index[it->value]->doc_freq;
            }
            vocab.push_back(entry);
        }
        
        
        vocab.sort_quick();
        
        
        char vocab_path[512];
        char postings_path[512];
        snprintf(vocab_path, sizeof(vocab_path), "%s/vocab.txt", output_dir);
        snprintf(postings_path, sizeof(postings_path), "%s/postings.bin", output_dir);
        
        FILE* vocab_file = fopen(vocab_path, "w");
        FILE* postings_file = fopen(postings_path, "wb");
        
        if (!vocab_file || !postings_file) {
            std::cerr << "Ошибка: не удалось открыть выходные файлы" << std::endl;
            return;
        }
        
        long offset = 0;
        for (size_t i = 0; i < vocab.size(); i++) {
            VocabularyEntry& entry = vocab[i];
            PostingsList* plist = index[entry.term_id];
            
            if (!plist) continue;
            
            
            fprintf(vocab_file, "%s\t%d\t%d\t%ld\n", 
                   entry.term, entry.term_id, entry.doc_freq, offset);
            
            
            int num_postings = plist->postings.size();
            fwrite(&num_postings, sizeof(int), 1, postings_file);
            
            for (int j = 0; j < num_postings; j++) {
                Posting& p = plist->postings[j];
                
                
                fwrite(&p.doc_id, sizeof(int), 1, postings_file);
                
                
                fwrite(&p.term_freq, sizeof(int), 1, postings_file);
                
                
                int num_positions = p.positions.size();
                fwrite(&num_positions, sizeof(int), 1, postings_file);
                
                
                for (int k = 0; k < num_positions; k++) {
                    fwrite(&p.positions[k], sizeof(int), 1, postings_file);
                }
            }
            
            offset = ftell(postings_file);
        }
        
        fclose(vocab_file);
        fclose(postings_file);
        
        
        char docids_path[512];
        snprintf(docids_path, sizeof(docids_path), "%s/docids.txt", output_dir);
        FILE* docids_file = fopen(docids_path, "w");
        
        if (docids_file) {
            for (size_t i = 0; i < doc_ids.size(); i++) {
                fprintf(docids_file, "%zu\t%s\n", i, doc_ids[i]);
            }
            fclose(docids_file);
        }
        
        
        save_statistics(output_dir);
        
        std::cerr << "Индекс сохранён успешно" << std::endl;
    }
    
    void save_statistics(const char* output_dir) {
        char stats_path[512];
        snprintf(stats_path, sizeof(stats_path), "%s/index_stats.json", output_dir);
        
        FILE* stats_file = fopen(stats_path, "w");
        if (!stats_file) return;
        
        fprintf(stats_file, "{\n");
        fprintf(stats_file, "  \"num_documents\": %d,\n", num_docs);
        fprintf(stats_file, "  \"num_terms\": %zu,\n", term_to_id.size());
        
        
        long total_postings = 0;
        long total_positions = 0;
        for (size_t i = 0; i < index.size(); i++) {
            if (index[i]) {
                total_postings += index[i]->postings.size();
                for (size_t j = 0; j < index[i]->postings.size(); j++) {
                    total_positions += index[i]->postings[j].positions.size();
                }
            }
        }
        
        fprintf(stats_file, "  \"total_postings\": %ld,\n", total_postings);
        fprintf(stats_file, "  \"total_positions\": %ld\n", total_positions);
        fprintf(stats_file, "}\n");
        
        fclose(stats_file);
    }
    
    int get_num_docs() const { return num_docs; }
    int get_num_terms() const { return term_to_id.size(); }
};





void process_token_file(const char* filepath, int doc_id, Indexer& indexer) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        std::cerr << "Предупреждение: не удалось открыть " << filepath << std::endl;
        return;
    }
    
    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        char* term = strtok(line, " \t");
        if (!term) continue;
        
        char* pos_str = strtok(nullptr, " \t");
        if (!pos_str) continue; 
        
        do {
            int position = atoi(pos_str);
            if (position > 0) { 
                indexer.add_term_occurrence(term, doc_id, position);
            }
            pos_str = strtok(nullptr, " \t");
        } while (pos_str);
    }
    
    fclose(file);
}

void build_index_from_tokens(const char* tokens_dir, const char* output_dir, Indexer& indexer) {
    std::cerr << "Построение индекса из " << tokens_dir << std::endl;
    
    DIR* dir = opendir(tokens_dir);
    if (!dir) {
        std::cerr << "Ошибка: не удалось открыть директорию " << tokens_dir << std::endl;
        return;
    }
    
    struct dirent* entry;
    DynamicArray<FileInfo> files;
    
    
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        
        size_t len = strlen(name);
        if (len < 7 || strcmp(name + len - 7, ".tokens") != 0) continue;
        
        int doc_id_from_name;
        if (sscanf(name, "doc%d.tokens", &doc_id_from_name) != 1) continue;
        
        FileInfo info;
        info.doc_id_from_name = doc_id_from_name;
        strncpy(info.name, name, sizeof(info.name) - 1);
        info.name[sizeof(info.name) - 1] = '\0';
        files.push_back(info);
    }
    
    closedir(dir);
    
    
    files.sort_quick();
    
    
    for (size_t i = 0; i < files.size(); i++) {
        int doc_id_from_name = files[i].doc_id_from_name;
        const char* name = files[i].name;
        
        
        int actual_doc_id = indexer.add_document(name);
        
        
        if (actual_doc_id != doc_id_from_name - 1) {
            std::cerr << "Предупреждение: несоответствие doc_id для " << name 
                      << " (файл: " << (doc_id_from_name - 1) 
                      << ", фактический: " << actual_doc_id << ")" << std::endl;
        }
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", tokens_dir, name);
        process_token_file(filepath, actual_doc_id, indexer);
        
        if ((i + 1) % 100 == 0) {
            std::cerr << "Обработано " << (i + 1) << " файлов..." << std::endl;
        }
    }
    
    std::cerr << "Всего обработано: " << files.size() << " документов" << std::endl;
}





void process_corpus_file(const char* filepath, int doc_id, Indexer& indexer) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        std::cerr << "Предупреждение: не удалось открыть " << filepath << std::endl;
        return;
    }
    
    char word[256];
    int position = 0;
    int c;
    int word_len = 0;
    
    while ((c = fgetc(file)) != EOF) {
        if (isalnum(c) || c == '_') {
            if (word_len < 255) {
                word[word_len++] = tolower(c);
            }
        } else {
            if (word_len > 0) {
                word[word_len] = '\0';
                indexer.add_term_occurrence(word, doc_id, position);
                position++;
                word_len = 0;
            }
        }
    }
    
    
    if (word_len > 0) {
        word[word_len] = '\0';
        indexer.add_term_occurrence(word, doc_id, position);
    }
    
    fclose(file);
}

void build_index_from_corpus(const char* corpus_dir, const char* output_dir, Indexer& indexer) {
    std::cerr << "Построение индекса напрямую из " << corpus_dir << std::endl;
    
    DIR* dir = opendir(corpus_dir);
    if (!dir) {
        std::cerr << "Ошибка: не удалось открыть " << corpus_dir << std::endl;
        return;
    }
    
    struct dirent* entry;
    int processed = 0;
    
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        
        
        size_t len = strlen(name);
        if (len < 4 || strcmp(name + len - 4, ".txt") != 0) continue;
        
        
        int doc_id;
        if (sscanf(name, "doc%d.txt", &doc_id) != 1) continue;
        
        indexer.add_document(name);
        
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/%s", corpus_dir, name);
        process_corpus_file(filepath, doc_id - 1, indexer);
        
        processed++;
        if (processed % 100 == 0) {
            std::cerr << "Обработано " << processed << " документов..." << std::endl;
        }
    }
    
    closedir(dir);
    
    std::cerr << "Всего обработано: " << processed << " документов" << std::endl;
}





void print_usage(const char* program_name) {
    std::cout << "Использование: " << program_name << " [опции]\n\n"
              << "Опции:\n"
              << "  --tokens DIR      Директория с токенизированными файлами (.tokens)\n"
              << "  --corpus DIR      Директория с оригинальным корпусом (.txt)\n"
              << "  --out DIR         Выходная директория для индекса (по умолчанию: index/)\n"
              << "  --help            Показать эту справку\n\n"
              << "Примеры:\n"
              << "  # Из токенизированных файлов (рекомендуется)\n"
              << "  " << program_name << " --tokens tokens/ --out index/\n\n"
              << "  # Из оригинальных документов (базовая токенизация)\n"
              << "  " << program_name << " --corpus corpus/ --out index/\n";
}

int main(int argc, char* argv[]) {
    const char* tokens_dir = nullptr;
    const char* corpus_dir = nullptr;
    const char* output_dir = "index";
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--tokens") == 0 && i + 1 < argc) {
            tokens_dir = argv[++i];
        } else if (strcmp(argv[i], "--corpus") == 0 && i + 1 < argc) {
            corpus_dir = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else {
            std::cerr << "Неизвестная опция: " << argv[i] << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (!tokens_dir && !corpus_dir) {
        std::cerr << "Ошибка: необходимо указать --tokens или --corpus" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::cerr << "======================================" << std::endl;
    std::cerr << "Индексатор (ЛР6)" << std::endl;
    std::cerr << "======================================" << std::endl;
    
    Indexer indexer;
    
    
    if (tokens_dir) {
        build_index_from_tokens(tokens_dir, output_dir, indexer);
    } else {
        build_index_from_corpus(corpus_dir, output_dir, indexer);
    }
    
    
    indexer.sort_postings();
    
    
    indexer.save_index(output_dir);
    
    std::cerr << "\n======================================" << std::endl;
    std::cerr << "Индексирование завершено" << std::endl;
    std::cerr << "======================================" << std::endl;
    std::cerr << "Документов: " << indexer.get_num_docs() << std::endl;
    std::cerr << "Уникальных терминов: " << indexer.get_num_terms() << std::endl;
    std::cerr << "Выходная директория: " << output_dir << std::endl;
    
    return 0;
}