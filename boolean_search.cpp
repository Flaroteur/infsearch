#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "dynamic_array.h"
#include "query_parser.h"

struct Posting {
    int doc_id;
    int term_freq;
    DynamicArray<int> positions;
    
    Posting() : doc_id(0), term_freq(0) {}
    Posting(int d) : doc_id(d), term_freq(0) {}
    
    bool operator<(const Posting& other) const {
        return doc_id < other.doc_id;
    }
};

struct PostingsList {
    DynamicArray<Posting> postings;
    
    PostingsList() {}
};


struct TermInfo {
    int term_id;
    int doc_freq;
    long postings_offset;
    char term_str[256];  
    
    TermInfo() : term_id(-1), doc_freq(0), postings_offset(0) {
        term_str[0] = '\0';
    }
    
    TermInfo(int id, int df, long offset) 
        : term_id(id), doc_freq(df), postings_offset(offset) {
        term_str[0] = '\0';
    }
    
    TermInfo(const TermInfo& other) 
        : term_id(other.term_id), doc_freq(other.doc_freq), 
          postings_offset(other.postings_offset) {
        strncpy(term_str, other.term_str, sizeof(term_str) - 1);
        term_str[sizeof(term_str) - 1] = '\0';
    }
    
    TermInfo& operator=(const TermInfo& other) {
        if (this != &other) {
            term_id = other.term_id;
            doc_freq = other.doc_freq;
            postings_offset = other.postings_offset;
            strncpy(term_str, other.term_str, sizeof(term_str) - 1);
            term_str[sizeof(term_str) - 1] = '\0';
        }
        return *this;
    }
};


class SearchIndex {
private:
    DynamicArray<TermInfo> vocabulary;  
    DynamicArray<char*> docids;  
    char postings_path[512];
    int num_documents;
    
public:
    SearchIndex() : num_documents(0) {
        postings_path[0] = '\0';
    }
    
    ~SearchIndex() {
        for (size_t i = 0; i < docids.size(); i++) {
            if (docids[i]) {
                free(docids[i]);
            }
        }
    }
    
    bool load(const char* index_dir) {
        std::cerr << "Загрузка индекса из " << index_dir << std::endl;
        
        
        snprintf(postings_path, sizeof(postings_path), "%s/postings.bin", index_dir);
        
        
        char vocab_path[512];
        snprintf(vocab_path, sizeof(vocab_path), "%s/vocab.txt", index_dir);
        
        FILE* vocab_file = fopen(vocab_path, "r");
        if (!vocab_file) {
            std::cerr << "Ошибка: не удалось открыть " << vocab_path << std::endl;
            return false;
        }
        
        char line[1024];
        while (fgets(line, sizeof(line), vocab_file)) {
            char term[256];
            int term_id, doc_freq;
            long offset;
            
            if (sscanf(line, "%255[^\t]\t%d\t%d\t%ld", term, &term_id, &doc_freq, &offset) == 4) {
                TermInfo info(term_id, doc_freq, offset);
                strncpy(info.term_str, term, sizeof(info.term_str) - 1);
                info.term_str[sizeof(info.term_str) - 1] = '\0';
                vocabulary.push_back(info);
            }
        }
        
        fclose(vocab_file);
        
        
        char docids_path[512];
        snprintf(docids_path, sizeof(docids_path), "%s/docids.txt", index_dir);
        
        FILE* docids_file = fopen(docids_path, "r");
        if (!docids_file) {
            std::cerr << "Ошибка: не удалось открыть " << docids_path << std::endl;
            return false;
        }
        
        while (fgets(line, sizeof(line), docids_file)) {
            int doc_id;
            char filename[256];
            
            if (sscanf(line, "%d\t%255[^\n]", &doc_id, filename) == 2) {
                
                while (static_cast<int>(docids.size()) <= doc_id) {
                    docids.push_back(nullptr);
                }
                
                char* copy = static_cast<char*>(malloc(strlen(filename) + 1));
                strcpy(copy, filename);
                docids[doc_id] = copy;
                num_documents = doc_id + 1;
            }
        }
        
        fclose(docids_file);
        
        std::cerr << "Загружено: " << vocabulary.size() << " терминов, " 
                  << num_documents << " документов" << std::endl;
        
        return true;
    }
    
