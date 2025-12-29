#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>

class RussianStemmer {
private:
    char word[256];
    int rv_pos;  
    int r1_pos;  
    int r2_pos;  
    
    
    bool is_vowel(unsigned char c1, unsigned char c2) {
        
        if (c1 == 0xD0) {
            return (c2 == 0xB0 || c2 == 0xB5 || c2 == 0xB8 ||  
                    c2 == 0xBE || c2 == 0xBF || c2 == 0xB2 ||  
                    c2 == 0xAD || c2 == 0xAE || c2 == 0xAF);   
        }
        if (c1 == 0xD1) {
            return (c2 == 0x91);  
        }
        return false;
    }
    
    
    void compute_regions() {
        int len = strlen(word);
        rv_pos = len;
        r1_pos = len;
        r2_pos = len;
        
        
        for (int i = 0; i < len - 1; i++) {
            if (is_vowel((unsigned char)word[i], (unsigned char)word[i+1])) {
                rv_pos = i + 2;
                break;
            }
        }
        
        
        bool found_consonant = false;
        for (int i = 0; i < len - 1; i++) {
            if (!is_vowel((unsigned char)word[i], (unsigned char)word[i+1])) {
                found_consonant = true;
            } else if (found_consonant) {
                r1_pos = i + 2;
                break;
            }
        }
        
        
        found_consonant = false;
        for (int i = r1_pos; i < len - 1; i++) {
            if (!is_vowel((unsigned char)word[i], (unsigned char)word[i+1])) {
                found_consonant = true;
            } else if (found_consonant) {
                r2_pos = i + 2;
                break;
            }
        }
    }
    
    
    bool remove_ending(const char* ending, int min_pos) {
        int len = strlen(word);
        int ending_len = strlen(ending);
        
        if (len < ending_len) return false;
        if (len - ending_len < min_pos) return false;
        
        
        if (strcmp(word + len - ending_len, ending) == 0) {
            word[len - ending_len] = '\0';
            return true;
        }
        return false;
    }
    
    
    bool step1() {
        
        if (remove_ending("вши", rv_pos)) return true;
        if (remove_ending("вшись", rv_pos)) return true;
        if (remove_ending("ывши", rv_pos)) return true;
        if (remove_ending("ывшись", rv_pos)) return true;
        
        
        if (remove_ending("ся", rv_pos)) return true;
        if (remove_ending("сь", rv_pos)) return true;
        
        
        if (remove_ending("ими", rv_pos)) return true;
        if (remove_ending("ыми", rv_pos)) return true;
        if (remove_ending("его", rv_pos)) return true;
        if (remove_ending("ого", rv_pos)) return true;
        if (remove_ending("ему", rv_pos)) return true;
        if (remove_ending("ому", rv_pos)) return true;
        if (remove_ending("ими", rv_pos)) return true;
        if (remove_ending("ыми", rv_pos)) return true;
        if (remove_ending("ей", rv_pos)) return true;
        if (remove_ending("ой", rv_pos)) return true;
        if (remove_ending("ий", rv_pos)) return true;
        if (remove_ending("ый", rv_pos)) return true;
        if (remove_ending("ую", rv_pos)) return true;
        if (remove_ending("юю", rv_pos)) return true;
        if (remove_ending("ая", rv_pos)) return true;
        if (remove_ending("яя", rv_pos)) return true;
        if (remove_ending("ое", rv_pos)) return true;
        if (remove_ending("ее", rv_pos)) return true;
        
        
        if (remove_ending("ивш", rv_pos)) return true;
        if (remove_ending("ывш", rv_pos)) return true;
        if (remove_ending("ующ", rv_pos)) return true;
        if (remove_ending("ющ", rv_pos)) return true;
        if (remove_ending("ем", rv_pos)) return true;
        if (remove_ending("нн", rv_pos)) return true;
        if (remove_ending("вш", rv_pos)) return true;
        if (remove_ending("ющ", rv_pos)) return true;
        if (remove_ending("щ", rv_pos)) return true;
        
        
        if (remove_ending("ете", rv_pos)) return true;
        if (remove_ending("ите", rv_pos)) return true;
        if (remove_ending("ешь", rv_pos)) return true;
        if (remove_ending("ишь", rv_pos)) return true;
        if (remove_ending("ила", rv_pos)) return true;
        if (remove_ending("ыла", rv_pos)) return true;
        if (remove_ending("ена", rv_pos)) return true;
        if (remove_ending("ите", rv_pos)) return true;
        if (remove_ending("или", rv_pos)) return true;
        if (remove_ending("ыли", rv_pos)) return true;
        if (remove_ending("ило", rv_pos)) return true;
        if (remove_ending("ыло", rv_pos)) return true;
        if (remove_ending("ено", rv_pos)) return true;
        if (remove_ending("ует", rv_pos)) return true;
        if (remove_ending("ует", rv_pos)) return true;
        if (remove_ending("ят", rv_pos)) return true;
        if (remove_ending("ал", rv_pos)) return true;
        if (remove_ending("ил", rv_pos)) return true;
        if (remove_ending("ыл", rv_pos)) return true;
        if (remove_ending("ем", rv_pos)) return true;
        if (remove_ending("им", rv_pos)) return true;
        if (remove_ending("ым", rv_pos)) return true;
        if (remove_ending("ен", rv_pos)) return true;
        if (remove_ending("ет", rv_pos)) return true;
        if (remove_ending("ит", rv_pos)) return true;
        if (remove_ending("ыт", rv_pos)) return true;
        if (remove_ending("ют", rv_pos)) return true;
        if (remove_ending("ны", rv_pos)) return true;
        if (remove_ending("ть", rv_pos)) return true;
        if (remove_ending("ешь", rv_pos)) return true;
        if (remove_ending("нно", rv_pos)) return true;
        if (remove_ending("ла", rv_pos)) return true;
        if (remove_ending("на", rv_pos)) return true;
        if (remove_ending("ли", rv_pos)) return true;
        if (remove_ending("ло", rv_pos)) return true;
        if (remove_ending("но", rv_pos)) return true;
        if (remove_ending("ет", rv_pos)) return true;
        if (remove_ending("ют", rv_pos)) return true;
        if (remove_ending("ны", rv_pos)) return true;
        if (remove_ending("ть", rv_pos)) return true;
        if (remove_ending("й", rv_pos)) return true;
        if (remove_ending("л", rv_pos)) return true;
        if (remove_ending("н", rv_pos)) return true;
        
        
        if (remove_ending("иями", rv_pos)) return true;
        if (remove_ending("ями", rv_pos)) return true;
        if (remove_ending("ами", rv_pos)) return true;
        if (remove_ending("ией", rv_pos)) return true;
        if (remove_ending("ией", rv_pos)) return true;
        if (remove_ending("ьми", rv_pos)) return true;
        if (remove_ending("ием", rv_pos)) return true;
        if (remove_ending("ием", rv_pos)) return true;
        if (remove_ending("ьем", rv_pos)) return true;
        if (remove_ending("иям", rv_pos)) return true;
        if (remove_ending("ням", rv_pos)) return true;
        if (remove_ending("ам", rv_pos)) return true;
        if (remove_ending("ем", rv_pos)) return true;
        if (remove_ending("ом", rv_pos)) return true;
        if (remove_ending("ах", rv_pos)) return true;
        if (remove_ending("ях", rv_pos)) return true;
        if (remove_ending("ия", rv_pos)) return true;
        if (remove_ending("ья", rv_pos)) return true;
        if (remove_ending("ию", rv_pos)) return true;
        if (remove_ending("ью", rv_pos)) return true;
        if (remove_ending("ия", rv_pos)) return true;
        if (remove_ending("ев", rv_pos)) return true;
        if (remove_ending("ов", rv_pos)) return true;
        if (remove_ending("ие", rv_pos)) return true;
        if (remove_ending("ье", rv_pos)) return true;
        if (remove_ending("ии", rv_pos)) return true;
        if (remove_ending("ьи", rv_pos)) return true;
        if (remove_ending("ей", rv_pos)) return true;
        if (remove_ending("ой", rv_pos)) return true;
        if (remove_ending("ий", rv_pos)) return true;
        if (remove_ending("ем", rv_pos)) return true;
        if (remove_ending("ам", rv_pos)) return true;
        if (remove_ending("ом", rv_pos)) return true;
        if (remove_ending("ах", rv_pos)) return true;
        if (remove_ending("ях", rv_pos)) return true;
        if (remove_ending("ы", rv_pos)) return true;
        if (remove_ending("ь", rv_pos)) return true;
        if (remove_ending("ю", rv_pos)) return true;
        if (remove_ending("а", rv_pos)) return true;
        if (remove_ending("е", rv_pos)) return true;
        if (remove_ending("и", rv_pos)) return true;
        if (remove_ending("о", rv_pos)) return true;
        if (remove_ending("у", rv_pos)) return true;
        if (remove_ending("я", rv_pos)) return true;
        
        return false;
    }
    
    
    void step2() {
        remove_ending("и", rv_pos);
    }
    
    
    void step3() {
        if (remove_ending("ость", r2_pos)) return;
        if (remove_ending("ост", r2_pos)) return;
    }
    
    
    void step4() {
        if (remove_ending("ейш", rv_pos)) return;
        if (remove_ending("ейше", rv_pos)) return;
        remove_ending("ь", rv_pos);
    }
    
public:
    RussianStemmer() {
        word[0] = '\0';
    }
    
    
    const char* stem(const char* input) {
        
        strncpy(word, input, sizeof(word) - 1);
        word[sizeof(word) - 1] = '\0';
        
        
        int len = strlen(word);
        if (len < 3) {
            return word;
        }
        
        
        compute_regions();
        
        
        step1();
        step2();
        step3();
        step4();
        
        return word;
    }
};

