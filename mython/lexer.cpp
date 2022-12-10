#include "lexer.h"

#include <algorithm>
#include <charconv>

#include <string_view>
#include <iostream>


using namespace std;

namespace parse {
   
    void Lexer::PrepareForNewTokenReading(size_t num) {
        if (num > input_string_.size()) {
            return;
        }
        pos_ = 0;
        input_string_.remove_prefix(num);
    }

    token_type::String Lexer::LoadString(size_t pos) {
        std::string s;
        s.reserve(pos);
        for (size_t i = 1; i < pos; i++) {
            auto ch = input_string_[i];
            if (ch == '\\') {
                i++;
                if (i == pos) {
                    throw LexerError("String parsing error");
                }
                const char escaped_char = input_string_[i];
                switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                default:
                    throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
                }
            }
            else if (ch == '\n' || ch == '\r') {
                throw LexerError("Unexpected end of line"s);
            }
            else {
                s.push_back(ch);
            }
        }
        return token_type::String{s};
    }

    ANSWER Lexer::IsString() {
        
        if ((input_string_[0] == '"') || (input_string_[0] == '\'')) {
            if (pos_ == 0) {
                return MAYBE;
            }
            if (((input_string_[0] == '"')&& (input_string_[pos_] == '"') && (input_string_[pos_ - 1] != '\\')) ||
                (((input_string_[0] == '\'') && input_string_[pos_] == '\'') && (input_string_[pos_ - 1] != '\\'))) {

                current_token_ = std::move(LoadString(pos_));
                PrepareForNewTokenReading(pos_+1);
              
                return current_token_;
            }
            return MAYBE;
        }
        return {};
    }

    ANSWER Lexer::IsNumber() {

        parse::token_type::Number output;

        if (std::isdigit(input_string_[pos_])) {
            if (pos_ == input_string_.size() - 1) {
                output.value = stoi(string(input_string_.substr(0, pos_ + 1)));
                PrepareForNewTokenReading(pos_ + 1);
                current_token_ = std::move(output);
                return current_token_;
            }
            return MAYBE;
        }
        else if (pos_ != 0) {
            output.value = stoi(string(input_string_.substr(0, pos_)));
            PrepareForNewTokenReading(pos_);
            current_token_ = std::move(output);
            return current_token_;

        }
        return {};
    }

    ANSWER Lexer::IsId() {
        auto ch = input_string_[pos_];
        if ((std::isdigit(ch) && (pos_ != 0))||  std::isalpha(ch) || (ch =='_')) {
                if ((pos_ != input_string_.size()-1) &&
                    (std::isdigit(input_string_[pos_+1])||
                     std::isalpha(input_string_[pos_+1])||
                     (input_string_[pos_ + 1] == '_'))) {
                    return MAYBE;
                }
        }
        else if (pos_ == 0) {
            return {};
        }
        token_type::Id output;
        output.value = string(input_string_.substr(0, pos_+1));
        PrepareForNewTokenReading(pos_+1);
        current_token_ = output;
        return current_token_;
    }

    ANSWER Lexer::IsDoubleOperation() {
        if (pos_ == 0) {
            if ((input_string_[pos_] == '<') || (input_string_[pos_] == '>') || (input_string_[pos_] == '!') || (input_string_[pos_] == '=')) {
                return MAYBE;
            }
        }
        else if (input_string_[pos_] == '=') { //<= >= != ==
            if (input_string_[pos_ - 1] == '<') {
                current_token_ = token_type::LessOrEq();
            }

            else if (input_string_[pos_ - 1] == '>') {
                current_token_ = token_type::GreaterOrEq();
            }

            else if (input_string_[pos_ - 1] == '!') {
                current_token_ = token_type::NotEq();
            }
            else {
                current_token_ = token_type::Eq();
            }
            PrepareForNewTokenReading(pos_ + 1);
            return current_token_;
        }

        return {};
    }

    ANSWER Lexer::IsChar() {

        if (((input_string_[pos_] == '+') || (input_string_[pos_] == '-') || (input_string_[pos_] == ':') || 
            (input_string_[pos_] == '(') ||  (input_string_[pos_] == '*') || (input_string_[pos_] == '/') ||
            (input_string_[pos_] == ',') || (input_string_[pos_] == '.') || (input_string_[pos_] == ')')) ||
            (((input_string_[pos_] == '<') || (input_string_[pos_] == '>') || (input_string_[pos_] == '=')) &&
            ((pos_ == input_string_.size() - 1) || (input_string_[pos_ + 1] != '=')))) { // + - * /

                token_type::Char output;
                output.value = input_string_[pos_];
                current_token_ = output;
                PrepareForNewTokenReading(pos_ + 1);
            return current_token_;
        }
        return {};
    }

    ANSWER Lexer::IsComment() {
        if (input_string_[pos_] == '#') {
            PrepareForNewTokenReading(input_string_.size());
            return COMMENT;
        }
        return{};
    }

    bool Lexer::IsIndent() {
        if ((input_string_[pos_] == ' ') && (input_string_[pos_ + 1] == ' ')) {
             PrepareForNewTokenReading(2);
             return true;
         }
       return false;
    }
  
    ANSWER Lexer::IndentControl() {

        if (pos_ >= input_string_.size()) {
            return token_type::Dedent{};
        }

        while (IsIndent()) {
            if (++ident_control_.curr_ > ident_control_.last_) {
                return token_type::Indent{};
            }
        }

        if (ident_control_.curr_ < ident_control_.last_) {
            ident_control_.last_--;
            return token_type::Dedent{};
        }
        
        ident_control_.last_ = ident_control_.curr_;

        return {};
    }

    ANSWER Lexer::IsEof() {

        if (input_stream_.eof()){
               current_token_ = token_type::Eof();
               if (ident_control_.last_ == 0) {
                   return current_token_;
               }
               else {
                   ident_control_.last_--;
                   return token_type::Dedent{};
               }
        }
        return {};
    }

    ANSWER Lexer::IsNewline() {
        if (input_string_.empty()) {
            current_token_ = token_type::Newline();
            return current_token_;
        }
        return {};
    }

    void Lexer::ReadNextString() {
        while (!input_stream_.eof() && (/*input_string_.empty() || */
            (find_if_not(input_string_.begin(), input_string_.end(),
                         [](const auto& ch) {return (ch == ' ');})==input_string_.end()))) {
            getline(input_stream_, in_str_);
            input_string_ = in_str_;
        }

       if (input_string_.empty() && input_stream_.eof()) {
            current_token_ = token_type::Eof{};
        }
        ident_control_.curr_ = 0;

   }
    
    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input)
        : input_stream_(input) {

        LEXEMS_LIST = { &Lexer::IsKeyword<0>, 
                        &Lexer::IsKeyword<1>, &Lexer::IsKeyword<2>, &Lexer::IsKeyword<3>, &Lexer::IsKeyword<4>, 
                        &Lexer::IsKeyword<5>, &Lexer::IsKeyword<6>, &Lexer::IsKeyword<7>, &Lexer::IsKeyword<8>, 
                        &Lexer::IsKeyword<9>, &Lexer::IsKeyword<10>, &Lexer::IsKeyword<11>, &Lexer::IsString, &Lexer::IsNumber,
                        &Lexer::IsId, &Lexer::IsDoubleOperation, &Lexer::IsChar, &Lexer::IsComment };
        
        KEYWORDS_NAMES_.reserve(name_to_lexem.size());

        for (auto& [name, _] : name_to_lexem) {
            KEYWORDS_NAMES_.push_back(name);
        }

        ReadNextString();

        NextToken();
        if (current_token_ == token_type::Newline{}) {
            NextToken();
        }
    }

    const Token& Lexer::CurrentToken() const {
        return current_token_;
    }

    Token Lexer::NextToken() {

        if (current_token_ == token_type::Eof{}) {
            if (ident_control_.last_ == 0) {
                return current_token_;
            }
            else {
                return token_type::Dedent{};
            }
        }

        if ((current_token_ != token_type::Newline{}) && IsNewline().has_value()) {
            return token_type::Newline{};
        }
        
        if (current_token_ == token_type::Newline{}) {
            ReadNextString();
            auto temp = IsEof();
            if (temp.has_value()) {
                return get<Token>(temp.value());
            }
        }

        if ((current_token_ == token_type::Newline{})||
            (ident_control_.curr_ != ident_control_.last_)) {
                ANSWER ident = IndentControl();
                if (ident.has_value()) {
                    current_token_ = get<Token>(ident.value());
                    return current_token_;
                }
        }

        std::vector<list<ANSWER(Lexer::*)()>::iterator> for_erase_lexems;
        for_erase_lexems.reserve(LEXEMS_LIST.size());
        auto  lexem_list = LEXEMS_LIST;

        while (pos_!= input_string_.size()) {

            for (auto it = lexem_list.begin(); it != lexem_list.end(); ++it) {
                ANSWER temp = (this->*(*it))();
                if (temp.has_value()) {
                    if (temp.value().index() == 0){
                            PrepareForNewTokenReading(input_string_.find_first_not_of(' '));                 
                        return std::get<Token>(temp.value());
                    }
                    else if ((temp.value().index() == 1) && (get<STATE>(temp.value())==STATE::COMMENT)){
                        current_token_ = token_type::Newline{};
                        return token_type::Newline{};
                    }
                }
                else {
                    for_erase_lexems.push_back(it);
                }
            }
            for (auto to_erase : for_erase_lexems) {
                lexem_list.erase(to_erase);
            }
            for_erase_lexems.clear();
            ++pos_;
        }   
        return{};
    } 
} // namespace parse