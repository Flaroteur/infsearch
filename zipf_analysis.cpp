#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

struct TokenFreq {
    std::string token;
    size_t frequency;
    size_t rank;
};

class ZipfAnalyzer {
private:
    std::unordered_map<std::string, size_t> freq_map;
    std::vector<TokenFreq> ranked_tokens;
    
public:
    
    void process_directory(const std::string& dir_path) {
        auto start_time = std::chrono::high_resolution_clock::now();
        size_t file_count = 0;
        
        if (!fs::exists(dir_path)) {
            throw std::runtime_error("Директория не найдена: " + dir_path);
        }
        
        
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tokens") {
                process_file(entry.path());
                file_count++;
                
                if (file_count % 1000 == 0) {
                    std::cerr << "Обработано " << file_count << " файлов...\n";
                }
            }
        }
        
        if (file_count == 0) {
            throw std::runtime_error("В директории не найдены .tokens файлы");
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cerr << "Всего обработано: " << file_count << " файлов\n";
        std::cerr << "Уникальных токенов: " << freq_map.size() << "\n";
        std::cerr << "Время: " << duration.count() / 1000.0 << " сек.\n";
    }
    
    
    void process_file(const fs::path& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Предупреждение: не удалось открыть " << file_path << "\n";
            return;
        }
        
        std::string line;
        line.reserve(1024); 
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            
            size_t first_space = line.find(' ');
            if (first_space == std::string::npos) {
                
                continue;
            }
            
            
            std::string token(line.begin(), line.begin() + first_space);
            
            
            size_t freq = 0;
            size_t pos = first_space;
            while (pos != std::string::npos) {
                pos = line.find(' ', pos + 1);
                freq++;
            }
            
            if (freq > 0) {
                freq_map[token] += freq;
            }
        }
    }
    
    
    void rank_tokens() {
        ranked_tokens.reserve(freq_map.size());
        
        
        for (const auto& [token, freq] : freq_map) {
            ranked_tokens.push_back({token, freq, 0}); 
        }
        
        
        std::sort(ranked_tokens.begin(), ranked_tokens.end(),
            [](const TokenFreq& a, const TokenFreq& b) {
                if (a.frequency != b.frequency) {
                    return a.frequency > b.frequency;
                }
                return a.token < b.token;
            });
        
        
        for (size_t i = 0; i < ranked_tokens.size(); ++i) {
            ranked_tokens[i].rank = i + 1;
        }
        
        
        freq_map.clear();
    }
    
    
    void save_csv(const std::string& output_path) {
        std::ofstream csv(output_path);
        if (!csv.is_open()) {
            throw std::runtime_error("Не удалось создать CSV файл: " + output_path);
        }
        
        csv << "rank,token,frequency\n";
        for (const auto& item : ranked_tokens) {
            csv << item.rank << "," << item.token << "," << item.frequency << "\n";
        }
        
        std::cerr << "CSV сохранен: " << output_path << " (" << ranked_tokens.size() << " записей)\n";
    }
    
    
    struct RegressionResult {
        double slope;
        double intercept;
        double r_squared;
    };
    
    RegressionResult calculate_regression(size_t top_n = 0) {
        if (top_n == 0 || top_n > ranked_tokens.size()) {
            top_n = ranked_tokens.size();
        }
        
        
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
        
        for (size_t i = 0; i < top_n; ++i) {
            double x = std::log(ranked_tokens[i].rank);
            double y = std::log(ranked_tokens[i].frequency);
            
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
            sum_y2 += y * y;
        }
        
        
        double n = static_cast<double>(top_n);
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
        double intercept = (sum_y - slope * sum_x) / n;
        
        
        double mean_y = sum_y / n;
        double ss_tot = 0.0, ss_res = 0.0;
        
        for (size_t i = 0; i < top_n; ++i) {
            double x = std::log(ranked_tokens[i].rank);
            double y = std::log(ranked_tokens[i].frequency);
            double y_pred = slope * x + intercept;
            
            ss_tot += (y - mean_y) * (y - mean_y);
            ss_res += (y - y_pred) * (y - y_pred);
        }
        
        double r_squared = 1.0 - (ss_res / ss_tot);
        
        return {slope, intercept, r_squared};
    }
    
    
    void save_plot_data(const std::string& output_path, size_t top_n = 1000) {
        std::ofstream plot(output_path);
        if (!plot.is_open()) {
            throw std::runtime_error("Не удалось создать plot файл: " + output_path);
        }
        
        size_t limit = std::min(top_n, ranked_tokens.size());
        plot << "# rank\tfrequency\ttoken\n";
        for (size_t i = 0; i < limit; ++i) {
            plot << ranked_tokens[i].rank << "\t" 
                 << ranked_tokens[i].frequency << "\t"
                 << ranked_tokens[i].token << "\n";
        }
        
        std::cerr << "Данные для графика сохранены: " << output_path << "\n";
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  --tokens-dir DIR      Директория с .tokens файлами (по умолчанию: tokens)\n"
              << "  --output-csv FILE     Выходной CSV файл (по умолчанию: zipf_data.csv)\n"
              << "  --output-plot FILE    Файл с данными для графика (опционально)\n"
              << "  --top-n N             Количество топ-токенов для регрессии (по умолчанию: все)\n"
              << "  --plot-points N       Количество точек для plot файла (по умолчанию: 1000)\n"
              << "  --help                Показать эту справку\n\n"
              << "Примеры:\n"
              << "  " << program_name << "\n"
              << "  " << program_name << " --tokens-dir corpus_tokens --output-csv results.csv --output-plot data.dat\n";
}

int main(int argc, char* argv[]) {
    std::string tokens_dir = "tokens";
    std::string output_csv = "zipf_data.csv";
    std::string output_plot;
    size_t top_n = 0; 
    size_t plot_points = 1000;
    
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--tokens-dir" && i + 1 < argc) {
            tokens_dir = argv[++i];
        } else if (arg == "--output-csv" && i + 1 < argc) {
            output_csv = argv[++i];
        } else if (arg == "--output-plot" && i + 1 < argc) {
            output_plot = argv[++i];
        } else if (arg == "--top-n" && i + 1 < argc) {
            top_n = std::stoul(argv[++i]);
        } else if (arg == "--plot-points" && i + 1 < argc) {
            plot_points = std::stoul(argv[++i]);
        } else {
            std::cerr << "Неизвестный параметр: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    try {
        std::cerr << "=== Анализ закона Ципфа ===\n";
        
        ZipfAnalyzer analyzer;
        
        
        std::cerr << "Обработка директории: " << tokens_dir << "\n";
        analyzer.process_directory(tokens_dir);
        
        
        std::cerr << "Ранжирование токенов...\n";
        analyzer.rank_tokens();
        
        
        analyzer.save_csv(output_csv);
        
        
        std::cerr << "Расчет линейной регрессии...\n";
        auto regression = analyzer.calculate_regression(top_n);
        
        std::cerr << "\n=== Результаты регрессии ===\n";
        std::cerr << "Slope (параметр s): " << std::fixed << std::setprecision(4) 
                  << regression.slope << "\n";
        std::cerr << "Intercept: " << regression.intercept << "\n";
        std::cerr << "R²: " << regression.r_squared << "\n";
        std::cerr << "Теоретическое значение: ~ -1.0\n";
        
        
        if (!output_plot.empty()) {
            analyzer.save_plot_data(output_plot, plot_points);
        }
        
        std::cerr << "\nАнализ завершен успешно!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}