void process_token_file(const char* input_path, const char* output_path, 
                       FILE* stems_log, RussianStemmer& stemmer) {
    FILE* input = fopen(input_path, "r");
    if (!input) {
        std::cerr << "Ошибка: не удалось открыть " << input_path << std::endl;
        return;
    }
    
    FILE* output = fopen(output_path, "w");
    if (!output) {
        std::cerr << "Ошибка: не удалось создать " << output_path << std::endl;
        fclose(input);
        return;
    }
    
    char line[4096];
    while (fgets(line, sizeof(line), input)) {
        
        char token[256];
        char positions[4000];
        
        if (sscanf(line, "%255s %3999[^\n]", token, positions) >= 1) {
            
            const char* stem = stemmer.stem(token);
            
            
            if (stems_log && strcmp(token, stem) != 0) {
                fprintf(stems_log, "%s,%s\n", token, stem);
            }
            
            
            fprintf(output, "%s", stem);
            if (sscanf(line, "%*s %3999[^\n]", positions) == 1) {
                fprintf(output, " %s", positions);
            }
            fprintf(output, "\n");
        }
    }
    
    fclose(input);
    fclose(output);
}

void process_directory(const char* input_dir, const char* output_dir, 
                      const char* stems_csv_path) {
    std::cerr << "Обработка директории: " << input_dir << std::endl;
    std::cerr << "Выходная директория: " << output_dir << std::endl;
    
    
    mkdir(output_dir, 0755);
    
    
    FILE* stems_log = fopen(stems_csv_path, "w");
    if (stems_log) {
        fprintf(stems_log, "original,stem\n");
    }
    
    
    DIR* dir = opendir(input_dir);
    if (!dir) {
        std::cerr << "Ошибка: не удалось открыть директорию " << input_dir << std::endl;
        return;
    }
    
    RussianStemmer stemmer;
    struct dirent* entry;
    int processed = 0;
    
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        
        
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        
        
        size_t len = strlen(name);
        if (len < 7 || strcmp(name + len - 7, ".tokens") != 0) {
            continue;
        }
        
        
        char input_path[512];
        char output_path[512];
        snprintf(input_path, sizeof(input_path), "%s/%s", input_dir, name);
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, name);
        
        
        process_token_file(input_path, output_path, stems_log, stemmer);
        
        processed++;
        if (processed % 100 == 0) {
            std::cerr << "Обработано " << processed << " файлов..." << std::endl;
        }
    }
    
    closedir(dir);
    
    if (stems_log) {
        fclose(stems_log);
    }
    
    std::cerr << "Всего обработано: " << processed << " файлов" << std::endl;
    std::cerr << "Лог стемминга: " << stems_csv_path << std::endl;
}





