#include "../core/starter.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <set>
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else   
    #define EXPORT __attribute__((visibility("default")))
#endif

extern "C" struct EXPORT Pnix {
    // Helper function to check if file exists

    
    std::map<std::string, std::string> parse_arguments(const std::string& args_str) {
        std::map<std::string, std::string> args;
        
        // Remove outer parentheses
        std::string clean_args = args_str;
        if (!clean_args.empty() && clean_args.front() == '(') {
            clean_args = clean_args.substr(1);
        }
        if (!clean_args.empty() && clean_args.back() == ')') {
            clean_args.pop_back();
        }
        
        // Parse key="value" pairs
       std::regex arg_pattern("(\\w+)\\s*=\\s*\"([^\"]*)\"");
        std::smatch match;
        std::string::const_iterator searchStart(clean_args.cbegin());
        
        while (std::regex_search(searchStart, clean_args.cend(), match, arg_pattern)) {
            if (match.size() > 2) {
                std::string key = match[1].str();
                std::string value = match[2].str();
                args[key] = value;
            }
            searchStart = match.suffix().first;
        }
        
        return args;
    }

    std::string replace_placeholders(const std::string& content, const std::map<std::string, std::string>& args) {
        std::string result = content;
        
        for (const auto& pair : args) {
            std::string placeholder = "{" + pair.first + "}";
            size_t pos = 0;
            while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                result.replace(pos, placeholder.length(), pair.second);
                pos += pair.second.length();
            }
        }
        
        return result;
    }

    struct TagInfo {
        std::string name;
        std::map<std::string, std::string> arguments;
        std::string full_match;
    };

    std::vector<TagInfo> extract_tags_with_args(const std::string& content) {
        std::vector<TagInfo> tags;
        
        std::istringstream stream(content);
        std::string line;
        
        while (std::getline(stream, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
            
            // Check if line starts with @
            if (trimmed.length() > 1 && trimmed[0] == '@') {
                TagInfo tag_info;
                
                // Find the tag name (word characters after @)
                size_t tag_start = 1;
                size_t tag_end = tag_start;
                while (tag_end < trimmed.length() && 
                       (std::isalnum(trimmed[tag_end]) || trimmed[tag_end] == '_')) {
                    tag_end++;
                }
                
                if (tag_end > tag_start) {
                    tag_info.name = trimmed.substr(tag_start, tag_end - tag_start);
                    tag_info.full_match = trimmed;
                    
                    // Check for arguments
                    if (tag_end < trimmed.length() && trimmed[tag_end] == '(') {
                        size_t paren_end = trimmed.find(')', tag_end);
                        if (paren_end != std::string::npos) {
                            std::string args_str = trimmed.substr(tag_end, paren_end - tag_end + 1);
                            tag_info.arguments = parse_arguments(args_str);
                        }
                    }
                    
                    tags.push_back(tag_info);
                }
            }
        }

        return tags;
    }

    std::vector<std::string> extract_tags_from_content(const std::string& content) {
        std::set<std::string> unique_tags;
        auto tag_infos = extract_tags_with_args(content);
        
        for (const auto& tag_info : tag_infos) {
            unique_tags.insert(tag_info.name);
        }

        return std::vector<std::string>(unique_tags.begin(), unique_tags.end());
    }

    std::string copy_block_with_args(const std::string& layout_path, const std::string& tag, const std::map<std::string, std::string>& override_args = {}) {
        // Remove leading '@' if present
        std::string clean_tag = tag;
        if (!clean_tag.empty() && clean_tag[0] == '@') {
            clean_tag = clean_tag.substr(1);
        }

        std::ifstream file(layout_path);
        if (!file.is_open()) {
            std::cerr << "ERROR: Cannot open layout file: " << layout_path << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Parse line by line to find the block
        std::istringstream stream(content);
        std::string line;
        std::ostringstream block_content;
        bool in_block = false;
        std::string end_tag = "@" + clean_tag + ";";
        std::map<std::string, std::string> layout_args;
        
        while (std::getline(stream, line)) {
            std::string trimmed = line;
            // Trim whitespace
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
            
            // Check if this line starts with our tag (with or without arguments)
            if (!in_block) {
                std::string tag_prefix = "@" + clean_tag;
                if (trimmed.substr(0, tag_prefix.length()) == tag_prefix) {
                    in_block = true;
                    
                    // Extract arguments if present
                    size_t paren_start = trimmed.find('(');
                    size_t paren_end = trimmed.find(')', paren_start);
                    if (paren_start != std::string::npos && paren_end != std::string::npos) {
                        std::string args_str = trimmed.substr(paren_start, paren_end - paren_start + 1);
                        layout_args = parse_arguments(args_str);
                    }
                    
                    // Check if there's content after the tag on the same line
                    size_t content_start = trimmed.find(' ', tag_prefix.length());
                    if (paren_end != std::string::npos) {
                        content_start = trimmed.find_first_not_of(" \t", paren_end + 1);
                    } else {
                        content_start = trimmed.find_first_not_of(" \t", tag_prefix.length());
                    }
                    
                    if (content_start != std::string::npos && content_start < trimmed.length()) {
                        block_content << trimmed.substr(content_start) << "\n";
                    }
                    continue;
                }
            }
            
            if (in_block && trimmed == end_tag) {
                break;
            }
            
            if (in_block) {
                block_content << line << "\n";
            }
        }
        
        std::string result = block_content.str();
        // Remove trailing newline if present
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        
        // Merge layout arguments with override arguments (override takes precedence)
        std::map<std::string, std::string> final_args = layout_args;
        for (const auto& pair : override_args) {
            final_args[pair.first] = pair.second;
        }
        
        // Replace placeholders with final arguments
        result = replace_placeholders(result, final_args);
        
        return result;
    }

    // Keep the old function for backward compatibility
    std::string copy_block(const std::string& layout_path, const std::string& tag) {
        return copy_block_with_args(layout_path, tag);
    }

   void insert_block(const std::string& child_path, const std::string& tag) {
    std::ifstream file(child_path);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open child file: " << child_path << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    auto tag_infos = extract_tags_with_args(content);
    bool modified = false;

    for (const auto& tag_info : tag_infos) {
        if (tag_info.name != tag) continue;

        std::string processed_block = copy_block_with_args("public/layout.html", tag, tag_info.arguments);
        if (processed_block.empty()) {
            std::cerr << "ERROR: No layout block found for tag: " << tag << std::endl;
            continue;
        }

        // Find start of tag line (the line containing @tag(...))
        size_t start_pos = content.find(tag_info.full_match);
        if (start_pos == std::string::npos) continue;

        // Find end of start tag line (newline after start_pos)
        size_t start_line_end = content.find('\n', start_pos);
        if (start_line_end == std::string::npos) {
            // no newline, so treat end of string as line end
            start_line_end = content.length();
        } else {
            start_line_end++; // move to start of next line (block content start)
        }

        // Find closing tag line e.g. "@tag;" after start_line_end
        std::string end_tag = "@" + tag + ";";
        size_t end_tag_pos = content.find(end_tag, start_line_end);

        // If not found, try commented closing tag
        if (end_tag_pos == std::string::npos) {
            std::string comment_end_tag = "<!-- @" + tag + "; -->";
            end_tag_pos = content.find(comment_end_tag, start_line_end);
            if (end_tag_pos == std::string::npos) {
                continue;
            }
        }

        // Find end of closing tag line (include newline after closing tag line)
        size_t end_line_end = content.find('\n', end_tag_pos);
        if (end_line_end == std::string::npos) {
            end_line_end = content.length();
        } else {
            end_line_end++; // include newline
        }

        // Now replace content between start_line_end and end_tag_pos with new block content
        // Because end_tag_pos points to start of closing tag, which we don't want to replace
        content.replace(start_line_end, end_tag_pos - start_line_end, processed_block + "\n");

        modified = true;
    }

    if (modified) {
        std::ofstream out(child_path);
        if (!out.is_open()) {
            std::cerr << "ERROR: Cannot write to child file: " << child_path << std::endl;
            return;
        }

        out << content;
        out.close();
    }
}

    std::string erase_block(const std::string& filepath, const std::string& tag) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << filepath << '\n';
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        std::istringstream stream(content);
        std::ostringstream output;
        std::string line;

        while (std::getline(stream, line)) {
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n")); // Trim left
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1); // Trim right

            if (trimmed == tag || trimmed == "@end") {
                continue; // Skip the tag line and @end line
            }

            output << line << '\n';
        }

        return output.str();
    }

    std::string commentify_tags(const std::string& content, const std::vector<std::string>& tags) {
        std::istringstream stream(content);
        std::ostringstream output;
        std::string line;

        bool inside_block = false;
        std::string current_tag;

        while (std::getline(stream, line)) {
            std::string trimmed = line;
            // Trim leading/trailing whitespace
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
            trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

            if (!inside_block) {
                // Check if line starts with any of our tags
                bool found_tag = false;
                if (trimmed.length() > 1 && trimmed[0] == '@') {
                    // Extract tag name
                    size_t tag_start = 1;
                    size_t tag_end = tag_start;
                    while (tag_end < trimmed.length() && 
                           (std::isalnum(trimmed[tag_end]) || trimmed[tag_end] == '_')) {
                        tag_end++;
                    }
                    
                    if (tag_end > tag_start) {
                        std::string tag_name = trimmed.substr(tag_start, tag_end - tag_start);
                        for (const auto& tag : tags) {
                            if (tag_name == tag) {
                                current_tag = tag;
                                inside_block = true;
                                output << "<!-- " << trimmed << " -->\n";
                                found_tag = true;
                                break;
                            }
                        }
                    }
                }

                if (!found_tag) {
                    output << line << '\n';
                }
            } else {
                if (trimmed == "@" + current_tag + ";") {
                    output << "<!-- @" + current_tag + "; -->\n";
                    inside_block = false;
                    current_tag.clear();
                } else {
                    output << line << '\n';
                }
            }
        }

        // Close any unclosed block
        if (inside_block && !current_tag.empty()) {
            output << "<!-- @" << current_tag << "; -->\n";
        }

        return output.str();
    }

    void static_serve(const std::string path, struct mg_connection *connection) {
        std::ifstream file(path);
        if (!file) {
            const char *msg = "404 Not Found";
            mg_printf(connection,
                      "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: %zu\r\n\r\n%s",
                      strlen(msg), msg);
            return;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        mg_printf(connection,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: %zu\r\n\r\n%s",
                  content.length(), content.c_str());
    }

    void home(std::string path, struct mg_connection *connection) {
    
    std::ifstream file_layout("public/layout.html");
    if (!file_layout) {
        std::cerr << "> Phoenix[Info]: Error: Layout.html not found" << std::endl;
        return;
    }

    std::stringstream layout_buffer;
    layout_buffer << file_layout.rdbuf();
    std::string layout_content = layout_buffer.str();
    file_layout.close();

    // Extract tag list from layout file
    std::vector<std::string> tag_list_vect = extract_tags_from_content(layout_content);

    // Create a working copy of layout_content
    std::string final_content = layout_content;

    for (const auto& tag : tag_list_vect) {
        // Get rendered block with default args from layout.html
        std::string rendered_block = copy_block_with_args("public/layout.html", tag);

        if (rendered_block.empty()) continue;

        // Prepare to replace the layout block with the commented rendered version
        std::ostringstream commented_block;
        commented_block << "<!-- @" << tag << " (with default args) -->\n";
        
        // Comment each line of the rendered block
        std::istringstream rb_stream(rendered_block);
        std::string rb_line;
        while (std::getline(rb_stream, rb_line)) {
            commented_block << rb_line << "\n";
        }
        commented_block << "<!-- @" << tag << "; -->";

        // Use regex to replace the original block in layout.html
      std::regex block_pattern("@" + tag + R"((\([^\)]*\))?\s*\n([\s\S]*?)@)" + tag + R"(;)");
        final_content = std::regex_replace(final_content, block_pattern, commented_block.str());
    }

    mg_printf(connection,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n\r\n%s",
        final_content.length(), final_content.c_str());
}

   
    void SSR(std::string path, struct mg_connection *connection) {
        
        std::ifstream child_file(path);
        if (!child_file.is_open()) {
            const char *msg = "404 Not Found";
            mg_printf(connection,
                      "HTTP/1.1 404 Not Found\r\n"
                      "Content-Type: text/plain\r\n"
                      "Content-Length: %zu\r\n\r\n%s",
                      strlen(msg), msg);
            return;
        }

        std::stringstream child_buffer;
        child_buffer << child_file.rdbuf();
        std::string child_content = child_buffer.str();
        child_file.close();

        std::vector<std::string> tag_list_vect = extract_tags_from_content(child_content);

        for (const auto& tag : tag_list_vect) {
            insert_block(path, tag);
        }

        std::ifstream updated_file(path);
        if (!updated_file.is_open()) {
            std::cerr << "Cannot open updated child file after insertion: " << path << std::endl;
            return;
        }

        std::stringstream updated_buffer;
        updated_buffer << updated_file.rdbuf();
        std::string updated_content = updated_buffer.str();
        updated_file.close();

        updated_content = commentify_tags(updated_content, tag_list_vect);

        mg_printf(connection,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/html\r\n"
                  "Content-Length: %zu\r\n\r\n%s",
                  updated_content.length(), updated_content.c_str());
    }
};