    PostingsList* get_postings(const char* term) {
        
        
        TermInfo* found = nullptr;
        for (size_t i = 0; i < vocabulary.size(); i++) {
            if (strcmp(vocabulary[i].term_str, term) == 0) {
                found = &vocabulary[i];
                break;
            }
        }
        
        if (!found) {
            return nullptr;
        }
        
        
        FILE* file = fopen(postings_path, "rb");
        if (!file) {
            return nullptr;
        }
        
        fseek(file, found->postings_offset, SEEK_SET);
        
        
        int num_postings;
        if (fread(&num_postings, sizeof(int), 1, file) != 1) {
            fclose(file);
            return nullptr;
        }
        
        PostingsList* plist = new PostingsList();
        
        for (int i = 0; i < num_postings; i++) {
            int doc_id, term_freq, num_positions;
            
            fread(&doc_id, sizeof(int), 1, file);
            fread(&term_freq, sizeof(int), 1, file);
            fread(&num_positions, sizeof(int), 1, file);
            
            Posting p(doc_id);
            p.term_freq = term_freq;
            
            for (int j = 0; j < num_positions; j++) {
                int pos;
                fread(&pos, sizeof(int), 1, file);
                p.positions.push_back(pos);
            }
            
            plist->postings.push_back(p);
        }
        
        fclose(file);
        return plist;
    }
    
    const char* get_doc_filename(int doc_id) {
        if (doc_id >= 0 && doc_id < static_cast<int>(docids.size())) {
            return docids[doc_id];
        }
        return nullptr;
    }
    
    int get_num_documents() const {
        return num_documents;
    }
};

DynamicArray<int> intersect_postings(PostingsList* l1, PostingsList* l2) {
    DynamicArray<int> result;
    
    if (!l1 || !l2) return result;
    
    size_t i = 0, j = 0;
    
    while (i < l1->postings.size() && j < l2->postings.size()) {
        int doc1 = l1->postings[i].doc_id;
        int doc2 = l2->postings[j].doc_id;
        
        if (doc1 == doc2) {
            result.push_back(doc1);
            i++;
            j++;
        } else if (doc1 < doc2) {
            i++;
        } else {
            j++;
        }
    }
    
    return result;
}


DynamicArray<int> union_postings(PostingsList* l1, PostingsList* l2) {
    DynamicArray<int> result;
    
    if (!l1 && !l2) return result;
    
    if (!l1) {
        for (size_t i = 0; i < l2->postings.size(); i++) {
            result.push_back(l2->postings[i].doc_id);
        }
        return result;
    }
    
    if (!l2) {
        for (size_t i = 0; i < l1->postings.size(); i++) {
            result.push_back(l1->postings[i].doc_id);
        }
        return result;
    }
    
    size_t i = 0, j = 0;
    
    while (i < l1->postings.size() && j < l2->postings.size()) {
        int doc1 = l1->postings[i].doc_id;
        int doc2 = l2->postings[j].doc_id;
        
        if (doc1 == doc2) {
            result.push_back(doc1);
            i++;
            j++;
        } else if (doc1 < doc2) {
            result.push_back(doc1);
            i++;
        } else {
            result.push_back(doc2);
            j++;
        }
    }
    
    while (i < l1->postings.size()) {
        result.push_back(l1->postings[i].doc_id);
        i++;
    }
    
    while (j < l2->postings.size()) {
        result.push_back(l2->postings[j].doc_id);
        j++;
    }
    
    return result;
}


