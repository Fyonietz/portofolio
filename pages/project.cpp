#include "../models/project_model.hpp"
#include "civetweb.h"
#include "handler.hpp"
#include <bits/types/cookie_io_functions_t.h>
#include <cstdlib>
#include <stack>
#include <string>
#define OK(conn) Server.Response(conn, 200, "Ok", R"({"message":"success"})")

using namespace nlohmann;
const std::string DATABASE = "data.db";

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Struct to hold one part
struct MultipartPart {
  std::unordered_map<std::string, std::string> headers;
  std::string name;       // form field name
  std::string filename;   // filename for file field
  std::vector<char> data; // binary content (or string for text fields)
};

std::string trim(const std::string &str) {
  const char *whitespace = " \t\r\n";
  size_t start = str.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return "";

  size_t end = str.find_last_not_of(whitespace);
  return str.substr(start, end - start + 1);
}

// Parse Content-Disposition header, extract name and filename
void parse_content_disposition(const std::string &line, std::string &name,
                               std::string &filename) {
  // example line: Content-Disposition: form-data; name="file";
  // filename="photo.jpg"
  size_t pos = line.find(':');
  if (pos == std::string::npos)
    return;
  std::string value = line.substr(pos + 1);
  value = trim(value);

  // parse tokens separated by ;
  std::istringstream iss(value);
  std::string token;
  while (std::getline(iss, token, ';')) {
    token = trim(token);
    if (token.find("name=") == 0) {
      size_t start = token.find('"');
      size_t end = token.find_last_of('"');
      if (start != std::string::npos && end != std::string::npos &&
          end > start) {
        name = token.substr(start + 1, end - start - 1);
      }
    } else if (token.find("filename=") == 0) {
      size_t start = token.find('"');
      size_t end = token.find_last_of('"');
      if (start != std::string::npos && end != std::string::npos &&
          end > start) {
        filename = token.substr(start + 1, end - start - 1);
      }
    }
  }
}

// Parse the multipart body into parts
std::vector<MultipartPart> parse_multipart(const std::string &body,
                                           const std::string &boundary) {
  std::vector<MultipartPart> parts;

  std::string delimiter = "--" + boundary;
  std::string close_delim = delimiter + "--";

  size_t pos = 0;
  while (true) {
    // find start of part
    pos = body.find(delimiter, pos);
    if (pos == std::string::npos)
      break;
    pos += delimiter.size();

    // skip CRLF
    if (body.compare(pos, 2, "\r\n") == 0)
      pos += 2;

    // find next delimiter
    size_t next_pos = body.find(delimiter, pos);
    if (next_pos == std::string::npos)
      break;

    // extract part content between pos and next_pos
    std::string part = body.substr(pos, next_pos - pos);

    MultipartPart mp;

    // Split headers and data
    size_t header_end = part.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      pos = next_pos;
      continue; // malformed
    }

    std::string header_block = part.substr(0, header_end);
    std::string data_block = part.substr(header_end + 4); // after \r\n\r\n

    // Parse headers line by line
    std::istringstream header_stream(header_block);
    std::string line;
    while (std::getline(header_stream, line)) {
      line = trim(line);
      if (line.empty())
        continue;

      size_t colon = line.find(':');
      if (colon == std::string::npos)
        continue;
      std::string header_name = line.substr(0, colon);
      std::string header_value = line.substr(colon + 1);
      header_value = trim(header_value);
      mp.headers[header_name] = header_value;

      // Parse Content-Disposition special
      if (header_name == "Content-Disposition") {
        parse_content_disposition(line, mp.name, mp.filename);
      }
    }

    // Copy data_block into mp.data
    mp.data.assign(data_block.begin(), data_block.end());

    parts.push_back(mp);

    pos = next_pos;
  }

  return parts;
}

route("/admin/project", project) {
  Server.SSR("public/admin/project.htpp", connection);
  return 200;
}

route("/admin/project/lists", project_lists) {
  Sqlite3 db;
  if (!Sqlite_Open()) {
    return Server.Response(connection, 500, "Internal Server Error",
                           R"({"error":"Failed to open DB"})");
  }

  try {
    auto result = sqlite.SELECT("*").FROM("project").JSON();

    Sqlite_Close();
    return Server.Response(connection, 200, "Ok", result.dump(4));

  } catch (const std::exception &e) {

    Sqlite_Close();
    return Server.Response(connection, 500, "Error",
                           std::string("{\"error\":\"") + e.what() + "\"}");
  }
}

route("/admin/project/create", project_create) {
  sqlite3 *db;
  json data_as_json = json::parse(Server.Read(connection));
  Model<Project> project_mapper;
  project_mapper.bind("title", &Project::name)
      .bind("description", &Project::desc)
      .bind("link", &Project::link);

  auto project_object = project_mapper.parse_one(data_as_json);

  if (!Sqlite_Open()) {
    return Server.Response(connection, 500, "Internal Server Error",
                           R"({"error":"Failed to open DB"})");
  }

  sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite
      .INSERT("project", "(title,desc,link)",
              "(" + Escape(project_object.name) + "," +
                  Escape(project_object.desc) + "," +
                  Escape(project_object.link) + ")")
      .execute();

  Sqlite_Close();
  return Server.Response(connection, 200, "Ok", R"({"message":"Success"})");
}

route("/admin/project/delete", project_delete) {
  json post_data = json::parse(Server.Read(connection));
  Model<Project> pr;
  pr.bind("id", &Project::id);
  auto pr_obj = pr.parse_one(post_data);

  if (!Sqlite_Open()) {
    return Server.Response(connection, 500, "Internal Server Error",
                           R"({"error":"Failed to open DB"})");
  }

  try {

    sqlite.DELETE("project").WHERE("id=" + pr_obj.id).execute();
    Sqlite_Close();
    return Server.Response(connection, 200, "Ok", R"({"message":"Success"})");

  } catch (const std::exception &e) {
    Sqlite_Close();
    return Server.Response(connection, 500, "Error",
                           std::string("{\"error\":\"") + e.what() + "\"}");
  }
}

