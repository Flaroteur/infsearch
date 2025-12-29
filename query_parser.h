#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

#include <cstring>
#include <cctype>

enum TokenType {
    TOKEN_TERM,      
    TOKEN_AND,       
    TOKEN_OR,        
    TOKEN_NOT,       
    TOKEN_LPAREN,    
    TOKEN_RPAREN,    
    TOKEN_EOF        
};


struct Token {
    TokenType type;
    char text[256];
    
    Token() : type(TOKEN_EOF) {
        text[0] = '\0';
    }
    
    Token(TokenType t, const char* txt = "") : type(t) {
        strncpy(text, txt, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
    }
};


class Lexer {
private:
    const char* input;
    size_t pos;
    
    void skip_whitespace() {
        while (input[pos] && isspace(input[pos])) {
            pos++;
        }
    }
    
	bool is_term_char(char c) {
		unsigned char uc = static_cast<unsigned char>(c);
		
		
		if ((uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z') || 
			(uc >= '0' && uc <= '9')) {
			return true;
		}
		
		
		if (c == '_' || c == '-' || c == '\'') {
			return true;
		}
		
		
		if (uc >= 0x80) {
			return true;
		}
		
		return false;
	}
    
public:
    Lexer(const char* query) : input(query), pos(0) {}
    
    Token next_token() {
        skip_whitespace();
        
        if (!input[pos]) {
            return Token(TOKEN_EOF);
        }
        
        
        if (input[pos] == '(') {
            pos++;
            return Token(TOKEN_LPAREN);
        }
        
        if (input[pos] == ')') {
            pos++;
            return Token(TOKEN_RPAREN);
        }
        
        
        char word[256];
        size_t word_len = 0;
        
        while (input[pos] && is_term_char(input[pos]) && word_len < 255) {
            word[word_len++] = tolower(input[pos]);
            pos++;
        }
        word[word_len] = '\0';
        
        if (word_len == 0) {
            
            pos++;
            return next_token();
        }
        
        
        if (strcmp(word, "and") == 0 || strcmp(word, "&&") == 0) {
            return Token(TOKEN_AND);
        }
        
        if (strcmp(word, "or") == 0 || strcmp(word, "||") == 0) {
            return Token(TOKEN_OR);
        }
        
        if (strcmp(word, "not") == 0 || strcmp(word, "!") == 0) {
            return Token(TOKEN_NOT);
        }
        
        
        return Token(TOKEN_TERM, word);
    }
    
    void reset() {
        pos = 0;
    }
};


enum NodeType {
    NODE_TERM,
    NODE_AND,
    NODE_OR,
    NODE_NOT
};

struct ASTNode {
    NodeType type;
    char term[256];
    ASTNode* left;
    ASTNode* right;
    
    ASTNode() : type(NODE_TERM), left(nullptr), right(nullptr) {
        term[0] = '\0';
    }
    
    ASTNode(NodeType t) : type(t), left(nullptr), right(nullptr) {
        term[0] = '\0';
    }
    
    ASTNode(const char* t) : type(NODE_TERM), left(nullptr), right(nullptr) {
        strncpy(term, t, sizeof(term) - 1);
        term[sizeof(term) - 1] = '\0';
    }
    
    ~ASTNode() {
        delete left;
        delete right;
    }
};

class QueryParser {
private:
    Lexer* lexer;
    Token current_token;
    
    void advance() {
        current_token = lexer->next_token();
    }
    
    bool match(TokenType type) {
        return current_token.type == type;
    }
    
    void expect(TokenType type) {
        if (!match(type)) {
            
        }
        advance();
    }
    
    ASTNode* parse_primary() {
        if (match(TOKEN_TERM)) {
            ASTNode* node = new ASTNode(current_token.text);
            advance();
            return node;
        }
        
        if (match(TOKEN_LPAREN)) {
            advance();
            ASTNode* node = parse_expression();
            expect(TOKEN_RPAREN);
            return node;
        }
        
        
        return nullptr;
    }
    
    ASTNode* parse_not_expr() {
        if (match(TOKEN_NOT)) {
            advance();
            ASTNode* node = new ASTNode(NODE_NOT);
            node->left = parse_not_expr();
            return node;
        }
        
        return parse_primary();
    }
    
    ASTNode* parse_and_expr() {
        ASTNode* left = parse_not_expr();
        
        while (match(TOKEN_AND)) {
            advance();
            ASTNode* node = new ASTNode(NODE_AND);
            node->left = left;
            node->right = parse_not_expr();
            left = node;
        }
        
        return left;
    }
    
    ASTNode* parse_or_expr() {
        ASTNode* left = parse_and_expr();
        
        while (match(TOKEN_OR)) {
            advance();
            ASTNode* node = new ASTNode(NODE_OR);
            node->left = left;
            node->right = parse_and_expr();
            left = node;
        }
        
        return left;
    }
    
    ASTNode* parse_expression() {
        return parse_or_expr();
    }
    
public:
    QueryParser(Lexer* lex) : lexer(lex) {}
    
    ASTNode* parse() {
        advance();
        return parse_expression();
    }
};


void print_ast(ASTNode* node, int depth = 0) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    
    switch (node->type) {
        case NODE_TERM:
            printf("TERM: %s\n", node->term);
            break;
        case NODE_AND:
            printf("AND\n");
            print_ast(node->left, depth + 1);
            print_ast(node->right, depth + 1);
            break;
        case NODE_OR:
            printf("OR\n");
            print_ast(node->left, depth + 1);
            print_ast(node->right, depth + 1);
            break;
        case NODE_NOT:
            printf("NOT\n");
            print_ast(node->left, depth + 1);
            break;
    }
}

#endif 