DynamicArray<int> complement_postings(PostingsList* l, int num_docs) {
    DynamicArray<int> result;
    
    if (!l) {
        
        for (int i = 0; i < num_docs; i++) {
            result.push_back(i);
        }
        return result;
    }
    
    int doc_idx = 0;
    size_t posting_idx = 0;
    
    while (doc_idx < num_docs) {
        if (posting_idx < l->postings.size() && 
            l->postings[posting_idx].doc_id == doc_idx) {
            
            posting_idx++;
        } else {
            result.push_back(doc_idx);
        }
        doc_idx++;
    }
    
    return result;
}


DynamicArray<int> postings_to_docids(PostingsList* l) {
    DynamicArray<int> result;
    if (l) {
        for (size_t i = 0; i < l->postings.size(); i++) {
            result.push_back(l->postings[i].doc_id);
        }
    }
    return result;
}


PostingsList* docids_to_postings(const DynamicArray<int>& docids) {
    PostingsList* l = new PostingsList();
    for (size_t i = 0; i < docids.size(); i++) {
        Posting p(docids[i]);
        l->postings.push_back(p);
    }
    return l;
}


int extract_doc_number(const char* filename) {
    int num = 0;
    if (sscanf(filename, "doc%d", &num) == 1) {
        return num;
    }
    return -1;
}


char* read_first_line(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        return nullptr;
    }
    
    char* line = static_cast<char*>(malloc(1024));
    if (!line) {
        fclose(file);
        return nullptr;
    }
    
    if (fgets(line, 1024, file)) {
        
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        fclose(file);
        return line;
    }
    
    free(line);
    fclose(file);
    return nullptr;
}

class QueryEvaluator {
private:
    SearchIndex* index;
    
    DynamicArray<int> evaluate_node(ASTNode* node) {
        if (!node) {
            return DynamicArray<int>();
        }
        
        switch (node->type) {
            case NODE_TERM: {
                PostingsList* plist = index->get_postings(node->term);
                return postings_to_docids(plist);
            }
            
            case NODE_AND: {
                DynamicArray<int> left = evaluate_node(node->left);
                DynamicArray<int> right = evaluate_node(node->right);
                
                PostingsList* l1 = docids_to_postings(left);
                PostingsList* l2 = docids_to_postings(right);
                
                DynamicArray<int> result = intersect_postings(l1, l2);
                
                delete l1;
                delete l2;
                
                return result;
            }
            
            case NODE_OR: {
                DynamicArray<int> left = evaluate_node(node->left);
                DynamicArray<int> right = evaluate_node(node->right);
                
                PostingsList* l1 = docids_to_postings(left);
                PostingsList* l2 = docids_to_postings(right);
                
                DynamicArray<int> result = union_postings(l1, l2);
                
                delete l1;
                delete l2;
                
                return result;
            }
            
            case NODE_NOT: {
                DynamicArray<int> left = evaluate_node(node->left);
                PostingsList* l = docids_to_postings(left);
                
                DynamicArray<int> result = complement_postings(l, index->get_num_documents());
                
                delete l;
                
                return result;
            }
        }
        
        return DynamicArray<int>();
    }
    
public:
    QueryEvaluator(SearchIndex* idx) : index(idx) {}
    
    DynamicArray<int> evaluate(ASTNode* query) {
        return evaluate_node(query);
    }
};


void print_usage(const char* program_name) {
    std::cout << "Использование: " << program_name << " [опции]\n\n"
              << "Опции:\n"
              << "  --index DIR       Директория с индексом (по умолчанию: index/)\n"
              << "  --corpus DIR      Директория с оригинальным корпусом (опционально)\n"
              << "  --metadata FILE   Файл с метаданными (опционально)\n"
              << "  --help            Показать эту справку\n\n"
              << "Формат запроса:\n"
              << "  term1 AND term2         Пересечение\n"
              << "  term1 OR term2          Объединение\n"
              << "  NOT term1               Дополнение\n"
              << "  (term1 OR term2) AND term3  Скобки для группировки\n\n"
              << "Примеры:\n"
              << "  echo \"python AND programming\" | " << program_name << " --index index/\n"
              << "  echo \"(java OR python) AND NOT tutorial\" | " << program_name << " --corpus corpus/\n";
}

