#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <unicode/unistr.h>
#include <unicode/normalizer2.h>
#include <unicode/uchar.h>
#include <unicode/ustream.h>

using namespace std;
using namespace icu;

struct TokenizerConfig {
    bool lowercase = true;           
    bool normalize_unicode = true;   
    bool remove_punctuation = true;  
    bool keep_numbers = true;        
    bool normalize_hyphens = true;   
    bool remove_stopwords = false;   
    bool output_positions = true;    
    string stopwords_file;           
    string input_file;               
    string output_file;              
};

set<string> load_stopwords(const string& filename) {
    set<string> stopwords;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Warning: Cannot open stopwords file: " << filename << endl;
        return stopwords;
    }
    
    string word;
    while (getline(file, word)) {
        
        size_t comment_pos = word.find('#');
        if (comment_pos != string::npos) {
            word = word.substr(0, comment_pos);
        }
        
        
        word.erase(0, word.find_first_not_of(" \t\r\n"));
        word.erase(word.find_last_not_of(" \t\r\n") + 1);
        
        if (!word.empty()) {
            stopwords.insert(word);
        }
    }
    
    cerr << "Loaded " << stopwords.size() << " stopwords from " << filename << endl;
    return stopwords;
}

UnicodeString normalize_unicode(const UnicodeString& input) {
    UErrorCode status = U_ZERO_ERROR;
    const Normalizer2* nfkc = Normalizer2::getNFKCInstance(status);
    if (U_FAILURE(status)) {
        cerr << "Error: Failed to get NFKC normalizer" << endl;
        return input;
    }
    
    UnicodeString normalized = nfkc->normalize(input, status);
    if (U_FAILURE(status)) {
        cerr << "Error: NFKC normalization failed" << endl;
        return input;
    }
    
    return normalized;
}


UnicodeString to_lowercase(const UnicodeString& input) {
    UnicodeString result = input;
    result.toLower();
    return result;
}

bool is_word_char(UChar32 c) {
    return u_isalpha(c) || u_isdigit(c) || c == U'_';
}

struct Token {
    string text;
    int position;
};

vector<Token> tokenize(const UnicodeString& text, const TokenizerConfig& config, 
                       const set<string>& stopwords) {
    vector<Token> tokens;
    UnicodeString current_token;
    int position = 0;
    bool in_token = false;
    
    for (int32_t i = 0; i < text.length(); i++) {
        UChar32 c = text.char32At(i);
        
        
        if (config.normalize_hyphens) {
            
            
            if (c == 0x2010 || c == 0x2011 || c == 0x2012 || 
                c == 0x2013 || c == 0x2014 || c == 0x2015 || 
                c == 0x2212) {
                c = '-';
            }
        }
        
        bool is_word = is_word_char(c);
        bool is_hyphen = (c == '-' || c == 0x2D);
        bool is_apostrophe = (c == '\'' || c == 0x2019); 
        
        
        if (in_token && is_hyphen) {
            if (config.normalize_hyphens) {
                
                
                if (current_token.length() > 0) {
                    string token_str;
                    current_token.toUTF8String(token_str);
                    
                    
                    bool is_stopword = config.remove_stopwords && 
                                      stopwords.find(token_str) != stopwords.end();
                    
                    if (!is_stopword) {
                        tokens.push_back({token_str, position});
                    }
                    position++;
                }
                current_token.remove();
                in_token = false;
            } else {
                
                current_token.append(c);
            }
            continue;
        }
        
        
        if (in_token && is_apostrophe) {
            
            current_token.append(c);
            continue;
        }
        
        if (is_word) {
            if (!in_token) {
                in_token = true;
            }
            current_token.append(c);
        } else {
            
            if (in_token) {
                string token_str;
                current_token.toUTF8String(token_str);
                
                
                if (!config.keep_numbers) {
                    bool all_digits = true;
                    for (char ch : token_str) {
                        if (!isdigit(ch)) {
                            all_digits = false;
                            break;
                        }
                    }
                    if (all_digits) {
                        current_token.remove();
                        in_token = false;
                        position++;
                        continue;
                    }
                }
                
                
                bool is_stopword = config.remove_stopwords && 
                                  stopwords.find(token_str) != stopwords.end();
                
                if (!is_stopword) {
                    tokens.push_back({token_str, position});
                }
                
                current_token.remove();
                in_token = false;
                position++;
            }
        }
    }
    
    
    if (in_token && current_token.length() > 0) {
        string token_str;
        current_token.toUTF8String(token_str);
        
        bool is_stopword = config.remove_stopwords && 
                          stopwords.find(token_str) != stopwords.end();
        
        if (!is_stopword) {
            tokens.push_back({token_str, position});
        }
    }
    
    return tokens;
}





void process_document(const string& input_text, const TokenizerConfig& config,
                     const set<string>& stopwords, ostream& output) {
    
    UnicodeString utext = UnicodeString::fromUTF8(StringPiece(input_text));
    
    
    if (config.normalize_unicode) {
        utext = normalize_unicode(utext);
    }
    
    
    if (config.lowercase) {
        utext = to_lowercase(utext);
    }
    
    
    vector<Token> tokens = tokenize(utext, config, stopwords);
    
    
    map<string, vector<int>> token_positions;
    for (const auto& token : tokens) {
        token_positions[token.text].push_back(token.position);
    }
    
    
    for (const auto& entry : token_positions) {
        output << entry.first;
        if (config.output_positions) {
            for (int pos : entry.second) {
                output << " " << pos;
            }
        }
        output << "\n";
    }
}