std::unordered_map<std::string, std::string>
loadPaths(const std::string &filename) {
  std::unordered_map<std::string, std::string> config;
  std::ifstream file(filename);

  if (!file) {
    std::cerr << "Failed to Open Config File:" << filename << std::endl;
    return config;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    std::istringstream iss(line);
    std::string key, value;
    if (std::getline(iss, key, '=') && std::getline(iss, value)) {
      config[key] = value;
    }
  }
  return config;
};

std::string public_folder() {
  std::string result;
  std::array<char, 128> buffer;

  // Run the tree command in the 'public' folder
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("cd public && tree", "r"),
                                                pclose);

  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }

  // Read the output into the result string
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  return result;
}

std::string get_tree_json_output() {
  std::string result;
  std::array<char, 256> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("tree -J public", "r"),
                                                pclose);

  if (!pipe)
    throw std::runtime_error("popen() failed!");

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }

  return result;
}
route("/api/folder/lists", temp_path) {
  std::string folder = get_tree_json_output();

  Server.Response(connection, 200, "Path Lists", folder);
  return Server.Response(connection, 200, "Ok", R"({"message":"success"})");
}

route("/admin/project/media", media) {
  Server.SSR("public/admin/media.htpp", connection);
  return OK(connection);
}

route("/api/media/add", media_add) {
  const char *content_type_cstr = mg_get_header(connection, "Content-Type");
  if (!content_type_cstr) {
    return Server.Response(connection, 400, "Bad Request",
                           R"({"message":"Missing Content-Type"})");
  }

  std::string content_type = content_type_cstr;
  std::string boundary;

  size_t bpos = content_type.find("boundary=");
  if (bpos == std::string::npos) {
    return Server.Response(connection, 400, "Bad Request",
                           R"({"message":"No boundary in Content-Type"})");
  }
  boundary = content_type.substr(bpos + 9); // extract boundary

  // 2. Get Content-Length
  const char *content_length_str = mg_get_header(connection, "Content-Length");
  if (!content_length_str) {
    return Server.Response(connection, 411, "Length Required",
                           R"({"message":"Missing Content-Length"})");
  }

  size_t content_length = std::stoul(content_length_str);
  std::vector<char> body(content_length);

  size_t total_read = 0;
  while (total_read < content_length) {
    int r = mg_read(connection, body.data() + total_read,
                    content_length - total_read);
    if (r <= 0) {
      return Server.Response(connection, 400, "Bad Request",
                             R"({"message":"Incomplete request body"})");
    }
    total_read += r;
  }

  // Convert to string for parsing
  std::string body_str(body.begin(), body.end());

  // 3. Parse multipart
  auto parts = parse_multipart(body_str, boundary);

  // 4. Extract needed parts
  std::string folder_path;
  std::string filename;
  std::vector<char> file_data;

  for (const auto &p : parts) {
    if (p.name == "folder") {
      folder_path = trim(std::string(p.data.begin(), p.data.end()));
    } else if (p.name == "file") {
      file_data = p.data;
      if (!p.filename.empty()) {
        filename = trim(std::filesystem::path(p.filename).filename().string());
      }
    }
  }
  // Validate inputs
  if (folder_path.empty() || filename.empty() || file_data.empty()) {
    return Server.Response(
        connection, 400, "Bad Request",
        R"({"message":"Missing file or folder or filename"})");
  }

  // Sanitize filename
  filename = std::filesystem::path(filename).filename().string();

  // Base upload directory
  std::string base_dir = "public/"; // adjust to your actual base path
  std::string full_folder_path = base_dir + "/" + folder_path;

  try {
    // Ensure directory exists
    std::filesystem::create_directories(full_folder_path);

    // Save the file
    std::string full_path = full_folder_path + "/" + filename;
    std::ofstream out(full_path, std::ios::binary);
    if (!out.is_open()) {
      return Server.Response(
          connection, 500, "Server Error",
          R"({"message":"Failed to open file for writing"})");
    }
    out.write(file_data.data(), file_data.size());
    out.close();
  } catch (const std::exception &ex) {
    return Server.Response(connection, 500, "Server Error",
                           R"({"message":"Exception while saving file"})");
  }

  return Server.Response(connection, 200, "Ok",
                         R"({"message":"Upload successful"})");
}

//
// route("/project", dynamic_project) {
//   const struct mg_request_info* req_info = mg_get_request_info(connection);
//   std::string uri = req_info->request_uri;  // e.g. "/project/A"
//
//   // Check if the route is something like /project/X
//   if (uri.find("/project/") == 0) {
//     std::string subRoute = uri.substr(std::string("/project/").length()); //
//     gets "A"
//
//     auto paths = loadPaths("pages/paths.txt");  // e.g.
//     A=public/projects/a.html
//
//     auto it = paths.find(subRoute);
//     if (it != paths.end()) {
//       std::string pagePath = it->second;
//
//       // Optionally validate file exists
//       if (!std::filesystem::exists(pagePath)) {
//           return Server.Response(connection, 404, "Not Found",
//           R"({"error":"Page not found"})");
//       }
//
//       Server.SSR(pagePath, connection);  // Render page
//     } else {
//       return Server.Response(connection, 404, "Not Found",
//       R"({"error":"Unknown project"})");
//     }
//   }
//
//   // Fallback for /project itself
//   return Server.Response(connection, 200, "Ok", R"({"message":"Base Project
//   Endpoint"})");
// }