void print_usage(const char* program_name) {
    std::cout << "Использование: " << program_name << " [опции]\n\n"
              << "Опции:\n"
              << "  --input DIR       Директория с токенами (.tokens файлы)\n"
              << "  --output DIR      Выходная директория для стеммов\n"
              << "  --stems-log FILE  Файл для логирования (по умолчанию: stems.csv)\n"
              << "  --test            Запустить тесты на примерах\n"
              << "  --help            Показать эту справку\n\n"
              << "Примеры:\n"
              << "  " << program_name << " --input tokens/ --output stems/\n"
              << "  " << program_name << " --test\n";
}

void run_tests() {
    std::cout << "=== Тесты Russian Porter Stemmer ===" << std::endl;
    std::cout << std::endl;
    
    RussianStemmer stemmer;
    
    
    const char* test_words[] = {
        "программирование", "программа", "программист",
        "обучение", "обучать", "обученный",
        "книга", "книги", "книгой",
        "большой", "большая", "большие",
        "делать", "делал", "делающий",
        "красивый", "красивая", "красиво",
        "быстро", "быстрый", "быстрее",
        "машина", "машины", "машинный",
        nullptr
    };
    
    std::cout << "Слово                  → Стемма" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    for (int i = 0; test_words[i] != nullptr; i++) {
        const char* stem = stemmer.stem(test_words[i]);
        printf("%-22s → %s\n", test_words[i], stem);
    }
}

int main(int argc, char* argv[]) {
    const char* input_dir = nullptr;
    const char* output_dir = "stems";
    const char* stems_csv = "stems.csv";
    bool run_test = false;
    
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_dir = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (strcmp(argv[i], "--stems-log") == 0 && i + 1 < argc) {
            stems_csv = argv[++i];
        } else if (strcmp(argv[i], "--test") == 0) {
            run_test = true;
        }
    }
    
    if (run_test) {
        run_tests();
        return 0;
    }
    
    if (!input_dir) {
        std::cerr << "Ошибка: необходимо указать --input" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::cerr << "======================================" << std::endl;
    std::cerr << "Russian Porter Stemmer (ЛР4)" << std::endl;
    std::cerr << "======================================" << std::endl;
    std::cerr << std::endl;
    
    process_directory(input_dir, output_dir, stems_csv);
    
    std::cerr << std::endl;
    std::cerr << "======================================" << std::endl;
    std::cerr << "Стемминг завершён" << std::endl;
    std::cerr << "======================================" << std::endl;
    
    return 0;
}