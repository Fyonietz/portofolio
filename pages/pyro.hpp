#include "../core/starter.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C" struct EXPORT Pnix {

  // Cache for layout blocks (tag -> block content and args)
  std::unordered_map<std::string,
                     std::pair<std::string, std::map<std::string, std::string>>>
      layout_cache;
  std::mutex cache_mutex;
  
  int Response(struct mg_connection* conn, int status_code, const std::string& status_text, const std::string& json_body) {
    mg_printf(conn,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        status_code,
        status_text.c_str(),
        json_body.length(),
        json_body.c_str()
    );

  return status_code;
}
  std::string Read(struct mg_connection *connection) {
    std::string body;
    char buffer[2048];
    int bytes_read;

    const char *content_length_str =
        mg_get_header(connection, "Content-Length");
    if (content_length_str) {
      size_t content_length = std::stoul(content_length_str);
      body.reserve(content_length);
    }

    while ((bytes_read = mg_read(connection, buffer, sizeof(buffer))) > 0) {
      body.append(buffer, bytes_read);
    }

    return body;
  }

  std::map<std::string, std::string>
  parse_arguments(const std::string &args_str) {
    std::map<std::string, std::string> args;
    std::string clean_args = args_str;
    if (!clean_args.empty() && clean_args.front() == '(') {
      clean_args = clean_args.substr(1);
    }
    if (!clean_args.empty() && clean_args.back() == ')') {
      clean_args.pop_back();
    }

    // Simple manual parsing (faster than regex)
    size_t pos = 0;
    while (pos < clean_args.length()) {
      // Find key
      size_t key_start = clean_args.find_first_not_of(" \t", pos);
      if (key_start == std::string::npos)
        break;
      size_t key_end = clean_args.find('=', key_start);
      if (key_end == std::string::npos)
        break;

      std::string key = clean_args.substr(key_start, key_end - key_start);
      key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());

      // Find value
      size_t val_start = clean_args.find('"', key_end);
      if (val_start == std::string::npos)
        break;
      size_t val_end = clean_args.find('"', val_start + 1);
      if (val_end == std::string::npos)
        break;

      std::string value =
          clean_args.substr(val_start + 1, val_end - val_start - 1);

      args[key] = value;

      pos = val_end + 1;
      // Skip comma or whitespace
      while (pos < clean_args.length() &&
             (clean_args[pos] == ',' || isspace(clean_args[pos])))
        pos++;
    }

    return args;
  }

  std::string
  replace_placeholders(const std::string &content,
                       const std::map<std::string, std::string> &args) {
    std::string result = content;
    for (const auto &pair : args) {
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

  std::vector<TagInfo> extract_tags_with_args(const std::string &content) {
    std::vector<TagInfo> tags;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
      std::string trimmed = line;
      trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
      trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

      if (trimmed.length() > 1 && trimmed[0] == '@') {
        TagInfo tag_info;
        size_t tag_start = 1;
        size_t tag_end = tag_start;
        while (tag_end < trimmed.length() &&
               (std::isalnum(trimmed[tag_end]) || trimmed[tag_end] == '_')) {
          tag_end++;
        }

        if (tag_end > tag_start) {
          tag_info.name = trimmed.substr(tag_start, tag_end - tag_start);
          tag_info.full_match = trimmed;

          if (tag_end < trimmed.length() && trimmed[tag_end] == '(') {
            size_t paren_end = trimmed.find(')', tag_end);
            if (paren_end != std::string::npos) {
              std::string args_str =
                  trimmed.substr(tag_end, paren_end - tag_end + 1);
              tag_info.arguments = parse_arguments(args_str);
            }
          }
          tags.push_back(tag_info);
        }
      }
    }
    return tags;
  }

  std::vector<std::string>
  extract_tags_from_content(const std::string &content) {
    std::set<std::string> unique_tags;
    auto tag_infos = extract_tags_with_args(content);
    for (const auto &tag_info : tag_infos) {
      unique_tags.insert(tag_info.name);
    }
    return std::vector<std::string>(unique_tags.begin(), unique_tags.end());
  }

  // Cache-aware version of copy_block_with_args
  std::string copy_block_with_args(
      const std::string &layout_path, const std::string &tag,
      const std::map<std::string, std::string> &override_args = {}) {
    std::string clean_tag = tag;
    if (!clean_tag.empty() && clean_tag[0] == '@') {
      clean_tag = clean_tag.substr(1);
    }

    {
      // Check cache first
      std::lock_guard<std::mutex> lock(cache_mutex);
      auto it = layout_cache.find(clean_tag);
      if (it != layout_cache.end()) {
        std::string cached_block = it->second.first;
        auto cached_args = it->second.second;

        // Merge cached args with override args
        for (const auto &pair : override_args) {
          cached_args[pair.first] = pair.second;
        }

        return replace_placeholders(cached_block, cached_args);
      }
    }

    // Not in cache, read and parse layout file
    std::ifstream file(layout_path);
    if (!file.is_open()) {
      std::cerr << "ERROR: Cannot open layout file: " << layout_path
                << std::endl;
      return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::istringstream stream(content);
    std::string line;
    std::ostringstream block_content;
    bool in_block = false;
    std::string end_tag = "@" + clean_tag + ";";
    std::map<std::string, std::string> layout_args;

    while (std::getline(stream, line)) {
      std::string trimmed = line;
      trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
      trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

      if (!in_block) {
        std::string tag_prefix = "@" + clean_tag;
        if (trimmed.substr(0, tag_prefix.length()) == tag_prefix) {
          in_block = true;

          size_t paren_start = trimmed.find('(');
          size_t paren_end = trimmed.find(')', paren_start);
          if (paren_start != std::string::npos &&
              paren_end != std::string::npos) {
            std::string args_str =
                trimmed.substr(paren_start, paren_end - paren_start + 1);
            layout_args = parse_arguments(args_str);
          }

          size_t content_start;
          if (paren_end != std::string::npos) {
            content_start = trimmed.find_first_not_of(" \t", paren_end + 1);
          } else {
            content_start =
                trimmed.find_first_not_of(" \t", tag_prefix.length());
          }

          if (content_start != std::string::npos &&
              content_start < trimmed.length()) {
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

    std::string block_str = block_content.str();
    if (!block_str.empty() && block_str.back() == '\n') {
      block_str.pop_back();
    }

    // Cache the block (without replacing placeholders)
    {
      std::lock_guard<std::mutex> lock(cache_mutex);
      layout_cache[clean_tag] = {block_str, layout_args};
    }

    // Merge layout args with override args and replace placeholders
    for (const auto &pair : override_args) {
      layout_args[pair.first] = pair.second;
    }
    return replace_placeholders(block_str, layout_args);
  }

  std::string copy_block(const std::string &layout_path,
                         const std::string &tag) {
    return copy_block_with_args(layout_path, tag);
  }

  void insert_block(const std::string &child_path, const std::string &tag) {
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

    for (const auto &tag_info : tag_infos) {
      if (tag_info.name != tag)
        continue;

      std::string processed_block =
          copy_block_with_args("public/layout.html", tag, tag_info.arguments);
      if (processed_block.empty()) {
        std::cerr << "ERROR: No layout block found for tag: " << tag
                  << std::endl;
        continue;
      }

      size_t start_pos = content.find(tag_info.full_match);
      if (start_pos == std::string::npos)
        continue;

      size_t start_line_end = content.find('\n', start_pos);
      if (start_line_end == std::string::npos) {
        start_line_end = content.length();
      } else {
        start_line_end++;
      }

      std::string end_tag = "@" + tag + ";";
      size_t end_tag_pos = content.find(end_tag, start_line_end);

      if (end_tag_pos == std::string::npos) {
        std::string comment_end_tag = "<!-- @" + tag + "; -->";
        end_tag_pos = content.find(comment_end_tag, start_line_end);
        if (end_tag_pos == std::string::npos) {
          continue;
        }
      }

      size_t end_line_end = content.find('\n', end_tag_pos);
      if (end_line_end == std::string::npos) {
        end_line_end = content.length();
      } else {
        end_line_end++;
      }

      content.replace(start_line_end, end_tag_pos - start_line_end,
                      processed_block + "\n");
      modified = true;
    }

    if (modified) {
      std::ofstream out(child_path);
      if (!out.is_open()) {
        std::cerr << "ERROR: Cannot write to child file: " << child_path
                  << std::endl;
        return;
      }
      out << content;
      out.close();
    }
  }

  std::string erase_block(const std::string &filepath, const std::string &tag) {
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
      trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
      trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

      if (trimmed == tag || trimmed == "@end") {
        continue;
      }

      output << line << '\n';
    }

    return output.str();
  }

  std::string commentify_tags(const std::string &content,
                              const std::vector<std::string> &tags) {
    std::istringstream stream(content);
    std::ostringstream output;
    std::string line;

    bool inside_block = false;
    std::string current_tag;

    while (std::getline(stream, line)) {
      std::string trimmed = line;
      trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
      trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);

      if (!inside_block) {
        bool found_tag = false;
        if (trimmed.length() > 1 && trimmed[0] == '@') {
          size_t tag_start = 1;
          size_t tag_end = tag_start;
          while (tag_end < trimmed.length() &&
                 (std::isalnum(trimmed[tag_end]) || trimmed[tag_end] == '_')) {
            tag_end++;
          }
          if (tag_end > tag_start) {
            std::string tag_name =
                trimmed.substr(tag_start, tag_end - tag_start);
            for (const auto &tag : tags) {
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

  void home(const std::string &view_path, struct mg_connection *connection) {
    const std::string layout_path = "public/layout.html";
    const std::string child_path = view_path;

    // Insert blocks based on tags found in the view
    std::ifstream file(child_path);
    if (!file.is_open()) {
      const char *msg = "500 Internal Server Error";
      mg_printf(connection,
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %zu\r\n\r\n%s",
                strlen(msg), msg);
      return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string html_content = buffer.str();
    file.close();

    // Find all tags (e.g., @body) and insert layout blocks
    std::vector<std::string> tags = extract_tags_from_content(html_content);
    for (const auto &tag : tags) {
      insert_block(child_path, tag);
    }

    // Read the modified file again
    std::ifstream updated_file(child_path);
    if (!updated_file.is_open()) {
      const char *msg = "500 Internal Server Error";
      mg_printf(connection,
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %zu\r\n\r\n%s",
                strlen(msg), msg);
      return;
    }

    std::stringstream updated_buffer;
    updated_buffer << updated_file.rdbuf();
    std::string final_html = updated_buffer.str();
    updated_file.close();

    // Replace layout tags with HTML comments for SSR cleanliness
    final_html = commentify_tags(final_html, tags);

    mg_printf(connection,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html\r\n"
              "Content-Length: %zu\r\n\r\n%s",
              final_html.length(), final_html.c_str());
  }
  // Inside struct Pnix
  void SSR(const std::string &view_path, struct mg_connection *connection) {
    const std::string layout_path = "public/layout.html";
    const std::string child_path = view_path;

    std::ifstream file(child_path);
    if (!file.is_open()) {
      const char *msg = "500 Internal Server Error";
      mg_printf(connection,
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %zu\r\n\r\n%s",
                strlen(msg), msg);
      return;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    std::string html = buf.str();
    file.close();

    std::vector<std::string> tags = extract_tags_from_content(html);
    for (const auto &tag : tags) {
      insert_block(child_path, tag);
    }

    std::ifstream file2(child_path);
    if (!file2.is_open()) {
      const char *msg = "500 Internal Server Error";
      mg_printf(connection,
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %zu\r\n\r\n%s",
                strlen(msg), msg);
      return;
    }

    std::stringstream buf2;
    buf2 << file2.rdbuf();
    std::string final_html = buf2.str();
    file2.close();

    final_html = commentify_tags(final_html, tags);

    mg_printf(connection,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html\r\n"
              "Content-Length: %zu\r\n\r\n%s",
              final_html.length(), final_html.c_str());
  }
};