void process_file(const TokenizerConfig& config, const set<string>& stopwords) {
    
    ifstream input_file(config.input_file);
    if (!input_file.is_open()) {
        cerr << "Error: Cannot open input file: " << config.input_file << endl;
        exit(1);
    }
    
    
    ostream* output;
    ofstream output_file;
    
    if (config.output_file == "-" || config.output_file.empty()) {
        output = &cout;
    } else {
        output_file.open(config.output_file);
        if (!output_file.is_open()) {
            cerr << "Error: Cannot open output file: " << config.output_file << endl;
            exit(1);
        }
        output = &output_file;
    }
    
    
    stringstream buffer;
    buffer << input_file.rdbuf();
    string text = buffer.str();
    
    
    process_document(text, config, stopwords, *output);
    
    
    input_file.close();
    if (output_file.is_open()) {
        output_file.close();
    }
}


void process_corpus(const string& corpus_dir, const string& output_dir,
                   const TokenizerConfig& config, const set<string>& stopwords) {
    cerr << "Processing corpus from: " << corpus_dir << endl;
    cerr << "Output directory: " << output_dir << endl;
    
    
    system(("mkdir -p " + output_dir).c_str());
    
    
    string cmd = "ls -1 " + corpus_dir + "/doc*.txt 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        cerr << "Error: Cannot list corpus files" << endl;
        return;
    }
    
    char buffer[256];
    int processed = 0;
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        string filepath = buffer;
        
        filepath.erase(filepath.find_last_not_of("\n\r") + 1);
        
        
        size_t last_slash = filepath.find_last_of('/');
        string filename = (last_slash != string::npos) ? 
                         filepath.substr(last_slash + 1) : filepath;
        
        
        size_t dot_pos = filename.find_last_of('.');
        string output_filename = (dot_pos != string::npos) ?
                                filename.substr(0, dot_pos) + ".tokens" : 
                                filename + ".tokens";
        
        string output_path = output_dir + "/" + output_filename;
        
        
        TokenizerConfig file_config = config;
        file_config.input_file = filepath;
        file_config.output_file = output_path;
        
        process_file(file_config, stopwords);
        
        processed++;
        if (processed % 100 == 0) {
            cerr << "Processed " << processed << " documents..." << endl;
        }
    }
    
    pclose(pipe);
    cerr << "Total processed: " << processed << " documents" << endl;
}





void print_usage(const char* program_name) {
    cout << "Usage: " << program_name << " [options]\n\n"
         << "Options:\n"
         << "  --input FILE          Input file (required for single file mode)\n"
         << "  --output FILE         Output file (- for stdout)\n"
         << "  --corpus-dir DIR      Process entire corpus directory\n"
         << "  --output-dir DIR      Output directory for corpus mode\n"
         << "  --stopwords FILE      Stopwords file\n"
         << "  --no-lowercase        Do not convert to lowercase\n"
         << "  --no-normalize        Do not apply NFKC normalization\n"
         << "  --keep-punctuation    Keep punctuation\n"
         << "  --remove-numbers      Remove numbers\n"
         << "  --no-normalize-hyphens Keep hyphens in tokens\n"
         << "  --remove-stopwords    Remove stopwords (requires --stopwords)\n"
         << "  --no-positions        Do not output token positions\n"
         << "  --help                Show this help\n\n"
         << "Examples:\n"
         << "  # Single file\n"
         << "  " << program_name << " --input doc.txt --output tokens.txt\n\n"
         << "  # Corpus processing\n"
         << "  " << program_name << " --corpus-dir corpus/ --output-dir tokens/\n\n"
         << "  # With stopwords\n"
         << "  " << program_name << " --input doc.txt --stopwords stopwords.txt --remove-stopwords\n";
}

int main(int argc, char* argv[]) {
    TokenizerConfig config;
    string corpus_dir;
    string output_dir = "tokens";
    
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--input" && i + 1 < argc) {
            config.input_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if (arg == "--corpus-dir" && i + 1 < argc) {
            corpus_dir = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--stopwords" && i + 1 < argc) {
            config.stopwords_file = argv[++i];
        } else if (arg == "--no-lowercase") {
            config.lowercase = false;
        } else if (arg == "--no-normalize") {
            config.normalize_unicode = false;
        } else if (arg == "--keep-punctuation") {
            config.remove_punctuation = false;
        } else if (arg == "--remove-numbers") {
            config.keep_numbers = false;
        } else if (arg == "--no-normalize-hyphens") {
            config.normalize_hyphens = false;
        } else if (arg == "--remove-stopwords") {
            config.remove_stopwords = true;
        } else if (arg == "--no-positions") {
            config.output_positions = false;
        } else {
            cerr << "Unknown option: " << arg << endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    
    set<string> stopwords;
    if (!config.stopwords_file.empty()) {
        stopwords = load_stopwords(config.stopwords_file);
    }
    
    
    if (!corpus_dir.empty()) {
        
        process_corpus(corpus_dir, output_dir, config, stopwords);
    } else if (!config.input_file.empty()) {
        
        process_file(config, stopwords);
    } else {
        cerr << "Error: Either --input or --corpus-dir must be specified" << endl;
        print_usage(argv[0]);
        return 1;
    }
    
    return 0;
}