int main(int argc, char* argv[]) {
    const char* index_dir = "index";
    const char* metadata_file = nullptr;
    const char* corpus_dir = nullptr;
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_dir = argv[++i];
        } else if (strcmp(argv[i], "--corpus") == 0 && i + 1 < argc) {
            corpus_dir = argv[++i];
        } else if (strcmp(argv[i], "--metadata") == 0 && i + 1 < argc) {
            metadata_file = argv[++i];
        }
    }
    
    
    SearchIndex index;
    if (!index.load(index_dir)) {
        std::cerr << "Ошибка загрузки индекса" << std::endl;
        return 1;
    }
    
    
    DynamicArray<char*> metadata_titles;
    if (metadata_file) {
        FILE* mf = fopen(metadata_file, "r");
        if (mf) {
            char line[1024];
            fgets(line, sizeof(line), mf); 
            
            while (fgets(line, sizeof(line), mf)) {
                int doc_id;
                char title[512];
                
                
                if (sscanf(line, "%d,\"%511[^\"]\"", &doc_id, title) == 2) {
                    
                    while (static_cast<int>(metadata_titles.size()) <= doc_id) {
                        metadata_titles.push_back(nullptr);
                    }
                    
                    char* title_copy = static_cast<char*>(malloc(strlen(title) + 1));
                    strcpy(title_copy, title);
                    metadata_titles[doc_id] = title_copy;
                }
            }
            
            fclose(mf);
        }
    }
    
    std::cerr << "\nБулев поиск готов. Введите запрос:" << std::endl;
    
    
    char query[4096];
    if (!fgets(query, sizeof(query), stdin)) {
        std::cerr << "Ошибка чтения запроса" << std::endl;
        return 1;
    }
    
    
    size_t len = strlen(query);
    if (len > 0 && query[len - 1] == '\n') {
        query[len - 1] = '\0';
    }
    
    std::cerr << "Запрос: " << query << std::endl;
    
    
    Lexer lexer(query);
    QueryParser parser(&lexer);
    
    ASTNode* ast = parser.parse();
    if (!ast) {
        std::cerr << "Ошибка парсинга запроса" << std::endl;
        return 1;
    }
    
    
    std::cerr << "\nДерево запроса:" << std::endl;
    print_ast(ast);
    std::cerr << std::endl;
    
    
    QueryEvaluator evaluator(&index);
    DynamicArray<int> results = evaluator.evaluate(ast);
    
    
    std::cout << "Найдено документов: " << results.size() << std::endl;
    std::cout << std::endl;
    
    for (size_t i = 0; i < results.size(); i++) {
        int doc_id = results[i];
        const char* filename = index.get_doc_filename(doc_id);
        
        std::cout << doc_id << "\t";
        
        if (filename) {
            std::cout << filename << "\t";
            
            
            if (corpus_dir) {
                int doc_num = extract_doc_number(filename);
                if (doc_num >= 0) {
                    char corpus_path[512];
                    snprintf(corpus_path, sizeof(corpus_path), "%s/doc%05d.txt", corpus_dir, doc_num);
                    
                    char* first_line = read_first_line(corpus_path);
                    if (first_line) {
                        std::cout << first_line;
                        free(first_line);
                    } else {
                        
                        std::cout << "(не удалось прочитать заголовок)";
                    }
                }
            } else if (metadata_file && doc_id < static_cast<int>(metadata_titles.size())) {
                
                const char* title = metadata_titles[doc_id];
                if (title) {
                    std::cout << title;
                }
            }
        }
        
        std::cout << std::endl;
    }
    
    
    for (size_t i = 0; i < metadata_titles.size(); i++) {
        if (metadata_titles[i]) {
            free(metadata_titles[i]);
        }
    }
    
    delete ast;
    
    return 0;
}